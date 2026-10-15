#include "application/usecases/GenerateObjectsFromSample.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include "domain/image_depth.h"
#include "domain/lattice/lattice_accumulator.h"
#include "domain/lattice/lattice_predictor.h"

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
    // Gray16 依有效位元數換到 0..255（12 bit 的影像值只到 4095）：minContrast 的單位不變。
    const double gray16Scale = 255.0 / normalizedMax(effectiveBits(image));
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
                case PixelFormat::Bgr24: {
                    const std::uint8_t* p = row + 3 * x;
                    luminance = (77.0 * p[2] + 150.0 * p[1] + 29.0 * p[0]) / 256.0;
                    break;
                }
                case PixelFormat::Gray16: {
                    std::uint16_t v = 0;
                    std::memcpy(&v, row + 2 * x, sizeof v);  // 列的起點不保證 2 byte 對齊
                    luminance = v * gray16Scale;
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

// 一個預測的缺格：哪一組晶格的哪一格、在哪裡。
struct Hole {
    Point2D position;
    std::size_t lattice = 0;
    LatticeIndex cell;
};

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

    // 比對區（ADR-026）：範本只用它的像素，所以對比與最小邊長量它。矩形裁到影像內；遮罩的外接矩形由 adapter 裁。
    // 沒有比對區時 matchArea ＝ sample（和之前相同）。
    MatchSpec match = request.match;
    Rect matchArea = sample;
    if (match.rect) {
        if (!finite(*match.rect)) return makeError(ErrorCode::InvalidArgument, "The match region is not finite.");
        matchArea = intersection(*match.rect, imageRect);
        if (matchArea.empty()) return stopWith(NoMatchReason::SampleOffPart);
        if (std::min(matchArea.w, matchArea.h) < options.minSamplePx) return stopWith(NoMatchReason::SampleTooSmall);
        if (match.mask.empty()) match.rect = matchArea;
    }
    if (luminanceStdDev(request.image, matchArea) < options.minContrast) return stopWith(NoMatchReason::LowContrast);

    // ---- 2. 建範本 ----
    const Clock::time_point templateStart = Clock::now();
    Result<TemplatePtr> built = [&] {
        ScopedTimer timer(probe_, "uc.sample.template");
        return builder_.buildFromSample(request.image, SampleSpec{sample, std::move(match)}, request.search, cancel);
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

    // 同一個元件被找到好幾次（位置差幾個像素）時，只留分數最高的（NMS，主體框大小）。搜尋器的契約沒有保證匹配不重疊，
    // 真的 back end 也常這樣。留下的順序（分數由高到低、再依 y、x）與搜尋器的排序規則相同。
    std::vector<Match> seedMatches;
    {
        std::vector<ScoredBox> boxes;
        boxes.reserve(seeds.value().size());
        for (const Match& m : seeds.value()) boxes.push_back({RectObj{m.pose, proposal.partSize.w, proposal.partSize.h}, m.score});
        const std::vector<std::size_t> kept = NonMaxSuppression(options.nms).keep(boxes);
        seedMatches.reserve(kept.size());
        for (const std::size_t k : kept) seedMatches.push_back(seeds.value()[k]);
    }

    // 樣本本身：落在樣本框內、分數最高的那個匹配（自我匹配）。它的位姿代表「使用者框的那個元件」。
    // 搜尋器沒有回報自我匹配時，樣本位置就是框的中心、分數 1.0（範本就是從它建的，依定義完全相符）。
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

    // ---- 4. 擬合晶格（domain/lattice，可能多組）與預測缺格。沒有晶格（點太少、不成陣列、信心不足）就不補漏。 ----
    const Clock::time_point fitStart = Clock::now();
    const double sampleAngle = proposal.candidates.front().pose.angleDeg;
    const auto partBox = [&](Point2D center) {
        return RectObj{Pose2D{center.x, center.y, sampleAngle}, proposal.partSize.w, proposal.partSize.h};
    };
    std::vector<LatticeModel> lattices;
    std::vector<Hole> holes;
    {
        ScopedTimer timer(probe_, "uc.sample.fit");
        std::vector<Point2D> centers;
        centers.reserve(proposal.candidates.size());
        for (const Candidate& c : proposal.candidates) centers.push_back(c.pose.position());
        LatticeFit fit = LatticeEstimator(options.lattice).fitAll(centers);
        lattices = std::move(fit.models);
        proposal.grid.noLattice = fit.reason;

        // 缺格：每組晶格索引範圍內、影像內的空格。與已有的候選（或前面那組晶格的缺格）重疊的不算：
        // 那裡已經有元件（例如一個殘差稍大、沒被算成內點的種子），或是兩組晶格預測到同一個位置。
        const double cellSize = std::sqrt(proposal.partSize.w * proposal.partSize.w + proposal.partSize.h * proposal.partSize.h);
        OverlapIndex taken(cellSize, options.nms.maxOverlap, options.nms.measure);
        for (const Candidate& c : proposal.candidates) taken.add(RectObj{c.pose, proposal.partSize.w, proposal.partSize.h});
        for (std::size_t l = 0; l < lattices.size(); ++l) {
            for (const PredictedCell& p : LatticePredictor(lattices[l], imageRect).holes()) {
                const RectObj box = partBox(p.position);
                if (taken.overlapsAny(box)) continue;
                taken.add(box);
                holes.push_back({p.position, l, p.cell});
            }
        }
        // 由近到遠（離樣本）：先驗證、先顯示離已知點近的；超過上限時留近的。同距離依晶格、再依格子，結果與走訪順序無關。
        const Point2D anchor = proposal.candidates.front().pose.position();
        std::vector<double> d2(holes.size());
        for (std::size_t k = 0; k < holes.size(); ++k) {
            const double dx = holes[k].position.x - anchor.x;
            const double dy = holes[k].position.y - anchor.y;
            d2[k] = dx * dx + dy * dy;
        }
        std::vector<std::size_t> order(holes.size());
        for (std::size_t k = 0; k < order.size(); ++k) order[k] = k;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            if (d2[a] != d2[b]) return d2[a] < d2[b];
            if (holes[a].lattice != holes[b].lattice) return holes[a].lattice < holes[b].lattice;
            if (holes[a].cell.i != holes[b].cell.i) return holes[a].cell.i < holes[b].cell.i;
            return holes[a].cell.j < holes[b].cell.j;
        });
        std::vector<Hole> sorted;
        sorted.reserve(std::min(order.size(), options.maxPredictions));
        for (const std::size_t k : order) {
            if (sorted.size() >= options.maxPredictions) break;
            sorted.push_back(holes[k]);
        }
        holes = std::move(sorted);

        if (!lattices.empty()) {
            const LatticeModel& main = lattices.front();
            proposal.grid.found = true;
            proposal.grid.hasV = main.hasV();
            proposal.grid.u = main.basis.u;
            proposal.grid.v = main.basis.v;
            proposal.grid.rmsPx = main.rmsPx;
            proposal.grid.coverage = main.coverage;
            proposal.grid.confidence = main.confidence;
            proposal.grid.suggestCheck = main.suggestCheck;
            proposal.grid.lattices = lattices.size();
        }
        proposal.grid.holes = holes.size();
    }
    proposal.timings.fitMs = millisecondsSince(fitStart);
    report(progress, Stage::Fit, 1.0);
    if (cancel.cancelled()) return cancelledError();

    // ---- 5. 視窗驗證：在每個缺格附近的小視窗裡，以較低的分數下限再找一次 ----
    const Clock::time_point verifyStart = Clock::now();
    if (!holes.empty()) {
        std::vector<Rect> windows;
        windows.reserve(holes.size());
        for (const Hole& hole : holes) {
            const Rect w = rectFromCenter(hole.position, {proposal.partSize.w * options.windowScale, proposal.partSize.h * options.windowScale});
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
                const double d = distance(m.pose.position(), holes[k].position);
                if (d < nearestDistance) {
                    nearestDistance = d;
                    nearest = k;
                }
            }
            if (nearest == holes.size()) continue;
            if (best[nearest] == nullptr || m.score > best[nearest]->score) best[nearest] = &m;
        }

        // 驗證找到的元件加進各自的晶格重新擬合（正規方程累加器：每個 O(1)，S00 示範頁的 Refit while verifying），
        // 沒找到的缺格放在修正後的位置。離預測位置超過晶格容差的匹配不加（可能是視窗裡的另一個東西）。
        std::vector<std::optional<LatticeBasis>> refined(lattices.size());
        {
            std::vector<LatticeAccumulator> equations;
            equations.reserve(lattices.size());
            for (const LatticeModel& m : lattices) equations.push_back(m.equations);
            std::vector<bool> changed(lattices.size(), false);
            for (std::size_t k = 0; k < holes.size(); ++k) {
                if (best[k] == nullptr) continue;
                const LatticeModel& m = lattices[holes[k].lattice];
                if (distance(best[k]->pose.position(), holes[k].position) > m.tolerancePx) continue;
                equations[holes[k].lattice].add(holes[k].cell, best[k]->pose.position());
                changed[holes[k].lattice] = true;
            }
            for (std::size_t l = 0; l < lattices.size(); ++l) {
                if (changed[l]) refined[l] = equations[l].solve();
            }
        }
        for (std::size_t k = 0; k < holes.size(); ++k) {
            if (best[k] != nullptr) {
                proposal.candidates.push_back({best[k]->pose, best[k]->score, CandidateSource::Verified});
            } else {
                // 影像上沒有找到：只有預測的位置，沒有分數（Inferred）。角度沿用樣本的角度（同一個陣列的元件方向一致）。
                const std::optional<LatticeBasis>& basis = refined[holes[k].lattice];
                const Point2D at = basis ? basis->at(holes[k].cell) : holes[k].position;
                proposal.candidates.push_back({Pose2D{at.x, at.y, sampleAngle}, 0.0, CandidateSource::Predicted});
            }
        }
    }
    proposal.timings.verifyMs = millisecondsSince(verifyStart);
    report(progress, Stage::Verify, 1.0);

    proposal.timings.totalMs = millisecondsSince(started);
    return proposal;
}

}  // namespace aoi
