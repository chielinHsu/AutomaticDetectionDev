#include "infrastructure/backend_opencv/OpenCvPatternSearcher.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <new>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <utility>

#include "domain/lattice/non_max_suppression.h"
#include "infrastructure/backend_opencv/OpenCvCorrelation.h"
#include "infrastructure/backend_opencv/OpenCvImages.h"
#include "infrastructure/backend_opencv/OpenCvPyramid.h"

namespace aoi::infra {
namespace {

using Clock = std::chrono::steady_clock;

double millisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

Error cancelledError() { return makeError(ErrorCode::Cancelled, "The OpenCV search was cancelled."); }

// 一塊要掃的區域：roi 是要轉換、要掃的影像像素；accept 是匹配的中心要落在的地方（連續座標）。
struct Area {
    cv::Rect roi;
    Rect accept;
    std::size_t maxCandidates = 0;
};

struct Candidate {
    double x = 0.0;  // 第 0 層的影像連續座標（ADR-010）
    double y = 0.0;
    double angleDeg = 0.0;
    double score = 0.0;
    bool alive = true;
};

// 要的角度範圍。整圈時每個角度都要；否則以範圍中心為準（整圈的範本，角度會頭尾相接）。
struct AngleWindow {
    double centerDeg = 0.0;
    double halfRangeDeg = 0.0;
    bool all = false;
};

bool angleWanted(const OpenCvTemplate& t, const AngleWindow& w, double a, double tolerance) {
    return w.all || std::abs(t.angleDifference(a, w.centerDeg)) <= w.halfRangeDeg + tolerance;
}

// 這一層要比的角度：範圍內的格點，加上範圍外半步以內的（元件剛好在範圍邊上時，才找得到；最後的角度會夾回範圍內）。
std::vector<int> wantedAngles(const OpenCvTemplate& t, const OpenCvTemplateLevel& level, const AngleWindow& w) {
    std::vector<int> out;
    const double tolerance = level.angleStepDeg * 0.5 + 1e-9;
    for (int k = 0; k < static_cast<int>(level.anglesDeg.size()); ++k) {
        if (angleWanted(t, w, level.anglesDeg[static_cast<std::size_t>(k)], tolerance)) out.push_back(k);
    }
    if (out.empty()) {
        int nearest = 0;
        double best = 1e300;
        for (int k = 0; k < static_cast<int>(level.anglesDeg.size()); ++k) {
            const double d = std::abs(t.angleDifference(level.anglesDeg[static_cast<std::size_t>(k)], w.centerDeg));
            if (d < best) {
                best = d;
                nearest = k;
            }
        }
        out.push_back(nearest);
    }
    return out;
}

// 等間隔三點（−1、0、+1）的拋物線頂點，相對中間那一點、單位是取樣間隔；開口不向下（沒有峰）時回 0。
double parabolaVertex(double a, double b, double c) {
    const double denominator = a - 2.0 * b + c;
    if (!(denominator < 0.0)) return 0.0;
    return std::clamp(0.5 * (a - c) / denominator, -1.0, 1.0);
}

// 同上，中間那一點是三點裡最大的：頂點夾在 ±0.5 之內。
double parabolaOffset(double a, double b, double c) { return std::clamp(parabolaVertex(a, b, c), -0.5, 0.5); }


// 一個角度的分數圖：小的直接算（OpenCvCorrelation.h），大的用 cv::matchTemplate（DFT）。兩者是同一個定義。
void scoreMap(const cv::Mat& patch, const PatchIntegrals* integrals, const OpenCvTemplateLevel& lv, int k, cv::Mat& scores) {
    const std::size_t i = static_cast<std::size_t>(k);
    if (integrals != nullptr) correlate(patch, *integrals, lv.rotated[i], lv.sums[i], lv.centeredSquares[i], scores);
    else cv::matchTemplate(patch, lv.rotated[i], scores, cv::TM_CCOEFF_NORMED);
}

// 把角度放回 [min, max]（整圈的範本會給出差 360° 的同一個角度）；放不回去就夾在邊上。
double angleIntoRange(double a, const SearchParams& params) {
    for (int turn = -2; turn <= 2; ++turn) {
        const double shifted = a + 360.0 * turn;
        if (shifted >= params.angleMinDeg && shifted <= params.angleMaxDeg) return shifted;
    }
    return std::clamp(a, params.angleMinDeg, params.angleMaxDeg);
}

// 平行的工作裡攔下的第一個錯誤（cv::parallel_for_ 的例外不保證傳得出來，所以在工作裡攔）。
class FirstError {
public:
    void set(Error e) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!error_) error_ = std::move(e);
        failed_.store(true, std::memory_order_relaxed);
    }
    bool failed() const noexcept { return failed_.load(std::memory_order_relaxed); }
    Error take() {
        std::lock_guard<std::mutex> lock(mutex_);
        return error_ ? *error_ : makeError(ErrorCode::Internal, "Unknown failure in the OpenCV search.");
    }

private:
    std::mutex mutex_;
    std::atomic<bool> failed_{false};
    std::optional<Error> error_;
};

template <typename Body>
void guarded(FirstError& errors, Body&& body) {
    try {
        body();
    } catch (const cv::Exception& e) {
        errors.set(makeError(ErrorCode::BackendFailed, std::string("OpenCV failed while searching: ") + e.what()));
    } catch (const std::bad_alloc&) {
        errors.set(makeError(ErrorCode::BackendFailed, "Not enough memory to search the image."));
    } catch (...) {
        errors.set(makeError(ErrorCode::Internal, "Unexpected exception while searching the image."));
    }
}

class StatsSink {
public:
    void add(const OpenCvSearchStats& s) {
        std::lock_guard<std::mutex> lock(mutex_);
        total_.convertMs += s.convertMs;
        total_.pyramidMs += s.pyramidMs;
        total_.coarseMs += s.coarseMs;
        total_.refineMs += s.refineMs;
        total_.coarseCandidates += s.coarseCandidates;
        total_.topLevel = std::max(total_.topLevel, s.topLevel);
        total_.topAngles = std::max(total_.topAngles, s.topAngles);
    }
    OpenCvSearchStats total() const { return total_; }

private:
    std::mutex mutex_;
    OpenCvSearchStats total_;
};

struct SearchContext {
    const ImageView& image;
    const OpenCvTemplate& tmpl;
    const SearchParams& params;
    const OpenCvSearchOptions& options;
    const CancelToken& cancel;
    AngleWindow window;
    double coarseThreshold = 0.0;
    FirstError& errors;
    StatsSink& stats;
};

// 頂層：每個角度一次 matchTemplate，取每個位置最好的分數與角度，局部最大值 ≥ 門檻的是候選。
// 影像切成幾條橫帶平行（每條帶比所有角度、只寫自己那幾列的結果），記憶體只要每條帶一張分數圖。
std::vector<Candidate> coarseSearch(const SearchContext& ctx, const cv::Mat& image, int level, const cv::Point& origin,
                                    std::size_t& anglesUsed) {
    const OpenCvTemplateLevel& lv = ctx.tmpl.levels()[static_cast<std::size_t>(level)];
    const std::vector<int> angles = wantedAngles(ctx.tmpl, lv, ctx.window);
    anglesUsed = angles.size();
    const int resultRows = image.rows - lv.height + 1;
    const int resultCols = image.cols - lv.width + 1;
    if (resultRows <= 0 || resultCols <= 0) return {};

    cv::Mat best(resultRows, resultCols, CV_32F, cv::Scalar(-2.0f));
    cv::Mat bestAngle(resultRows, resultCols, CV_32S, cv::Scalar(0));
    const int bands = std::clamp(resultRows / 64, 1, std::max(1, cv::getNumThreads() * 2));
    cv::parallel_for_(cv::Range(0, bands), [&](const cv::Range& range) {
        guarded(ctx.errors, [&] {
            cv::Mat scores;
            for (int b = range.start; b < range.end; ++b) {
                const int r0 = resultRows * b / bands;
                const int r1 = resultRows * (b + 1) / bands;
                const cv::Mat strip = image(cv::Rect(0, r0, image.cols, r1 - r0 + lv.height - 1));
                const bool direct = correlateDirectly(strip.size(), lv.rotated.front().size());
                const PatchIntegrals integrals = direct ? integralsOf(strip) : PatchIntegrals{};
                for (const int k : angles) {
                    if (ctx.cancel.cancelled() || ctx.errors.failed()) return;
                    scoreMap(strip, direct ? &integrals : nullptr, lv, k, scores);
                    for (int y = 0; y < scores.rows; ++y) {
                        const float* s = scores.ptr<float>(y);
                        float* bst = best.ptr<float>(r0 + y);
                        int* arg = bestAngle.ptr<int>(r0 + y);
                        for (int x = 0; x < scores.cols; ++x) {
                            if (s[x] > bst[x]) {
                                bst[x] = s[x];
                                arg[x] = k;
                            }
                        }
                    }
                }
            }
        });
    });
    if (ctx.cancel.cancelled() || ctx.errors.failed()) return {};

    // 局部最大值：鄰域（範本邊長 × peakRadiusFraction）裡最大的。平台上會有幾個一樣大的，之後的 NMS 合併。
    const int rx = std::max(1, static_cast<int>(std::lround(lv.width * ctx.options.peakRadiusFraction)));
    const int ry = std::max(1, static_cast<int>(std::lround(lv.height * ctx.options.peakRadiusFraction)));
    cv::Mat neighborhoodMax;
    cv::dilate(best, neighborhoodMax, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2 * rx + 1, 2 * ry + 1)));
    const float threshold = static_cast<float>(ctx.coarseThreshold);
    std::vector<Candidate> out;
    for (int y = 0; y < resultRows; ++y) {
        const float* v = best.ptr<float>(y);
        const float* m = neighborhoodMax.ptr<float>(y);
        const int* arg = bestAngle.ptr<int>(y);
        for (int x = 0; x < resultCols; ++x) {
            if (v[x] < threshold || v[x] < m[x]) continue;
            Candidate c;
            c.x = fromPyramidLevel(x + lv.width * 0.5, level) + origin.x;
            c.y = fromPyramidLevel(y + lv.height * 0.5, level) + origin.y;
            c.angleDeg = lv.anglesDeg[static_cast<std::size_t>(arg[x])];
            c.score = v[x];
            out.push_back(c);
        }
    }
    return out;
}

// 同一個元件的候選只留分數最高的（範本大小的旋轉矩形），再依分數留前 limit 個。
std::vector<Candidate> suppress(std::vector<Candidate> candidates, const OpenCvTemplate& t, double maxOverlap, std::size_t limit) {
    std::vector<ScoredBox> boxes;
    boxes.reserve(candidates.size());
    for (const Candidate& c : candidates) {
        if (c.alive) boxes.push_back({RectObj{Pose2D{c.x, c.y, c.angleDeg}, t.size().w, t.size().h}, c.score});
    }
    std::vector<Candidate> alive;
    alive.reserve(boxes.size());
    for (const Candidate& c : candidates) {
        if (c.alive) alive.push_back(c);
    }
    const std::vector<std::size_t> kept = NonMaxSuppression(NmsOptions{maxOverlap, OverlapMeasure::IntersectionOverUnion}).keep(boxes);
    std::vector<Candidate> out;
    out.reserve(std::min(kept.size(), limit));
    for (const std::size_t k : kept) {
        if (out.size() >= limit) break;
        out.push_back(alive[k]);
    }
    return out;
}

// 一個候選在第 level 層精修：位置在換算過來的位置 ± radius、角度在上一層的估計 ± previousStep 之內比一次。
// 最好的那一點在範圍的邊上（真正的峰可能在外面）時，往那邊移過去再比（爬坡，最多幾次）：角度補比旁邊的角度，
// 位置以最好的點為中心重新比。所以第 0 層的最好點兩側都有分數，次像素與角度的拋物線內插才做得到。
// 分數 < threshold 的候選淘汰。
void refineCandidate(const SearchContext& ctx, const cv::Mat& image, int level, const cv::Point& origin, int radius,
                     double previousStep, bool finalLevel, double threshold, Candidate& c) {
    const OpenCvTemplate& t = ctx.tmpl;
    const OpenCvTemplateLevel& lv = t.levels()[static_cast<std::size_t>(level)];
    const int count = static_cast<int>(lv.anglesDeg.size());
    const double tolerance = lv.angleStepDeg * 0.5 + 1e-9;
    const auto wanted = [&](int k) { return angleWanted(t, ctx.window, lv.anglesDeg[static_cast<std::size_t>(k)], tolerance); };
    const auto neighbor = [&](int k, int d) {  // 格點上的相鄰角度；沒有（範圍的兩端）回 −1
        const int n = k + d;
        if (t.fullCircle()) return (n + count) % count;
        return n >= 0 && n < count ? n : -1;
    };

    std::vector<int> ks;
    for (int k = 0; k < count; ++k) {
        if (std::abs(t.angleDifference(lv.anglesDeg[static_cast<std::size_t>(k)], c.angleDeg)) <= previousStep + 1e-9 && wanted(k)) {
            ks.push_back(k);
        }
    }
    if (ks.empty()) ks = wantedAngles(t, lv, AngleWindow{c.angleDeg, 0.0, false});

    struct Evaluation {
        int k = 0;
        cv::Mat scores;
        double best = -2.0;
        cv::Point at;
    };
    const double xl = toPyramidLevel(c.x - origin.x, level);
    const double yl = toPyramidLevel(c.y - origin.y, level);
    int tlx = static_cast<int>(std::lround(xl - lv.width * 0.5));
    int tly = static_cast<int>(std::lround(yl - lv.height * 0.5));
    constexpr int kMaxMoves = 3;
    for (int move = 0;; ++move) {
        const int ax0 = std::max(0, tlx - radius);
        const int ax1 = std::min(image.cols - lv.width, tlx + radius);
        const int ay0 = std::max(0, tly - radius);
        const int ay1 = std::min(image.rows - lv.height, tly + radius);
        if (ax0 > ax1 || ay0 > ay1) {  // 範本放不進影像（元件貼著邊緣）
            c.alive = false;
            return;
        }
        const cv::Mat patch = image(cv::Rect(ax0, ay0, ax1 - ax0 + lv.width, ay1 - ay0 + lv.height));
        const bool direct = correlateDirectly(patch.size(), cv::Size(lv.width, lv.height));
        const PatchIntegrals integrals = direct ? integralsOf(patch) : PatchIntegrals{};

        std::vector<Evaluation> evaluations;
        evaluations.reserve(ks.size() + 4);
        const auto evaluate = [&](int k) -> std::size_t {
            for (std::size_t i = 0; i < evaluations.size(); ++i) {
                if (evaluations[i].k == k) return i;
            }
            Evaluation e;
            e.k = k;
            scoreMap(patch, direct ? &integrals : nullptr, lv, k, e.scores);
            cv::minMaxLoc(e.scores, nullptr, &e.best, nullptr, &e.at);
            evaluations.push_back(std::move(e));
            return evaluations.size() - 1;
        };
        std::size_t winner = evaluate(ks.front());
        for (const int k : ks) {
            const std::size_t i = evaluate(k);
            if (evaluations[i].best > evaluations[winner].best) winner = i;
        }
        // 角度的爬坡：最好的角度兩側的格點都要比過；旁邊的更好就移過去。
        for (int step = 0; step < count; ++step) {
            bool moved = false;
            for (const int d : {-1, 1}) {
                const int k = neighbor(evaluations[winner].k, d);
                if (k < 0 || !wanted(k)) continue;
                const std::size_t i = evaluate(k);
                if (evaluations[i].best > evaluations[winner].best) {
                    winner = i;
                    moved = true;
                }
            }
            if (!moved) break;
        }
        const Evaluation w = evaluations[winner];  // 複本（cv::Mat 只複製標頭）：之後的 evaluate 可能讓 vector 重新配置
        if (w.best < threshold) {
            c.alive = false;
            return;
        }

        // 位置的爬坡：最好的點在分數圖的邊上（而且那一邊不是影像的邊），以它為中心再比一次。
        const bool leftEdge = w.at.x == 0 && ax0 > 0;
        const bool rightEdge = w.at.x == w.scores.cols - 1 && ax1 < image.cols - lv.width;
        const bool topEdge = w.at.y == 0 && ay0 > 0;
        const bool bottomEdge = w.at.y == w.scores.rows - 1 && ay1 < image.rows - lv.height;
        if ((leftEdge || rightEdge || topEdge || bottomEdge) && move < kMaxMoves) {
            tlx = ax0 + w.at.x;
            tly = ay0 + w.at.y;
            ks.assign(1, w.k);
            continue;
        }

        double cx = ax0 + w.at.x + lv.width * 0.5;
        double cy = ay0 + w.at.y + lv.height * 0.5;
        double angle = lv.anglesDeg[static_cast<std::size_t>(w.k)];
        if (finalLevel) {
            const cv::Mat& s = w.scores;
            const cv::Point p = w.at;
            const auto at = [&](int x, int y) { return static_cast<double>(s.at<float>(y, x)); };
            if (p.x > 0 && p.x + 1 < s.cols) cx += parabolaOffset(at(p.x - 1, p.y), at(p.x, p.y), at(p.x + 1, p.y));
            if (p.y > 0 && p.y + 1 < s.rows) cy += parabolaOffset(at(p.x, p.y - 1), at(p.x, p.y), at(p.x, p.y + 1));
            // 角度：同一個位置上，相鄰兩個角度的分數（爬坡保證比過了）。最好的角度在範圍的一端時，
            // 用往內的三個角度擬合，頂點夾在「這一端」與「往內半步」之間（不超出範圍）。
            // 誤差約是這一層角度步進的 0.2 倍（角度掃描的測試；NCC 對角度的曲線在真值兩側不對稱）。
            const auto valid = [&](int k) { return k >= 0 && k != w.k && wanted(k); };
            const auto scoreAt = [&](int k) { return static_cast<double>(evaluations[evaluate(k)].scores.at<float>(p.y, p.x)); };
            const double self = w.best;
            const int before = neighbor(w.k, -1);
            const int after = neighbor(w.k, 1);
            if (lv.angleStepDeg > 0.0) {
                if (valid(before) && valid(after) && before != after) {
                    angle += lv.angleStepDeg * parabolaOffset(scoreAt(before), self, scoreAt(after));
                } else if (valid(before) && valid(neighbor(before, -1))) {
                    const double v = -1.0 + parabolaVertex(scoreAt(neighbor(before, -1)), scoreAt(before), self);
                    angle += lv.angleStepDeg * std::clamp(v, -0.5, 0.0);
                } else if (valid(after) && valid(neighbor(after, 1))) {
                    const double v = 1.0 + parabolaVertex(self, scoreAt(after), scoreAt(neighbor(after, 1)));
                    angle += lv.angleStepDeg * std::clamp(v, 0.0, 0.5);
                }
            }
        }
        c.x = fromPyramidLevel(cx, level) + origin.x;
        c.y = fromPyramidLevel(cy, level) + origin.y;
        c.angleDeg = angle;
        c.score = w.best;
        return;
    }
}

// 一塊區域：轉灰階 → 金字塔 → 頂層 → 逐層精修。回傳第 0 層的候選（還沒過濾區域與分數）。
std::vector<Candidate> searchArea(const SearchContext& ctx, const Area& area) {
    OpenCvSearchStats stats;
    Clock::time_point t0 = Clock::now();
    const GrayRoi gray = toGray8(ctx.image, area.roi);
    stats.convertMs = millisecondsSince(t0);
    if (gray.mat.empty()) return {};

    const OpenCvTemplate& t = ctx.tmpl;
    t0 = Clock::now();
    const std::vector<cv::Mat> pyramid = buildPyramid(gray.mat, t.topLevel() + 1);
    stats.pyramidMs = millisecondsSince(t0);

    // 頂層：範本要放得進那一層的影像（小視窗在高層可能放不進，就從低一點的層開始）。
    int top = t.topLevel();
    while (top > 0 && (pyramid[static_cast<std::size_t>(top)].cols < t.levels()[static_cast<std::size_t>(top)].width ||
                       pyramid[static_cast<std::size_t>(top)].rows < t.levels()[static_cast<std::size_t>(top)].height)) {
        --top;
    }
    stats.topLevel = top;
    const cv::Point origin = area.roi.tl();

    t0 = Clock::now();
    std::vector<Candidate> candidates = coarseSearch(ctx, pyramid[static_cast<std::size_t>(top)], top, origin, stats.topAngles);
    // 中心離區域太遠的不必精修（精修最多移動頂層的一兩個像素）。
    const double slack = std::ldexp(2.0, top);
    const Rect reach{area.accept.x - slack, area.accept.y - slack, area.accept.w + 2.0 * slack, area.accept.h + 2.0 * slack};
    std::erase_if(candidates, [&](const Candidate& c) { return !reach.contains({c.x, c.y}); });
    candidates = suppress(std::move(candidates), t, ctx.options.nmsMaxOverlap, area.maxCandidates);
    stats.coarseMs = millisecondsSince(t0);
    stats.coarseCandidates = candidates.size();

    t0 = Clock::now();
    const int firstLevel = top == 0 ? 0 : top - 1;
    for (int level = firstLevel; level >= 0 && !candidates.empty(); --level) {
        if (ctx.cancel.cancelled() || ctx.errors.failed()) break;
        const bool finalLevel = level == 0;
        // top ＝ 0（範本太小，沒有金字塔）：頂層就是第 0 層，這一輪只做次像素與角度的內插（± 1 像素、± 1 步）。
        const int radius = top == 0 ? 1 : ctx.options.refineRadiusPx;
        const double previousStep = top == 0 ? t.levels()[0].angleStepDeg : t.levels()[static_cast<std::size_t>(level + 1)].angleStepDeg;
        const double threshold = finalLevel ? ctx.params.minScore : ctx.coarseThreshold;
        const cv::Mat& image = pyramid[static_cast<std::size_t>(level)];
        cv::parallel_for_(cv::Range(0, static_cast<int>(candidates.size())), [&](const cv::Range& range) {
            guarded(ctx.errors, [&] {
                for (int i = range.start; i < range.end; ++i) {
                    if (ctx.cancel.cancelled() || ctx.errors.failed()) return;
                    refineCandidate(ctx, image, level, origin, radius, previousStep, finalLevel, threshold,
                                    candidates[static_cast<std::size_t>(i)]);
                }
            });
        });
        std::erase_if(candidates, [](const Candidate& c) { return !c.alive; });
    }
    stats.refineMs = millisecondsSince(t0);
    ctx.stats.add(stats);
    return candidates;
}

}  // namespace

Result<std::vector<Match>> OpenCvPatternSearcher::find(const ImageView& image, const ITemplate& tmpl, const SearchRegion& region,
                                                       const SearchParams& params, const CancelToken& cancel) {
    return find(image, tmpl, region, params, cancel, nullptr);
}

Result<std::vector<Match>> OpenCvPatternSearcher::find(const ImageView& image, const ITemplate& tmpl, const SearchRegion& region,
                                                       const SearchParams& params, const CancelToken& cancel,
                                                       OpenCvSearchStats* statsOut) const {
    const Clock::time_point started = Clock::now();
    try {
        const auto* t = dynamic_cast<const OpenCvTemplate*>(&tmpl);
        if (t == nullptr || tmpl.backendId() != kOpenCvBackendId) {
            return makeError(ErrorCode::BackendMismatch, "The template was not built by the OpenCV back end.");
        }
        if (!image.valid()) return makeError(ErrorCode::InvalidArgument, "The image is not valid.");
        if (!(params.minScore >= 0.0 && params.minScore <= 1.0)) {
            return makeError(ErrorCode::InvalidArgument, "minScore must be between 0 and 1.");
        }
        if (!std::isfinite(params.angleMinDeg) || !std::isfinite(params.angleMaxDeg) || params.angleMinDeg > params.angleMaxDeg) {
            return makeError(ErrorCode::InvalidArgument, "The angle range is not valid.");
        }
        // 範本只建了它的角度範圍：要搜的範圍超出它，就沒有可以比的範本（不默默只搜一部分）。
        if (!t->fullCircle() && (params.angleMinDeg < t->angleMinDeg() - 1e-6 || params.angleMaxDeg > t->angleMaxDeg() + 1e-6)) {
            return makeError(ErrorCode::InvalidArgument,
                             "The template was built for angles " + std::to_string(t->angleMinDeg()) + " to " +
                                 std::to_string(t->angleMaxDeg()) + " degrees; the search asks for " +
                                 std::to_string(params.angleMinDeg) + " to " + std::to_string(params.angleMaxDeg) + ".");
        }
        if (t->levels().empty() || t->sampleProblem()) return std::vector<Match>{};
        if (cancel.cancelled()) return cancelledError();

        const Rect imageRect{0.0, 0.0, static_cast<double>(image.width), static_cast<double>(image.height)};
        const cv::Rect imagePixels(0, 0, image.width, image.height);
        const double tw = t->size().w;
        const double th = t->size().h;
        std::vector<Area> areas;
        if (region.kind == SearchRegion::Kind::WholeImage) {
            // 頂層的候選最多 maxMatches 的兩倍（精修會淘汰一些）；不溢位。
            const std::size_t limit = params.maxMatches > SIZE_MAX / 2 ? SIZE_MAX : params.maxMatches * 2;
            areas.push_back({imagePixels, imageRect, std::max<std::size_t>(64, limit)});
        } else {
            // 每個視窗外加「範本的一半 ＋ 頂層兩個像素」：中心落在視窗裡的元件，整個範本都在這塊裡面。
            const double margin = 0.5 * std::hypot(tw, th) + std::ldexp(2.0, t->topLevel()) + 1.0;
            for (const Rect& w : region.windows) {
                const Rect accept = intersection(w, imageRect);
                if (accept.empty()) continue;
                const cv::Rect roi(static_cast<int>(std::floor(accept.x - margin)), static_cast<int>(std::floor(accept.y - margin)),
                                   static_cast<int>(std::ceil(accept.w + 2.0 * margin)) + 1, static_cast<int>(std::ceil(accept.h + 2.0 * margin)) + 1);
                const cv::Rect clipped = roi & imagePixels;
                if (clipped.width < static_cast<int>(tw) || clipped.height < static_cast<int>(th)) continue;
                // 視窗驗證的視窗只比範本大一點：最多 maxCandidatesPerWindow 個。大的區域（流程 B 的區域掃描）可以有很多元件：
                // 依面積放寬（每個範本面積 2 個）。
                const double capacity = std::ceil(2.0 * accept.area() / std::max(1.0, tw * th));
                const std::size_t limit = std::max(options_.maxCandidatesPerWindow, static_cast<std::size_t>(std::min(capacity, 1e7)));
                areas.push_back({clipped, accept, limit});
            }
        }

        FirstError errors;
        StatsSink stats;
        SearchContext ctx{image, *t, params, options_, cancel, {}, std::max(0.0, params.minScore - options_.coarseScoreDrop), errors, stats};
        const double range = params.angleMaxDeg - params.angleMinDeg;
        ctx.window = AngleWindow{params.angleMinDeg + 0.5 * range, 0.5 * range, range >= 360.0 - 1e-9};

        std::vector<std::vector<Candidate>> found(areas.size());
        if (areas.size() == 1) {
            found[0] = searchArea(ctx, areas[0]);
        } else {
            cv::parallel_for_(cv::Range(0, static_cast<int>(areas.size())), [&](const cv::Range& r) {
                guarded(errors, [&] {
                    for (int i = r.start; i < r.end; ++i) {
                        if (cancel.cancelled() || errors.failed()) return;
                        found[static_cast<std::size_t>(i)] = searchArea(ctx, areas[static_cast<std::size_t>(i)]);
                    }
                });
            });
        }
        if (errors.failed()) return errors.take();
        if (cancel.cancelled()) return cancelledError();

        // 過濾（分數、中心在區域內）→ NMS（重疊的視窗、平台上的幾個峰）→ 排序 → 最多 maxMatches 個。
        const Clock::time_point nmsStart = Clock::now();
        std::vector<Match> matches;
        for (std::size_t a = 0; a < areas.size(); ++a) {
            for (const Candidate& c : found[a]) {
                if (!(c.score >= params.minScore) || !areas[a].accept.contains({c.x, c.y})) continue;
                matches.push_back({Pose2D{c.x, c.y, angleIntoRange(c.angleDeg, params)}, std::clamp(c.score, 0.0, 1.0)});
            }
        }
        std::vector<ScoredBox> boxes;
        boxes.reserve(matches.size());
        for (const Match& m : matches) boxes.push_back({RectObj{m.pose, tw, th}, m.score});
        const std::vector<std::size_t> kept =
            NonMaxSuppression(NmsOptions{options_.nmsMaxOverlap, OverlapMeasure::IntersectionOverUnion}).keep(boxes);
        std::vector<Match> out;
        out.reserve(std::min(kept.size(), params.maxMatches));
        for (const std::size_t k : kept) {
            if (out.size() >= params.maxMatches) break;
            out.push_back(matches[k]);
        }

        if (statsOut != nullptr) {
            *statsOut = stats.total();
            statsOut->nmsMs = millisecondsSince(nmsStart);
            statsOut->totalMs = millisecondsSince(started);
            statsOut->areas = areas.size();
            statsOut->matches = out.size();
        }
        return out;
    } catch (const cv::Exception& e) {
        return makeError(ErrorCode::BackendFailed, std::string("OpenCV failed while searching: ") + e.what());
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::BackendFailed, "Not enough memory to search the image.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while searching the image.");
    }
}

}  // namespace aoi::infra
