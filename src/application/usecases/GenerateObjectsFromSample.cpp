#include "application/usecases/GenerateObjectsFromSample.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>

#include "application/usecases/SimpleGrid.h"

namespace aoi {

namespace {

using Clock = std::chrono::steady_clock;

double millisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// 樣本範圍內亮度的標準差（0..255）。最多抽樣約 4096 個像素，所以成本與樣本大小無關。
double luminanceStdDev(const ImageView& image, const Rect& area) {
    const int x0 = std::max(0, static_cast<int>(std::floor(area.x)));
    const int y0 = std::max(0, static_cast<int>(std::floor(area.y)));
    const int x1 = std::min(image.width, static_cast<int>(std::ceil(area.right())));
    const int y1 = std::min(image.height, static_cast<int>(std::ceil(area.bottom())));
    const int w = x1 - x0;
    const int h = y1 - y0;
    if (w <= 0 || h <= 0) return 0.0;

    const int step = std::max(1, static_cast<int>(std::sqrt(static_cast<double>(w) * static_cast<double>(h) / 4096.0)));
    double sum = 0.0;
    double sumSquares = 0.0;
    std::size_t count = 0;
    for (int y = y0; y < y1; y += step) {
        const std::uint8_t* row = image.data + static_cast<std::size_t>(y) * static_cast<std::size_t>(image.strideBytes);
        for (int x = x0; x < x1; x += step) {
            double luminance = 0.0;
            switch (image.format) {
                case PixelFormat::Gray8:
                    luminance = row[x];
                    break;
                case PixelFormat::Rgb24: {
                    const std::uint8_t* p = row + 3 * x;
                    luminance = (77.0 * p[0] + 150.0 * p[1] + 29.0 * p[2]) / 256.0;
                    break;
                }
                case PixelFormat::Bgrx32: {
                    const std::uint8_t* p = row + 4 * x;
                    luminance = (77.0 * p[2] + 150.0 * p[1] + 29.0 * p[0]) / 256.0;
                    break;
                }
            }
            sum += luminance;
            sumSquares += luminance * luminance;
            ++count;
        }
    }
    if (count == 0) return 0.0;
    const double mean = sum / static_cast<double>(count);
    return std::sqrt(std::max(0.0, sumSquares / static_cast<double>(count) - mean * mean));
}

bool finite(const Rect& r) {
    return std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w) && std::isfinite(r.h);
}

void report(IProgressSink* progress, Stage stage, double fraction) {
    if (progress != nullptr) progress->report({stage, fraction});
}

Error cancelledError() { return makeError(ErrorCode::Cancelled, "The job was cancelled."); }

}  // namespace

GenerateObjectsFromSample::GenerateObjectsFromSample(ITemplateBuilder& builder, IPatternSearcher& searcher,
                                                     IPerfProbe* probe)
    : builder_(builder), searcher_(searcher), probe_(probe) {}

Result<SampleProposal> GenerateObjectsFromSample::run(const SampleRequest& request, const CancelToken& cancel,
                                                      IProgressSink* progress) const {
    const Clock::time_point started = Clock::now();
    ScopedTimer totalTimer(probe_, "uc.sample.total");
    const GenerateOptions& options = request.options;

    // ---- 1. 檢查輸入。這些都是便宜的、不需要視覺函式庫就能判斷的事。 ----
    if (!request.image.valid()) return makeError(ErrorCode::InvalidArgument, "The image is not valid.");
    if (!finite(request.sample)) return makeError(ErrorCode::InvalidArgument, "The sample rectangle is not finite.");
    if (cancel.cancelled()) return cancelledError();

    const Rect imageRect{0.0, 0.0, static_cast<double>(request.image.width), static_cast<double>(request.image.height)};
    const Rect sample = intersection(request.sample, imageRect);

    SampleProposal proposal;
    const auto stopWith = [&](NoMatchReason reason) -> Result<SampleProposal> {
        proposal.sampleProblem = NoMatchInfo{reason, 0.0, 0.0, 0};
        proposal.timings.totalMs = millisecondsSince(started);
        return proposal;
    };

    if (request.sample.empty()) return stopWith(NoMatchReason::SampleTooSmall);
    if (sample.empty()) return stopWith(NoMatchReason::SampleOffPart);
    if (std::min(sample.w, sample.h) < options.minSamplePx) return stopWith(NoMatchReason::SampleTooSmall);
    if (sample.area() > options.maxSampleAreaFraction * imageRect.area()) return stopWith(NoMatchReason::SampleTooLarge);
    if (luminanceStdDev(request.image, sample) < options.minContrast) return stopWith(NoMatchReason::LowContrast);

    // ---- 2. 建範本 ----
    const Clock::time_point templateStart = Clock::now();
    Result<TemplatePtr> built = [&] {
        ScopedTimer timer(probe_, "uc.sample.template");
        return builder_.build(request.image, sample, request.search, cancel);
    }();
    proposal.timings.templateMs = millisecondsSince(templateStart);
    if (!built) return built.error();
    const TemplatePtr tmpl = built.value();
    if (!tmpl) return makeError(ErrorCode::Internal, "The template builder returned no template.");
    proposal.partSize = tmpl->size();
    report(progress, Stage::Template, 1.0);
    if (const std::optional<NoMatchReason> problem = tmpl->sampleProblem()) return stopWith(*problem);
    if (cancel.cancelled()) return cancelledError();

    // ---- 3. 種子搜尋：整張影像、較高的分數下限 ----
    const Clock::time_point seedStart = Clock::now();
    SearchParams seedParams = request.search;
    seedParams.minScore = options.seedMinScore;
    Result<std::vector<Match>> seeds = [&] {
        ScopedTimer timer(probe_, "uc.sample.seed");
        return searcher_.find(request.image, *tmpl, SearchRegion::wholeImage(), seedParams, cancel);
    }();
    proposal.timings.seedMs = millisecondsSince(seedStart);
    if (!seeds) return seeds.error();
    report(progress, Stage::Seed, 1.0);
    if (cancel.cancelled()) return cancelledError();

    // 樣本本身：落在樣本框內、分數最高的那個匹配（自我匹配）。它的位姿代表「使用者框的那個元件」。
    // 搜尋器沒有回報自我匹配時，樣本位置就是框的中心、分數 1.0（範本就是從它建的，依定義完全相符）。
    const std::vector<Match>& seedMatches = seeds.value();
    const Match* selfMatch = nullptr;
    for (const Match& m : seedMatches) {
        if (sample.contains(m.pose.position()) && (selfMatch == nullptr || m.score > selfMatch->score)) selfMatch = &m;
    }
    {
        Candidate sampleCandidate;
        sampleCandidate.source = CandidateSource::Sample;
        sampleCandidate.pose = selfMatch != nullptr ? selfMatch->pose : Pose2D{sample.center().x, sample.center().y, 0.0};
        sampleCandidate.score = selfMatch != nullptr ? selfMatch->score : 1.0;
        proposal.candidates.push_back(sampleCandidate);
    }
    for (const Match& m : seedMatches) {
        if (sample.contains(m.pose.position())) continue;  // 樣本框內的匹配都是樣本自己
        proposal.candidates.push_back({m.pose, m.score, CandidateSource::Seed});
    }

    // ---- 4. 擬合晶格（S03 暫代版）與預測缺格。至少要有 2 個點才有晶格。 ----
    const Clock::time_point fitStart = Clock::now();
    std::vector<Point2D> holes;
    {
        ScopedTimer timer(probe_, "uc.sample.fit");
        std::vector<Point2D> centers;
        centers.reserve(proposal.candidates.size());
        for (const Candidate& c : proposal.candidates) centers.push_back(c.pose.position());
        if (const std::optional<GridModel> grid = fitGrid(centers)) {
            holes = gridHoles(*grid, imageRect, options.maxPredictions);
            proposal.grid.found = true;
            proposal.grid.hasV = grid->hasV;
            proposal.grid.u = grid->u;
            proposal.grid.v = grid->v;
            proposal.grid.rmsPx = grid->rmsPx;
            proposal.grid.holes = holes.size();
        }
    }
    proposal.timings.fitMs = millisecondsSince(fitStart);
    report(progress, Stage::Fit, 1.0);
    if (cancel.cancelled()) return cancelledError();

    // ---- 5. 視窗驗證：在每個缺格附近的小視窗裡，以較低的分數下限再找一次 ----
    const Clock::time_point verifyStart = Clock::now();
    if (!holes.empty()) {
        std::vector<Rect> windows;
        windows.reserve(holes.size());
        for (const Point2D& hole : holes) {
            const Rect w = rectFromCenter(hole, {proposal.partSize.w * options.windowScale, proposal.partSize.h * options.windowScale});
            windows.push_back(intersection(w, imageRect));
        }
        SearchParams windowParams = request.search;
        windowParams.minScore = options.candidateFloor;
        Result<std::vector<Match>> verified = [&] {
            ScopedTimer timer(probe_, "uc.sample.verify");
            return searcher_.find(request.image, *tmpl, SearchRegion::inWindows(windows), windowParams, cancel);
        }();
        if (!verified) return verified.error();

        // 每個匹配歸給「離它最近、而且視窗包含它」的缺格；每個缺格只留分數最高的。
        std::vector<const Match*> best(holes.size(), nullptr);
        for (const Match& m : verified.value()) {
            std::size_t nearest = holes.size();
            double nearestDistance = std::numeric_limits<double>::max();
            for (std::size_t k = 0; k < holes.size(); ++k) {
                if (!windows[k].contains(m.pose.position())) continue;
                const double d = distance(m.pose.position(), holes[k]);
                if (d < nearestDistance) {
                    nearestDistance = d;
                    nearest = k;
                }
            }
            if (nearest == holes.size()) continue;
            if (best[nearest] == nullptr || m.score > best[nearest]->score) best[nearest] = &m;
        }
        for (std::size_t k = 0; k < holes.size(); ++k) {
            if (best[k] != nullptr) {
                proposal.candidates.push_back({best[k]->pose, best[k]->score, CandidateSource::Verified});
            } else {
                // 影像上沒有找到：只有預測的位置，沒有分數（Inferred）。角度沿用樣本的角度（同一個陣列的元件方向一致）。
                proposal.candidates.push_back(
                    {Pose2D{holes[k].x, holes[k].y, proposal.candidates.front().pose.angleDeg}, 0.0, CandidateSource::Predicted});
            }
        }
    }
    proposal.timings.verifyMs = millisecondsSince(verifyStart);
    report(progress, Stage::Verify, 1.0);

    proposal.timings.totalMs = millisecondsSince(started);
    return proposal;
}

}  // namespace aoi
