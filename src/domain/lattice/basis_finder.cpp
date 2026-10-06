#include "domain/lattice/basis_finder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include "domain/lattice/lattice_detail.h"
#include "domain/lattice/spatial_grid.h"

namespace aoi::detail {

BasisFinder::BasisFinder(const LatticeOptions& options, const IVectorClustering& clustering, std::span<const Point2D> points,
                         double minDistance)
    : o_(options), clustering_(clustering), pts_(points), minDistance_(minDistance) {}

std::vector<Point2D> BasisFinder::directions() const {
    const std::size_t n = pts_.size();
    const SpatialGrid grid(pts_, 0.0);
    const std::size_t samples = std::max<std::size_t>(o_.maxSamples, 1);
    const std::size_t stride = std::max<std::size_t>(1, (n + samples - 1) / samples);
    const std::size_t k = std::max<std::size_t>(o_.neighbors, 1);
    std::vector<Point2D> vectors;
    vectors.reserve((n / stride + 1) * k);
    std::vector<double> nearestDistances;
    std::vector<SpatialGrid::Neighbor> neighbours;
    for (std::size_t i = 0; i < n; i += stride) {
        grid.nearest(i, k, minDistance_, neighbours);
        if (neighbours.empty()) continue;
        nearestDistances.push_back(neighbours.front().distance);
        for (const SpatialGrid::Neighbor& nb : neighbours) vectors.push_back(halfPlane(minus(pts_[nb.index], pts_[i])));
    }
    if (vectors.empty()) return {};
    const auto middle = nearestDistances.begin() + static_cast<std::ptrdiff_t>(nearestDistances.size() / 2);
    std::nth_element(nearestDistances.begin(), middle, nearestDistances.end());
    const double typicalSpacing = *middle;

    const VectorClusters clusters = clustering_.cluster(vectors, typicalSpacing);
    std::size_t most = 0;
    for (const VectorCluster& c : clusters.clusters) most = std::max(most, c.members);
    std::vector<Point2D> candidates;
    for (const VectorCluster& c : clusters.clusters) {
        if (static_cast<double>(c.members) < o_.supportFraction * static_cast<double>(most)) continue;
        if (!(length(c.center) > minDistance_)) continue;
        candidates.push_back(halfPlane(c.center));
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](Point2D a, Point2D b) { return length(a) < length(b); });
    return candidates;
}

std::optional<Point2D> BasisFinder::secondDirection(Point2D u, const std::vector<Point2D>& directions) const {
    if (const std::optional<Point2D> v = fromDirections(u, directions)) return v;
    return betweenRows(u);
}

// 取「與 u 不平行、列距最小」的方向，對 u 取餘。列距至少 0.8·|u|：u 是最短的格距時，真正的列距 ≥ 0.87·|u|；
// 更小的是別的陣列的倍數向量湊出來的（例如一排 25 px 間距旁邊另一排的 2 倍間距 60 px），不要。
std::optional<Point2D> BasisFinder::fromDirections(Point2D u, const std::vector<Point2D>& directions) {
    const double lu = length(u);
    const double minSpacing = 0.8 * lu;
    std::optional<Point2D> best;
    double bestSpacing = 0.0;
    for (const Point2D& d : directions) {
        const double spacing = std::abs(cross(u, d)) / lu;
        if (spacing < minSpacing) continue;  // 和 u 平行（或就是 u），或列距不可能
        if (!best || spacing < bestSpacing - 1e-9 * lu || (std::abs(spacing - bestSpacing) <= 1e-9 * lu && length(d) < length(*best))) {
            best = d;
            bestSpacing = spacing;
        }
    }
    if (!best) return std::nullopt;
    const Point2D v = cross(u, *best) < 0.0 ? times(*best, -1.0) : *best;
    return minus(v, times(u, std::round(dot(v, u) / dot(u, u))));
}

// 沿 u 把點分成列（垂直 u 的座標 t 相近的點），相鄰兩列之間的位移向量對 u 取餘（Lagrange 約化）後分群。
// 一個群「指向幾個不同的點」才是它的票數：一整排點都指向同一個離群點時，取餘後的向量完全相同，但只算 1 票。
std::optional<Point2D> BasisFinder::betweenRows(Point2D u) const {
    const double lu = length(u);
    const Point2D uh = times(u, 1.0 / lu);
    const Point2D nh{-uh.y, uh.x};   // cross(uh, nh) > 0：u 朝右時 nh 朝下
    const double rowGap = 0.4 * lu;  // 同一列的點 t 差不到這個值；約化的基底保證相鄰兩列至少差 0.87·|u|
    const std::size_t n = pts_.size();

    // (t, 索引) 排序後切成列；每一列再換成 (s, 索引) 依 s 排序。用成對的陣列排序：比較時不必回頭查表。
    std::vector<std::pair<double, std::uint32_t>> byT(n);
    for (std::size_t k = 0; k < n; ++k) byT[k] = {dot(pts_[k], nh), static_cast<std::uint32_t>(k)};
    std::sort(byT.begin(), byT.end());
    std::vector<std::size_t> rowStart{0};
    for (std::size_t k = 1; k < n; ++k) {
        if (byT[k].first - byT[k - 1].first > rowGap) rowStart.push_back(k);
    }
    rowStart.push_back(n);
    const std::size_t rows = rowStart.size() - 1;
    if (rows < 2) return std::nullopt;  // 只有一列：一維
    std::vector<std::pair<double, std::uint32_t>> byS(n);
    for (std::size_t k = 0; k < n; ++k) byS[k] = {dot(pts_[byT[k].second], uh), byT[k].second};
    for (std::size_t r = 0; r < rows; ++r) {
        std::sort(byS.begin() + static_cast<std::ptrdiff_t>(rowStart[r]), byS.begin() + static_cast<std::ptrdiff_t>(rowStart[r + 1]));
    }

    constexpr std::size_t kSamplesPerRow = 16;
    std::vector<Point2D> vectors;
    std::vector<std::uint32_t> targets;
    vectors.reserve(2 * rows * kSamplesPerRow + 16);
    targets.reserve(2 * rows * kSamplesPerRow + 16);
    for (std::size_t r = 0; r + 1 < rows; ++r) {
        const std::size_t fromBegin = rowStart[r];
        const std::size_t fromSize = rowStart[r + 1] - fromBegin;
        const std::size_t step = std::max<std::size_t>(1, fromSize / kSamplesPerRow);
        for (std::size_t r2 = r + 1; r2 <= std::min(r + 2, rows - 1); ++r2) {  // 下一列與下下一列：中間夾一個離群點時仍配得到
            const auto toBegin = byS.begin() + static_cast<std::ptrdiff_t>(rowStart[r2]);
            const auto toEnd = byS.begin() + static_cast<std::ptrdiff_t>(rowStart[r2 + 1]);
            for (std::size_t a = 0; a < fromSize; a += step) {
                const auto& from = byS[fromBegin + a];
                // 另一列裡沿 u 方向最接近的點（二分搜尋）。
                auto it = std::lower_bound(toBegin, toEnd, from.first,
                                           [](const std::pair<double, std::uint32_t>& e, double value) { return e.first < value; });
                auto best = it == toEnd ? toEnd - 1 : it;
                if (it != toBegin && (it == toEnd || std::abs((it - 1)->first - from.first) <= std::abs(it->first - from.first)))
                    best = it - 1;
                Point2D d = minus(pts_[best->second], pts_[from.second]);
                d = minus(d, times(u, static_cast<double>(fastRound(dot(d, uh) / lu))));
                vectors.push_back(d);
                targets.push_back(best->second);
            }
        }
    }
    if (vectors.empty()) return std::nullopt;

    const VectorClusters clusters = clustering_.cluster(vectors, lu);
    // 每一群指向幾個不同的點：(群, 目標) 打包成一個 64 位元的鍵，排序後數不重複的。
    std::vector<std::uint64_t> pointed;
    pointed.reserve(vectors.size());
    for (std::size_t k = 0; k < vectors.size(); ++k) {
        if (clusters.labels[k] >= 0) pointed.push_back((static_cast<std::uint64_t>(clusters.labels[k]) << 32) | targets[k]);
    }
    std::sort(pointed.begin(), pointed.end());
    pointed.erase(std::unique(pointed.begin(), pointed.end()), pointed.end());
    std::vector<std::size_t> distinct(clusters.clusters.size(), 0);
    for (const std::uint64_t key : pointed) ++distinct[static_cast<std::size_t>(key >> 32)];
    const std::size_t most = distinct.empty() ? 0 : *std::max_element(distinct.begin(), distinct.end());

    std::optional<Point2D> best;
    double bestSpacing = 0.0;
    for (std::size_t c = 0; c < clusters.clusters.size(); ++c) {
        if (distinct[c] < 2 || static_cast<double>(distinct[c]) < o_.supportFraction * static_cast<double>(most)) continue;
        const Point2D center = clusters.clusters[c].center;
        const double spacing = cross(u, center) / lu;  // 列距；依建構方式一定是正的
        if (spacing < rowGap) continue;
        if (!best || spacing < bestSpacing - 1e-9 * lu || (std::abs(spacing - bestSpacing) <= 1e-9 * lu && length(center) < length(*best))) {
            best = center;
            bestSpacing = spacing;
        }
    }
    if (!best) return std::nullopt;
    return minus(*best, times(u, std::round(dot(*best, uh) / lu)));
}

}  // namespace aoi::detail
