#include "domain/lattice/lattice_accumulator.h"

#include <algorithm>
#include <cmath>

namespace aoi {

// 記號：S·· 是「乘上 n 的中心化二階動差」，例如 Sii ＝ n·Σi² − (Σi)² ＝ n²·Var(i)。
// 索引是整數，Sii、Sjj、Sij 在 2^53 以內都是精確的整數，所以「有沒有變化」可以用 ≥ 0.5 判斷，不必猜容差。

void LatticeAccumulator::accumulate(LatticeIndex cell, Point2D p, double weight) noexcept {
    const double di = static_cast<double>(cell.i) - static_cast<double>(referenceCell_.i);
    const double dj = static_cast<double>(cell.j) - static_cast<double>(referenceCell_.j);
    const double x = p.x - referencePoint_.x;
    const double y = p.y - referencePoint_.y;
    count_ += weight > 0.0 ? 1 : -1;
    n_ += weight;
    si_ += weight * di;
    sj_ += weight * dj;
    sii_ += weight * di * di;
    sij_ += weight * di * dj;
    sjj_ += weight * dj * dj;
    sx_ += weight * x;
    six_ += weight * di * x;
    sjx_ += weight * dj * x;
    sxx_ += weight * x * x;
    sy_ += weight * y;
    siy_ += weight * di * y;
    sjy_ += weight * dj * y;
    syy_ += weight * y * y;
}

bool LatticeAccumulator::twoDimensional() const noexcept {
    if (count_ < 3) return false;
    const double sii = n_ * sii_ - si_ * si_;
    const double sjj = n_ * sjj_ - sj_ * sj_;
    const double sij = n_ * sij_ - si_ * sj_;
    if (sjj < 0.5 || sii < 0.5) return false;
    const double det = sii * sjj - sij * sij;
    // det 在數學上是整數；很大的值相乘會有捨入，所以也看相對大小（幾乎共線的索引當成一維）。
    return det > std::max(0.5, 1e-12 * sii * sjj);
}

std::optional<LatticeBasis> LatticeAccumulator::solve() const noexcept {
    if (count_ < 2) return std::nullopt;
    const double sii = n_ * sii_ - si_ * si_;
    const double six = n_ * six_ - si_ * sx_;
    const double siy = n_ * siy_ - si_ * sy_;

    LatticeBasis basis;
    double ox = 0.0;
    double oy = 0.0;
    if (twoDimensional()) {
        const double sjj = n_ * sjj_ - sj_ * sj_;
        const double sij = n_ * sij_ - si_ * sj_;
        const double sjx = n_ * sjx_ - sj_ * sx_;
        const double sjy = n_ * sjy_ - sj_ * sy_;
        const double det = sii * sjj - sij * sij;
        basis.hasV = true;
        basis.u = {(sjj * six - sij * sjx) / det, (sjj * siy - sij * sjy) / det};
        basis.v = {(sii * sjx - sij * six) / det, (sii * sjy - sij * siy) / det};
        ox = (sx_ - basis.u.x * si_ - basis.v.x * sj_) / n_;
        oy = (sy_ - basis.u.y * si_ - basis.v.y * sj_) / n_;
    } else {
        if (sii < 0.5) return std::nullopt;  // 所有點同一個 i：沒有方向
        basis.hasV = false;
        basis.u = {six / sii, siy / sii};
        basis.v = {0.0, 0.0};
        ox = (sx_ - basis.u.x * si_) / n_;
        oy = (sy_ - basis.u.y * si_) / n_;
    }
    // 中心化的原點換回影像座標：origin ＝ 參考點 ＋ o′ − ri·u − rj·v。
    const double ri = static_cast<double>(referenceCell_.i);
    const double rj = basis.hasV ? static_cast<double>(referenceCell_.j) : 0.0;
    basis.origin = {referencePoint_.x + ox - ri * basis.u.x - rj * basis.v.x,
                    referencePoint_.y + oy - ri * basis.u.y - rj * basis.v.y};
    if (!std::isfinite(basis.origin.x) || !std::isfinite(basis.origin.y) || !std::isfinite(basis.u.x) ||
        !std::isfinite(basis.u.y) || !std::isfinite(basis.v.x) || !std::isfinite(basis.v.y))
        return std::nullopt;
    return basis;
}

double LatticeAccumulator::residualSumOfSquares(const LatticeBasis& basis) const noexcept {
    if (count_ <= 0) return 0.0;
    const double ri = static_cast<double>(referenceCell_.i);
    const double rj = static_cast<double>(referenceCell_.j);
    const double vx = basis.hasV ? basis.v.x : 0.0;
    const double vy = basis.hasV ? basis.v.y : 0.0;
    // 中心化座標下的參數：x′ ＝ ox′ ＋ ux·di ＋ vx·dj。
    const double ox = basis.origin.x - referencePoint_.x + basis.u.x * ri + vx * rj;
    const double oy = basis.origin.y - referencePoint_.y + basis.u.y * ri + vy * rj;
    const auto axis = [&](double o, double u, double v, double s, double si, double sj, double ss) {
        const double cross = o * s + u * si + v * sj;
        const double quad = o * o * n_ + u * u * sii_ + v * v * sjj_ + 2.0 * (o * u * si_ + o * v * sj_ + u * v * sij_);
        return ss - 2.0 * cross + quad;
    };
    const double rss = axis(ox, basis.u.x, vx, sx_, six_, sjx_, sxx_) + axis(oy, basis.u.y, vy, sy_, siy_, sjy_, syy_);
    return std::max(0.0, rss);
}

}  // namespace aoi
