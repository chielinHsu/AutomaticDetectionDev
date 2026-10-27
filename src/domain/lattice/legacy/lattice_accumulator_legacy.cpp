#include "domain/lattice/legacy/lattice_accumulator_legacy.h"

#include <algorithm>
#include <cmath>

namespace aoi::legacy {

// R05.2：原本是 LatticeAccumulator 的成員函式；為了讓公開的類別不必複製狀態，改成直接對 detail::LatticeSums 操作的
// 自由函式（運算一行沒改，只把成員 n_ 換成 sums.n）。

// 記號：S·· 是「乘上 n 的中心化二階動差」，例如 Sii ＝ n·Σi² − (Σi)² ＝ n²·Var(i)。
// 索引是整數，Sii、Sjj、Sij 在 2^53 以內都是精確的整數，所以「有沒有變化」可以用 ≥ 0.5 判斷，不必猜容差。

void accumulate(detail::LatticeSums& sums, LatticeIndex cell, Point2D p, double weight) noexcept {
    const double di = static_cast<double>(cell.i) - static_cast<double>(sums.referenceCell.i);
    const double dj = static_cast<double>(cell.j) - static_cast<double>(sums.referenceCell.j);
    const double x = p.x - sums.referencePoint.x;
    const double y = p.y - sums.referencePoint.y;
    sums.count += weight > 0.0 ? 1 : -1;
    sums.n += weight;
    sums.si += weight * di;
    sums.sj += weight * dj;
    sums.sii += weight * di * di;
    sums.sij += weight * di * dj;
    sums.sjj += weight * dj * dj;
    sums.sx += weight * x;
    sums.six += weight * di * x;
    sums.sjx += weight * dj * x;
    sums.sxx += weight * x * x;
    sums.sy += weight * y;
    sums.siy += weight * di * y;
    sums.sjy += weight * dj * y;
    sums.syy += weight * y * y;
}

bool twoDimensional(const detail::LatticeSums& sums) noexcept {
    if (sums.count < 3) return false;
    const double sii = sums.n * sums.sii - sums.si * sums.si;
    const double sjj = sums.n * sums.sjj - sums.sj * sums.sj;
    const double sij = sums.n * sums.sij - sums.si * sums.sj;
    if (sjj < 0.5 || sii < 0.5) return false;
    const double det = sii * sjj - sij * sij;
    // det 在數學上是整數；很大的值相乘會有捨入，所以也看相對大小（幾乎共線的索引當成一維）。
    return det > std::max(0.5, 1e-12 * sii * sjj);
}

std::optional<LatticeBasis> solve(const detail::LatticeSums& sums) noexcept {
    if (sums.count < 2) return std::nullopt;
    const double sii = sums.n * sums.sii - sums.si * sums.si;
    const double six = sums.n * sums.six - sums.si * sums.sx;
    const double siy = sums.n * sums.siy - sums.si * sums.sy;

    LatticeBasis basis;
    double ox = 0.0;
    double oy = 0.0;
    if (legacy::twoDimensional(sums)) {
        const double sjj = sums.n * sums.sjj - sums.sj * sums.sj;
        const double sij = sums.n * sums.sij - sums.si * sums.sj;
        const double sjx = sums.n * sums.sjx - sums.sj * sums.sx;
        const double sjy = sums.n * sums.sjy - sums.sj * sums.sy;
        const double det = sii * sjj - sij * sij;
        basis.hasV = true;
        basis.u = {(sjj * six - sij * sjx) / det, (sjj * siy - sij * sjy) / det};
        basis.v = {(sii * sjx - sij * six) / det, (sii * sjy - sij * siy) / det};
        ox = (sums.sx - basis.u.x * sums.si - basis.v.x * sums.sj) / sums.n;
        oy = (sums.sy - basis.u.y * sums.si - basis.v.y * sums.sj) / sums.n;
    } else {
        if (sii < 0.5) return std::nullopt;  // 所有點同一個 i：沒有方向
        basis.hasV = false;
        basis.u = {six / sii, siy / sii};
        basis.v = {0.0, 0.0};
        ox = (sums.sx - basis.u.x * sums.si) / sums.n;
        oy = (sums.sy - basis.u.y * sums.si) / sums.n;
    }
    // 中心化的原點換回影像座標：origin ＝ 參考點 ＋ o′ − ri·u − rj·v。
    const double ri = static_cast<double>(sums.referenceCell.i);
    const double rj = basis.hasV ? static_cast<double>(sums.referenceCell.j) : 0.0;
    basis.origin = {sums.referencePoint.x + ox - ri * basis.u.x - rj * basis.v.x,
                    sums.referencePoint.y + oy - ri * basis.u.y - rj * basis.v.y};
    if (!std::isfinite(basis.origin.x) || !std::isfinite(basis.origin.y) || !std::isfinite(basis.u.x) ||
        !std::isfinite(basis.u.y) || !std::isfinite(basis.v.x) || !std::isfinite(basis.v.y))
        return std::nullopt;
    return basis;
}

double residualSumOfSquares(const detail::LatticeSums& sums, const LatticeBasis& basis) noexcept {
    if (sums.count <= 0) return 0.0;
    const double ri = static_cast<double>(sums.referenceCell.i);
    const double rj = static_cast<double>(sums.referenceCell.j);
    const double vx = basis.hasV ? basis.v.x : 0.0;
    const double vy = basis.hasV ? basis.v.y : 0.0;
    // 中心化座標下的參數：x′ ＝ ox′ ＋ ux·di ＋ vx·dj。
    const double ox = basis.origin.x - sums.referencePoint.x + basis.u.x * ri + vx * rj;
    const double oy = basis.origin.y - sums.referencePoint.y + basis.u.y * ri + vy * rj;
    const auto axis = [&](double o, double u, double v, double s, double si, double sj, double ss) {
        const double cross = o * s + u * si + v * sj;
        const double quad = o * o * sums.n + u * u * sums.sii + v * v * sums.sjj + 2.0 * (o * u * sums.si + o * v * sums.sj + u * v * sums.sij);
        return ss - 2.0 * cross + quad;
    };
    const double rss = axis(ox, basis.u.x, vx, sums.sx, sums.six, sums.sjx, sums.sxx) + axis(oy, basis.u.y, vy, sums.sy, sums.siy, sums.sjy, sums.syy);
    return std::max(0.0, rss);
}

}  // namespace aoi::legacy
