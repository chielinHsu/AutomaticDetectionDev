#include "domain/lattice/lattice_region.h"

#include <cmath>

#include "domain/lattice/lattice_model.h"

namespace aoi::detail {

namespace {

// 一次走訪最多這麼多個格子（和預測器相同的防呆）。
constexpr double kMaxVisited = 16.0 * 1024.0 * 1024.0;

}  // namespace

LatticeRegion::LatticeRegion(const LatticeBasis& b, int minI, int maxI, int minJ, int maxJ, const std::vector<LatticeIndex>& cells)
    : b_(b), minI_(minI), maxI_(maxI), minJ_(b.hasV ? minJ : 0), maxJ_(b.hasV ? maxJ : 0) {
    if (!b.hasV || cells.empty()) return;
    const double uu = b.u.x * b.u.x + b.u.y * b.u.y;
    const double lu = std::sqrt(uu);
    if (!(lu > 0.0)) return;
    ux_ = b.u.x / lu;
    uy_ = b.u.y / lu;
    // 容差 ＝ 四分之一個最短格距（和內點的預設容差相同）：有雜訊時擬合出來的 v 會帶一點 u 方向的分量，矩形陣列缺角的
    // 格子投影會比內點的範圍多出幾個像素，不可以因此排除；交錯陣列多出來的格子至少差半個格距。
    const double rowSpacing = std::abs(b.u.x * b.v.y - b.u.y * b.v.x) / lu;
    eps_ = 0.25 * (rowSpacing < lu ? rowSpacing : lu);
    for (std::size_t k = 0; k < cells.size(); ++k) {
        const Point2D p = b.at(cells[k].i, cells[k].j);
        const double s = p.x * ux_ + p.y * uy_;
        const double t = p.y * ux_ - p.x * uy_;
        if (k == 0) {
            sMin_ = sMax_ = s;
            tMin_ = tMax_ = t;
        } else {
            if (s < sMin_) sMin_ = s;
            if (s > sMax_) sMax_ = s;
            if (t < tMin_) tMin_ = t;
            if (t > tMax_) tMax_ = t;
        }
    }
    // w：v、2v − u、2v + u 之中和 u 最接近垂直的（|cos| 最小；同樣時取前面的）。
    constexpr int kCandidates[3][2] = {{0, 1}, {-1, 2}, {1, 2}};
    std::size_t best = 0;
    double bestCos = 0.0;
    for (std::size_t k = 0; k < 3; ++k) {
        const double a = kCandidates[k][0];
        const double c = kCandidates[k][1];
        const double wx = a * b.u.x + c * b.v.x;
        const double wy = a * b.u.y + c * b.v.y;
        const double ww = wx * wx + wy * wy;
        if (!(ww > 0.0)) continue;
        const double cosine = std::abs(wx * b.u.x + wy * b.u.y) / (std::sqrt(ww) * lu);
        if (k == 0 || cosine < bestCos) {
            best = k;
            bestCos = cosine;
        }
    }
    wi_ = kCandidates[best][0];
    wj_ = kCandidates[best][1];
    occupied_.emplace(CellTable::of(cells));
    aligned_ = true;
}

bool LatticeRegion::occupied(int i, int j) const { return occupied_ && occupied_->get(i, j) >= 0; }

bool LatticeRegion::reaches(int i, int j, int di, int dj) const {
    for (int step = 1; step <= kSupportReach; ++step) {
        if (occupied(i + di * step, j + dj * step)) return true;
    }
    return false;
}

bool LatticeRegion::inExtent(int i, int j) const {
    const Point2D p = b_.at(i, j);
    const double s = p.x * ux_ + p.y * uy_;
    const double t = p.y * ux_ - p.x * uy_;
    return s >= sMin_ - eps_ && s <= sMax_ + eps_ && t >= tMin_ - eps_ && t <= tMax_ + eps_;
}

bool LatticeRegion::adjacentAny(int i, int j) const {
    for (int a = -1; a <= 1; ++a) {
        for (int c = -1; c <= 1; ++c) {
            if (a == 0 && c == 0) continue;
            if (occupied(i + a + c * wi_, j + c * wj_)) return true;
        }
    }
    return false;
}

bool LatticeRegion::contains(int i, int j) const {
    if (i < minI_ || i > maxI_ || j < minJ_ || j > maxJ_) return false;
    if (!aligned_) return true;
    if (occupied(i, j)) return true;
    if (!inExtent(i, j)) return false;
    return between(i, j, 1, 0) || between(i, j, wi_, wj_) || (adjacent(i, j, 1, 0) && adjacent(i, j, wi_, wj_));
}

std::size_t LatticeRegion::evidenceCount() const {
    if (!aligned_) return boxCount();
    const double w = static_cast<double>(maxI_) - minI_ + 1.0;
    const double h = static_cast<double>(maxJ_) - minJ_ + 1.0;
    if (w < 1.0 || h < 1.0 || w * h > kMaxVisited) return boxCount();
    std::size_t n = 0;
    for (int i = minI_; i <= maxI_; ++i) {
        for (int j = minJ_; j <= maxJ_; ++j) {
            if (contains(i, j) || (inExtent(i, j) && adjacentAny(i, j))) ++n;
        }
    }
    return n;
}

std::size_t LatticeRegion::boxCount() const noexcept {
    const auto w = static_cast<std::size_t>(maxI_ - minI_ + 1);
    const auto h = b_.hasV ? static_cast<std::size_t>(maxJ_ - minJ_ + 1) : std::size_t{1};
    return w * h;
}

std::size_t LatticeRegion::cellCount() const {
    if (!aligned_) return boxCount();
    const double w = static_cast<double>(maxI_) - minI_ + 1.0;
    const double h = static_cast<double>(maxJ_) - minJ_ + 1.0;
    if (w < 1.0 || h < 1.0 || w * h > kMaxVisited) return boxCount();
    std::size_t n = 0;
    for (int i = minI_; i <= maxI_; ++i) {
        for (int j = minJ_; j <= maxJ_; ++j) {
            if (contains(i, j)) ++n;
        }
    }
    return n;
}

}  // namespace aoi::detail

namespace aoi {

std::size_t LatticeModel::cellCount() const { return detail::LatticeRegion(basis, minI, maxI, minJ, maxJ, cells).cellCount(); }

}  // namespace aoi
