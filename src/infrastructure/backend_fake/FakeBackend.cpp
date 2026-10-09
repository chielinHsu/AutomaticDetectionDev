#include "infrastructure/backend_fake/FakeBackend.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <thread>

namespace aoi::fake {

namespace {

Error cancelled() { return makeError(ErrorCode::Cancelled, "The fake back end was cancelled."); }

// 角度範圍要是有限值、而且 min ≤ max（S09 的契約測試：和 OpenCV、HALCON 一樣回 InvalidArgument，不是默默找不到）。
bool validAngleRange(const SearchParams& params) {
    return std::isfinite(params.angleMinDeg) && std::isfinite(params.angleMaxDeg) && params.angleMinDeg <= params.angleMaxDeg;
}

// 假裝花了 latencyMs 毫秒；每 1 ms 檢查一次取消，所以取消的延遲約 1 ms。
Result<void> simulateLatency(double latencyMs, const CancelToken& cancel) {
    if (latencyMs <= 0.0) return Result<void>::success();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double, std::milli>(latencyMs);
    while (std::chrono::steady_clock::now() < deadline) {
        if (cancel.cancelled()) return cancelled();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return Result<void>::success();
}

}  // namespace

Result<TemplatePtr> FakeTemplateBuilder::build(const ImageView& image, const Rect& sample, const SearchParams& params,
                                               const CancelToken& cancel) {
    try {
        if (!image.valid()) return makeError(ErrorCode::InvalidArgument, "The image is not valid.");
        if (!validAngleRange(params)) return makeError(ErrorCode::InvalidArgument, "The angle range is not valid.");
        const Rect imageRect{0.0, 0.0, static_cast<double>(image.width), static_cast<double>(image.height)};
        const Rect clamped = intersection(sample, imageRect);
        if (clamped.empty()) return makeError(ErrorCode::InvalidArgument, "The sample is outside the image.");
        if (cancel.cancelled()) return cancelled();

        std::vector<FakeCell> parts;
        for (const FakeCell& cell : makeFakeGrid(spec_, clamped, image.width, image.height)) {
            if (cell.kind != FakeCellKind::Missing) parts.push_back(cell);
        }
        return TemplatePtr(std::make_shared<const FakeTemplate>(clamped.size(), std::move(parts), spec_.forcedSampleProblem));
    } catch (const std::exception& e) {  // port 的契約：例外不穿過邊界
        return makeError(ErrorCode::Internal, e.what());
    }
}

Result<std::vector<Match>> FakeSearcher::find(const ImageView& image, const ITemplate& tmpl, const SearchRegion& region,
                                              const SearchParams& params, const CancelToken& cancel) {
    try {
        const auto* fakeTemplate = dynamic_cast<const FakeTemplate*>(&tmpl);
        if (fakeTemplate == nullptr || tmpl.backendId() != kBackendId) {
            return makeError(ErrorCode::BackendMismatch, "The template was not built by the fake back end.");
        }
        if (!image.valid()) return makeError(ErrorCode::InvalidArgument, "The image is not valid.");
        if (!(params.minScore >= 0.0 && params.minScore <= 1.0)) {
            return makeError(ErrorCode::InvalidArgument, "minScore must be between 0 and 1.");
        }
        if (!validAngleRange(params)) return makeError(ErrorCode::InvalidArgument, "The angle range is not valid.");
        const auto& parts = fakeTemplate->parts();

        if (Result<void> waited = simulateLatency(spec_.latencyMs, cancel); !waited) return waited.error();

        const Rect imageRect{0.0, 0.0, static_cast<double>(image.width), static_cast<double>(image.height)};
        std::vector<Match> matches;
        std::size_t visited = 0;
        for (const FakeCell& part : parts) {
            if ((++visited & 0xFF) == 0 && cancel.cancelled()) return cancelled();
            if (part.score < params.minScore) continue;
            if (part.pose.angleDeg < params.angleMinDeg || part.pose.angleDeg > params.angleMaxDeg) continue;
            const Point2D p = part.pose.position();
            bool inside = false;
            if (region.kind == SearchRegion::Kind::WholeImage) {
                inside = imageRect.contains(p);
            } else {
                inside = std::any_of(region.windows.begin(), region.windows.end(), [&](const Rect& w) { return w.contains(p); });
            }
            if (inside) matches.push_back({part.pose, part.score});
        }
        if (cancel.cancelled()) return cancelled();

        // 契約：分數由高到低；同分時依 y、再依 x。
        std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
            if (a.score != b.score) return a.score > b.score;
            if (a.pose.y != b.pose.y) return a.pose.y < b.pose.y;
            return a.pose.x < b.pose.x;
        });
        if (matches.size() > params.maxMatches) matches.resize(params.maxMatches);
        return matches;
    } catch (const std::exception& e) {
        return makeError(ErrorCode::Internal, e.what());
    }
}

Backend makeFakeBackend(const FakeSpec& spec) {
    Backend backend;
    backend.id = std::string(kBackendId);
    backend.displayName = "Fake (synthetic grid)";
    backend.templateBuilder = std::make_unique<FakeTemplateBuilder>(spec);
    backend.searcher = std::make_unique<FakeSearcher>(spec);
    return backend;
}

}  // namespace aoi::fake
