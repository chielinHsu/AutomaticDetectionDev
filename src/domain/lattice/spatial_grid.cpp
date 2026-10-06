#include "domain/lattice/spatial_grid.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace aoi {

SpatialGrid::SpatialGrid(std::span<const Point2D> points, double cellSize) : points_(points) {
    const std::size_t n = points.size();
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    for (const Point2D& p : points) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }
    if (n == 0) minX = minY = maxX = maxY = 0.0;
    const double w = maxX - minX;
    const double h = maxY - minY;

    double cell = cellSize;
    if (!(cell > 0.0) || !std::isfinite(cell)) {
        const double count = static_cast<double>(std::max<std::size_t>(n, 1));
        cell = std::max(std::sqrt(w * h / count), std::max(w, h) / count);  // 平均每格約一點；共線時取平均間距
        if (!(cell > 0.0) || !std::isfinite(cell)) cell = 1.0;
    }
    // 格子數上限約 4n：點的範圍很大、格子又很小時（例如兩群點相距很遠），放大格子，記憶體與建置成本維持 O(n)。
    const double maxCells = 4.0 * static_cast<double>(n) + 16.0;
    for (;;) {
        const double cx = std::floor(w / cell) + 1.0;
        const double cy = std::floor(h / cell) + 1.0;
        if (cx * cy <= maxCells) {
            nx_ = static_cast<int>(cx);
            ny_ = static_cast<int>(cy);
            break;
        }
        cell *= std::max(1.01, std::sqrt(cx * cy / maxCells));
    }
    cell_ = cell;
    x0_ = minX;
    y0_ = minY;

    // 計數排序：每格的點連續放在 items_ 裡。
    const std::size_t cells = static_cast<std::size_t>(nx_) * static_cast<std::size_t>(ny_);
    start_.assign(cells + 1, 0);
    std::vector<std::uint32_t> cellOf(n);
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t c = static_cast<std::size_t>(cellY(points[k].y)) * static_cast<std::size_t>(nx_) +
                              static_cast<std::size_t>(cellX(points[k].x));
        cellOf[k] = static_cast<std::uint32_t>(c);
        ++start_[c + 1];
    }
    for (std::size_t c = 0; c < cells; ++c) start_[c + 1] += start_[c];
    items_.resize(n);
    std::vector<std::uint32_t> fill(start_.begin(), start_.end() - 1);
    for (std::size_t k = 0; k < n; ++k) items_[fill[cellOf[k]]++] = static_cast<std::uint32_t>(k);
}

int SpatialGrid::cellX(double x) const noexcept {
    const double c = std::floor((x - x0_) / cell_);
    return static_cast<int>(std::clamp(c, 0.0, static_cast<double>(nx_ - 1)));
}

int SpatialGrid::cellY(double y) const noexcept {
    const double c = std::floor((y - y0_) / cell_);
    return static_cast<int>(std::clamp(c, 0.0, static_cast<double>(ny_ - 1)));
}

void SpatialGrid::nearest(std::size_t self, std::size_t k, double minDistance, std::vector<Neighbor>& out) const {
    out.clear();
    if (k == 0 || self >= points_.size()) return;
    const Point2D p = points_[self];
    const int cx = cellX(p.x);
    const int cy = cellY(p.y);
    const double min2 = minDistance * minDistance;

    // 比較的是距離的平方（不開根號）；找完才換回距離。
    const auto visit = [&](int gx, int gy) {
        if (gx < 0 || gy < 0 || gx >= nx_ || gy >= ny_) return;
        const std::size_t c = static_cast<std::size_t>(gy) * static_cast<std::size_t>(nx_) + static_cast<std::size_t>(gx);
        for (std::uint32_t s = start_[c]; s < start_[c + 1]; ++s) {
            const std::size_t q = items_[s];
            if (q == self) continue;
            const double dx = points_[q].x - p.x;
            const double dy = points_[q].y - p.y;
            const double d2 = dx * dx + dy * dy;
            if (!(d2 > min2)) continue;  // 重疊的點沒有方向
            if (out.size() == k && d2 >= out.back().distance) continue;
            const auto at = std::upper_bound(out.begin(), out.end(), d2,
                                             [](double value, const Neighbor& e) { return value < e.distance; });
            out.insert(at, Neighbor{q, d2});
            if (out.size() > k) out.pop_back();
        }
    };

    // 一圈一圈往外找（Chebyshev 距離 r 的那一圈格子）。第 r＋1 圈的點離 p 至少 r 個格子寬，
    // 所以第 k 近的距離已經 ≤ r·cell 時，外面不可能有更近的。
    const int maxRing = std::max(nx_, ny_);
    for (int r = 0; r <= maxRing; ++r) {
        if (r == 0) {
            visit(cx, cy);
        } else {
            for (int dx = -r; dx <= r; ++dx) {
                visit(cx + dx, cy - r);
                visit(cx + dx, cy + r);
            }
            for (int dy = -r + 1; dy <= r - 1; ++dy) {
                visit(cx - r, cy + dy);
                visit(cx + r, cy + dy);
            }
        }
        const double reach = static_cast<double>(r) * cell_;
        if (out.size() == k && out.back().distance <= reach * reach) break;
    }
    for (Neighbor& n : out) n.distance = std::sqrt(n.distance);
}

}  // namespace aoi
