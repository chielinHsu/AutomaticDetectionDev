#include "domain/lattice/lattice_estimator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

#include "domain/lattice/basis_finder.h"
#include "domain/lattice/cell_components.h"
#include "domain/lattice/lattice_acceptance.h"
#include "domain/lattice/lattice_confidence.h"
#include "domain/lattice/lattice_detail.h"

// 擬合流程（找基底 → 原點 → 精修 → 連通 → 接受規則 → 定形式與品質）。找基底在 basis_finder，接受規則在 lattice_acceptance，
// 連通在 cell_components；這裡只有「把一組基底擬合到點上」與整個流程的順序。
namespace aoi {

namespace {

using detail::Assigned;
using detail::Assigner;
using detail::LatticeCandidate;

// 對一組點找「一組」晶格（主晶格）。fitAll 對剩下的點再建一個。
class Fitter {
public:
    struct Outcome {
        std::optional<LatticeModel> model;  // inliers 是 Fitter 的點的索引
        NoLatticeReason reason = NoLatticeReason::None;
        double rejectedConfidence = 0.0;
    };

    Fitter(const LatticeOptions& options, const IVectorClustering& clustering, std::span<const Point2D> points)
        : o_(options), clustering_(clustering), pts_(points), rng_(options.seed) {}

    Outcome run() {
        Outcome out;
        if (pts_.size() < 2) {
            out.reason = NoLatticeReason::TooFewPoints;
            return out;
        }
        double minX = pts_[0].x, maxX = pts_[0].x, minY = pts_[0].y, maxY = pts_[0].y;
        for (const Point2D& p : pts_) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
        const double extent = std::max(maxX - minX, maxY - minY);
        if (!(extent > 0.0)) {
            out.reason = NoLatticeReason::NoDirection;  // 全部重疊
            return out;
        }
        const detail::BasisFinder finder(o_, clustering_, pts_, 1e-9 * extent);
        const std::vector<Point2D> directions = finder.directions();
        if (directions.empty()) {
            out.reason = NoLatticeReason::NoDirection;
            return out;
        }
        // u 的候選：票數夠多的方向裡最短的 3 個。通常第一個 u 就成功；失敗時（例如兩種相近的間距被投票合併成一個
        // 不存在的向量）再試下一個。同一個 u 先試二維、再試一維：二維被任何一條規則擋下時，一維仍然要試
        // （否則三排間距不同時，會因為「先找到的二維是假的」而錯過真的那一排）。
        out.reason = NoLatticeReason::NoConsistentLattice;
        const std::size_t tries = std::min<std::size_t>(directions.size(), 3);
        for (std::size_t t = 0; t < tries; ++t) {
            const Point2D u = directions[t];
            if (const std::optional<Point2D> v = finder.secondDirection(u, directions)) {
                std::optional<LatticeCandidate> plane = fitHypothesis({{}, u, *v, true});
                if (plane && detail::twoDimensionalIsValid(*plane, pts_, o_) && accept(*plane, out)) return out;
            }
            std::optional<LatticeCandidate> line = fitHypothesis({{}, u, {}, false});
            if (line && accept(*line, out)) return out;
        }
        return out;
    }

private:
    // 證據、缺格、信心三條規則都過：放進 out 回傳 true。沒過：在 out 記下最好的失敗原因，回傳 false。
    bool accept(const LatticeCandidate& candidate, Outcome& out) const {
        if (!detail::enoughEvidence(candidate, pts_, o_)) return false;
        LatticeModel model = finalize(candidate);
        if (!detail::holesAreBacked(model)) {
            if (out.reason != NoLatticeReason::LowConfidence) out.reason = NoLatticeReason::TooManyHoles;
            return false;
        }
        if (model.confidence < o_.minConfidence) {
            if (out.reason != NoLatticeReason::LowConfidence || model.confidence > out.rejectedConfidence) {
                out.reason = NoLatticeReason::LowConfidence;
                out.rejectedConfidence = model.confidence;
            }
            return false;
        }
        out.model = std::move(model);
        out.reason = NoLatticeReason::None;
        out.rejectedConfidence = 0.0;
        return true;
    }

    // RANSAC：隨機挑點當原點（第一次固定用最靠近中位數的點），數它附近 ransacIndexRadius 格內有幾個點落在格點上。
    // 只數附近：基底的小誤差在遠處會放大，數全部的點會偏好「剛好不受誤差影響」的假設，而不是好的原點。
    std::size_t pickOrigin(LatticeBasis basis, double tol) {
        const std::size_t n = pts_.size();
        std::vector<double> xs(n);
        std::vector<double> ys(n);
        for (std::size_t k = 0; k < n; ++k) {
            xs[k] = pts_[k].x;
            ys[k] = pts_[k].y;
        }
        const auto mid = static_cast<std::ptrdiff_t>(n / 2);
        std::nth_element(xs.begin(), xs.begin() + mid, xs.end());
        std::nth_element(ys.begin(), ys.begin() + mid, ys.end());
        const Point2D median{xs[static_cast<std::size_t>(mid)], ys[static_cast<std::size_t>(mid)]};
        std::size_t central = 0;
        double centralDistance = detail::squaredDistance(pts_[0], median);
        for (std::size_t k = 1; k < n; ++k) {
            const double d = detail::squaredDistance(pts_[k], median);
            if (d < centralDistance) {
                centralDistance = d;
                central = k;
            }
        }

        const double tol2 = tol * tol;
        const int trials = std::max(1, std::min(o_.ransacTrials, static_cast<int>(std::min<std::size_t>(n, 1u << 20))));
        std::size_t bestIndex = central;
        std::size_t bestScore = 0;
        double bestError = std::numeric_limits<double>::infinity();
        for (int trial = 0; trial < trials; ++trial) {
            const std::size_t candidate = trial == 0 ? central : rng_.below(n);
            basis.origin = pts_[candidate];
            const Assigner assign(basis);
            std::size_t score = 0;
            std::size_t nearby = 0;
            double error = 0.0;
            for (const Point2D& p : pts_) {
                const Assigned a = assign(p);
                if (detail::reach(a.cell) > o_.ransacIndexRadius) continue;
                ++nearby;
                if (a.residual2 <= tol2) {
                    ++score;
                    error += a.residual2;
                }
            }
            if (trial == 0 || score > bestScore || (score == bestScore && error < bestError)) {
                bestIndex = candidate;
                bestScore = score;
                bestError = error;
            }
            if (score == nearby && score >= 2) break;  // 附近每個點都在格點上：這個原點夠好了
        }
        return bestIndex;
    }

    // 由原點附近開始精修，每次把範圍（格數）加倍：指派 → 最小平方法。範圍蓋住所有點時停止。
    // 基底的小誤差因此不會在陣列遠端累積成整格的錯位。
    LatticeBasis grow(LatticeBasis b, double tol) const {
        const double tol2 = tol * tol;
        int limit = std::max(2, o_.ransacIndexRadius);
        for (;;) {
            const Assigner assign(b);
            LatticeAccumulator acc(b.origin, LatticeIndex{0, 0});
            int farthest = 0;
            for (const Point2D& p : pts_) {
                const Assigned a = assign(p);
                const int r = detail::reach(a.cell);
                farthest = std::max(farthest, r);
                if (r > limit || !(a.residual2 <= tol2)) continue;
                acc.add(a.cell, p);
            }
            if (const std::optional<LatticeBasis> solved = acc.solve()) b = detail::mergeSolution(b, *solved);
            if (limit >= farthest) break;
            limit = limit >= (1 << 26) ? farthest : limit * 2;
        }
        return b;
    }

    // 每個點指派到最近的格子；殘差 ≤ 容差才算成員；同一格有兩個點時留殘差小的。
    // allowed 不是空的時候，只接受在這個索引範圍內的格子（{minI, maxI, minJ, maxJ}）。
    void assignAll(const LatticeBasis& b, double tol, LatticeCandidate& c, const int* allowed = nullptr) const {
        c.basis = b;
        c.tolerance = tol;
        c.members.clear();
        c.cells.clear();
        const double tol2 = tol * tol;
        const Assigner assign(b);
        // 第一輪：指派，記下落在容差內的點與它們的索引範圍。第二輪：同一格只留殘差最小的（用索引範圍上的密集表）。
        hits_.clear();
        int minI = 0, maxI = 0, minJ = 0, maxJ = 0;
        for (std::size_t k = 0; k < pts_.size(); ++k) {
            const Assigned a = assign(pts_[k]);
            if (!(a.residual2 <= tol2)) continue;
            if (allowed != nullptr &&
                (a.cell.i < allowed[0] || a.cell.i > allowed[1] || a.cell.j < allowed[2] || a.cell.j > allowed[3]))
                continue;
            if (hits_.empty()) {
                minI = maxI = a.cell.i;
                minJ = maxJ = a.cell.j;
            }
            minI = std::min(minI, a.cell.i);
            maxI = std::max(maxI, a.cell.i);
            minJ = std::min(minJ, a.cell.j);
            maxJ = std::max(maxJ, a.cell.j);
            hits_.push_back({k, a});
        }
        if (hits_.empty()) return;
        detail::CellTable bestHit(minI, maxI, minJ, maxJ, hits_.size());
        for (std::size_t h = 0; h < hits_.size(); ++h) {
            const LatticeIndex cell = hits_[h].second.cell;
            const std::int32_t current = bestHit.get(cell.i, cell.j);
            if (current < 0 || hits_[h].second.residual2 < hits_[static_cast<std::size_t>(current)].second.residual2)
                bestHit.set(cell.i, cell.j, static_cast<std::int32_t>(h));
        }
        for (std::size_t h = 0; h < hits_.size(); ++h) {
            const LatticeIndex cell = hits_[h].second.cell;
            if (bestHit.get(cell.i, cell.j) != static_cast<std::int32_t>(h)) continue;
            c.members.push_back(hits_[h].first);
            c.cells.push_back(cell);
        }
    }

    std::optional<LatticeBasis> refit(const LatticeCandidate& c) const {
        LatticeAccumulator acc(c.basis.origin, LatticeIndex{0, 0});
        for (std::size_t m = 0; m < c.members.size(); ++m) acc.add(c.cells[m], pts_[c.members[m]]);
        const std::optional<LatticeBasis> solved = acc.solve();
        if (!solved) return std::nullopt;
        return detail::mergeSolution(c.basis, *solved);
    }

    // 連通：只留成員最多的那一塊（cell_components.h）。回傳有沒有拿掉成員。
    bool keepLargestComponent(LatticeCandidate& c) const {
        const std::vector<std::size_t> keep = detail::largestComponent(c.cells, o_.maxGapCells + 1, c.basis.hasV);
        if (keep.size() == c.members.size()) return false;
        std::vector<std::size_t> members;
        std::vector<LatticeIndex> cells;
        members.reserve(keep.size());
        cells.reserve(keep.size());
        for (const std::size_t k : keep) {
            members.push_back(c.members[k]);
            cells.push_back(c.cells[k]);
        }
        c.members = std::move(members);
        c.cells = std::move(cells);
        return true;
    }

    std::optional<LatticeCandidate> fitHypothesis(LatticeBasis basis) {
        double tol = detail::toleranceFor(basis, o_.inlierFraction);
        if (!(tol > 0.0)) return std::nullopt;
        basis.origin = pts_[pickOrigin(basis, tol)];
        basis = grow(basis, tol);

        // grow 的最後一次已經用全部的點精修過；再指派一次（同一格只留一個點）、精修，讓精修後才落進容差的點也算進來。
        LatticeCandidate c;
        tol = detail::toleranceFor(basis, o_.inlierFraction);
        assignAll(basis, tol, c);
        if (c.members.size() < 2) return std::nullopt;
        if (const std::optional<LatticeBasis> solved = refit(c)) basis = *solved;
        tol = detail::toleranceFor(basis, o_.inlierFraction);
        assignAll(basis, tol, c);
        if (c.members.size() < 2) return std::nullopt;
        if (keepLargestComponent(c)) {
            if (const std::optional<LatticeBasis> solved = refit(c)) c.basis = *solved;
            // 重新指派，但只收這一塊的索引範圍內的點：遠處碰巧落在格點上的離群點不會再被收回來。
            int box[4] = {c.cells[0].i, c.cells[0].i, c.cells[0].j, c.cells[0].j};
            for (const LatticeIndex& cell : c.cells) {
                box[0] = std::min(box[0], cell.i);
                box[1] = std::max(box[1], cell.i);
                box[2] = std::min(box[2], cell.j);
                box[3] = std::max(box[3], cell.j);
            }
            assignAll(c.basis, detail::toleranceFor(c.basis, o_.inlierFraction), c, box);
            if (c.members.size() < 2) return std::nullopt;
            if (const std::optional<LatticeBasis> solved = refit(c)) c.basis = *solved;
        }
        return c;
    }

    // 定形式（約化、方向、索引從 0 起）、最後一次最小平方法、品質與信心。
    LatticeModel finalize(const LatticeCandidate& c) const {
        LatticeBasis b = c.basis;
        std::vector<std::size_t> order(c.members.size());
        std::iota(order.begin(), order.end(), std::size_t{0});
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t bb) { return c.members[a] < c.members[bb]; });
        std::vector<std::size_t> members;
        std::vector<LatticeIndex> cells;
        members.reserve(order.size());
        cells.reserve(order.size());
        for (const std::size_t k : order) {
            members.push_back(c.members[k]);
            cells.push_back(c.cells[k]);
        }
        // Lagrange–Gauss 約化：|u| ≤ |v|，而且 v 在 u 方向的分量不超過 |u| 的一半。基底換了，索引跟著換
        // （p ＝ o ＋ i·u ＋ j·v 不變）：交換 u、v 時交換 i、j；v −＝ k·u 時 i ＋＝ k·j。
        if (b.hasV) {
            for (int guard = 0; guard < 64; ++guard) {
                if (detail::length(b.v) < detail::length(b.u)) {
                    std::swap(b.u, b.v);
                    for (LatticeIndex& cell : cells) std::swap(cell.i, cell.j);
                }
                const double k = std::round(detail::dot(b.u, b.v) / detail::dot(b.u, b.u));
                if (k == 0.0) break;
                b.v = detail::minus(b.v, detail::times(b.u, k));
                const int step = static_cast<int>(k);
                for (LatticeIndex& cell : cells) cell.i += step * cell.j;
            }
        }
        // 方向：u 在右半平面、cross(u, v) > 0（u 朝右時 v 朝下）。
        if (b.u.x < 0.0 || (b.u.x == 0.0 && b.u.y < 0.0)) {
            b.u = detail::times(b.u, -1.0);
            for (LatticeIndex& cell : cells) cell.i = -cell.i;
        }
        if (b.hasV && detail::cross(b.u, b.v) < 0.0) {
            b.v = detail::times(b.v, -1.0);
            for (LatticeIndex& cell : cells) cell.j = -cell.j;
        }
        if (!b.hasV) {
            b.v = {0.0, 0.0};
            for (LatticeIndex& cell : cells) cell.j = 0;
        }
        int minI = cells.front().i, minJ = cells.front().j;
        for (const LatticeIndex& cell : cells) {
            minI = std::min(minI, cell.i);
            minJ = std::min(minJ, cell.j);
        }
        b.origin = b.at(minI, minJ);
        for (LatticeIndex& cell : cells) {
            cell.i -= minI;
            cell.j -= minJ;
        }

        LatticeModel model;
        model.equations = LatticeAccumulator(b.origin, LatticeIndex{0, 0});
        for (std::size_t k = 0; k < members.size(); ++k) model.equations.add(cells[k], pts_[members[k]]);
        if (const std::optional<LatticeBasis> solved = model.equations.solve()) b = detail::mergeSolution(b, *solved);
        model.basis = b;

        double sumSquares = 0.0;
        for (std::size_t k = 0; k < members.size(); ++k) sumSquares += detail::squaredDistance(pts_[members[k]], b.at(cells[k]));
        model.rmsPx = std::sqrt(sumSquares / static_cast<double>(members.size()));
        model.tolerancePx = detail::toleranceFor(b, o_.inlierFraction);
        for (const LatticeIndex& cell : cells) {
            model.maxI = std::max(model.maxI, cell.i);
            model.maxJ = std::max(model.maxJ, cell.j);
        }
        model.inliers = std::move(members);
        model.cells = std::move(cells);
        model.coverage = static_cast<double>(model.inliers.size()) / static_cast<double>(model.cellCount());
        model.consistency = detail::consistency(model, pts_);
        const LatticeQuality q =
            assessLattice(model.inliers.size(), model.rmsPx, model.tolerancePx, model.coverage, model.consistency, b.hasV);
        model.confidence = q.confidence;
        model.suggestCheck = q.confidence < o_.checkBelow;
        return model;
    }

    const LatticeOptions& o_;
    const IVectorClustering& clustering_;
    std::span<const Point2D> pts_;
    detail::SplitMix64 rng_;
    mutable std::vector<std::pair<std::size_t, Assigned>> hits_;  // assignAll 的暫存（重複使用，不必每次配置）
};

}  // namespace

LatticeEstimator::LatticeEstimator(LatticeOptions options, std::shared_ptr<const IVectorClustering> clustering)
    : options_(options), clustering_(clustering ? std::move(clustering) : std::make_shared<GridVoteClustering>()) {}

std::optional<LatticeModel> LatticeEstimator::fit(std::span<const Point2D> points) const {
    LatticeFit f = run(points, 1);
    if (f.models.empty()) return std::nullopt;
    return std::move(f.models.front());
}

LatticeFit LatticeEstimator::fitAll(std::span<const Point2D> points) const {
    return run(points, std::max<std::size_t>(options_.maxLattices, 1));
}

LatticeFit LatticeEstimator::run(std::span<const Point2D> points, std::size_t maxModels) const {
    LatticeFit result;
    std::vector<std::size_t> active;
    active.reserve(points.size());
    for (std::size_t k = 0; k < points.size(); ++k) {
        if (detail::finitePoint(points[k])) active.push_back(k);
    }
    std::vector<Point2D> local;
    while (result.models.size() < maxModels) {
        const std::size_t need = result.models.empty() ? 2 : std::max<std::size_t>(2, options_.minExtraLatticeInliers);
        if (active.size() < need) {
            if (result.models.empty()) result.reason = NoLatticeReason::TooFewPoints;
            break;
        }
        local.clear();
        for (const std::size_t k : active) local.push_back(points[k]);
        Fitter fitter(options_, *clustering_, local);
        Fitter::Outcome outcome = fitter.run();
        if (!outcome.model) {
            if (result.models.empty()) {
                result.reason = outcome.reason;
                result.rejectedConfidence = outcome.rejectedConfidence;
            }
            break;
        }
        LatticeModel model = std::move(*outcome.model);
        if (!result.models.empty() && model.inliers.size() < options_.minExtraLatticeInliers) break;
        for (std::size_t& index : model.inliers) index = active[index];  // 換回輸入的索引（仍由小到大）

        // 這組晶格「認領」的點：在它的索引範圍內、離格點不到 2 倍容差（含內點，也含同一格的重複點與雜訊大一點的點）。
        // 認領的點不再參與下一組，免得同一個陣列的殘渣被湊成第二組晶格。
        const Assigner assign(model.basis);
        const double claim2 = 4.0 * model.tolerancePx * model.tolerancePx;
        std::vector<std::size_t> rest;
        rest.reserve(active.size());
        for (const std::size_t k : active) {
            const Assigned a = assign(points[k]);
            const bool inBox = a.cell.i >= model.minI && a.cell.i <= model.maxI && a.cell.j >= model.minJ && a.cell.j <= model.maxJ;
            if (inBox && a.residual2 <= claim2) continue;
            rest.push_back(k);
        }
        active = std::move(rest);
        result.models.push_back(std::move(model));
    }
    if (!result.models.empty()) result.reason = NoLatticeReason::None;
    // 主晶格放第一個：內點最多的（同數時維持找到的順序）。
    std::stable_sort(result.models.begin(), result.models.end(),
                     [](const LatticeModel& a, const LatticeModel& b) { return a.inliers.size() > b.inliers.size(); });
    return result;
}

}  // namespace aoi
