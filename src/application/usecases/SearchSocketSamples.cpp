#include "application/usecases/SearchSocketSamples.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include "domain/lattice/non_max_suppression.h"

namespace aoi {
Result<std::vector<SocketSampleMatch>> SearchSocketSamples::search(const SocketSampleRequest& request,
                                                                 const CancelToken& cancel) const {
    try {
        if (!builder_ || !searcher_) return makeError(ErrorCode::BackendUnavailable, "Sample search is unavailable.");
        if (!request.image.valid() || request.samples.empty() || request.samples.size() > 16 ||
            !std::isfinite(request.minScore) || request.minScore < 0.50 || request.minScore > 0.99)
            return makeError(ErrorCode::InvalidArgument, "Teach 1 to 16 samples and choose a similarity from 50% to 99%.");
        const auto& image = request.image;
        const Rect bounds{0, 0, static_cast<double>(image.width), static_cast<double>(image.height)};
        for (const auto& r : request.samples) {
            if (!std::isfinite(r.x) || !std::isfinite(r.y) || !std::isfinite(r.w) || !std::isfinite(r.h) ||
                r.w < 8 || r.h < 8 || r.x < 0 || r.y < 0 || r.right() > image.width || r.bottom() > image.height ||
                r.w * r.h > 0.25 * image.width * image.height)
                return makeError(ErrorCode::InvalidArgument, "Each sample must contain one joint, at least 8 px per side, inside the image.");
        }
        SearchParams params;
        params.minScore = request.minScore;
        params.angleMinDeg = params.angleMaxDeg = 0;
        params.maxMatches = 5000;
        const Size2D output{request.samples.front().w, request.samples.front().h};
        std::vector<ScoredBox> candidates;
        for (std::size_t i = 0; i < request.samples.size(); ++i) {
            if (cancel.cancelled()) return makeError(ErrorCode::Cancelled, "Sample search cancelled.");
            const Rect& sample = request.samples[i];
            // 少量周邊特徵改善純亮塊的歧義；輸出仍是使用者教的主體框。
            const double pad = std::max(2.0, std::round(std::min(sample.w, sample.h) * 0.15));
            SampleSpec spec{sample, {}};
            spec.match.rect = intersection(Rect{sample.x-pad, sample.y-pad, sample.w+2*pad, sample.h+2*pad}, bounds);
            auto built = builder_->buildFromSample(image, spec, params, cancel);
            if (!built) return built.error();
            if (!built.value() || built.value()->sampleProblem())
                return makeError(ErrorCode::InvalidArgument, "Sample " + std::to_string(i+1) +
                    " has insufficient contrast. Include the joint outline and a little background.");
            auto found = searcher_->find(image, *built.value(), SearchRegion::wholeImage(), params, cancel);
            if (!found) return found.error();
            if (found.value().size() >= params.maxMatches)
                return makeError(ErrorCode::InvalidArgument, "Too many matches. Raise the similarity threshold or improve the samples.");
            for (const auto& m : found.value()) {
                const Rect r{m.pose.x-output.w/2, m.pose.y-output.h/2, output.w, output.h};
                if (!std::isfinite(m.score) || m.score < params.minScore || m.score > 1 ||
                    !std::isfinite(r.x) || !std::isfinite(r.y) || r.x < 0 || r.y < 0 ||
                    r.right() > image.width || r.bottom() > image.height) continue;
                candidates.push_back({RectObj{{m.pose.x,m.pose.y,0},output.w,output.h},m.score});
            }
        }
        NmsOptions nms;
        nms.maxOverlap = 0.30;
        nms.measure = OverlapMeasure::IntersectionOverSmaller;
        const auto kept = NonMaxSuppression(nms).keep(candidates);
        if (kept.size() > 4900) return makeError(ErrorCode::InvalidArgument, "Too many matches. Raise the similarity threshold.");
        std::vector<SocketSampleMatch> result;
        result.reserve(kept.size());
        for (std::size_t index : kept) {
            if (cancel.cancelled()) return makeError(ErrorCode::Cancelled, "Sample search cancelled.");
            const auto& c = candidates[index];
            result.push_back({{c.rect.pose.x-output.w/2,c.rect.pose.y-output.h/2,output.w,output.h},c.score});
        }
        return result;
    } catch (const std::exception& e) {
        return makeError(ErrorCode::Internal, e.what());
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected sample search failure.");
    }
}
}  // namespace aoi
