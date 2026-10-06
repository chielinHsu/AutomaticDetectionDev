#pragma once

#include <cmath>
#include <optional>

#include "domain/geometry.h"

// domain 層：只用 C++ 標準庫。晶格（元件陣列）的幾何：格子索引與基底。座標是影像像素，慣例見 geometry.h。
//
// 晶格：格子 (i, j) 的中心 ＝ origin + i·u + j·v。一維（一排）時 hasV ＝ false：只有 i，j 恆為 0，v 沒有意義。
// LatticeEstimator 回傳的基底是約化過的（Lagrange–Gauss）：u 是最短的格距向量，|u| ≤ |v|，
// v 在 u 方向的分量不超過 |u| 的一半，而且 cross(u, v) > 0（u 朝右時 v 朝下，和 S00 示範頁的 i、j 方向一致）。
// 約化的好處：任何離格點不到 0.43·|u| 的點，四捨五入 (a, b) 得到的就是最近的格點（spec.md 有推導）。
namespace aoi {

struct LatticeIndex {
    int i = 0;
    int j = 0;

    friend bool operator==(const LatticeIndex&, const LatticeIndex&) = default;
};

struct LatticeBasis {
    Point2D origin;
    Point2D u;
    Point2D v;
    bool hasV = false;

    Point2D at(int i, int j) const noexcept {
        if (!hasV) return {origin.x + i * u.x, origin.y + i * u.y};
        return {origin.x + i * u.x + j * v.x, origin.y + i * u.y + j * v.y};
    }
    Point2D at(LatticeIndex c) const noexcept { return at(c.i, c.j); }

    // 連續的格子座標 (a, b)：p ≈ origin + a·u + b·v。一維時 b 是 0（垂直方向的偏差留在殘差裡）。
    // 基底退化（u 長度為 0，或 u、v 平行）時回 nullopt。
    std::optional<Point2D> coordinates(Point2D p) const noexcept {
        const double dx = p.x - origin.x;
        const double dy = p.y - origin.y;
        if (!hasV) {
            const double uu = u.x * u.x + u.y * u.y;
            if (!(uu > 0.0)) return std::nullopt;
            return Point2D{(dx * u.x + dy * u.y) / uu, 0.0};
        }
        const double det = u.x * v.y - u.y * v.x;
        if (!(std::abs(det) > 1e-12 * (u.x * u.x + u.y * u.y + v.x * v.x + v.y * v.y))) return std::nullopt;
        return Point2D{(dx * v.y - dy * v.x) / det, (u.x * dy - u.y * dx) / det};
    }

    // 最近的格子（四捨五入）。基底退化時回 (0, 0)；離原點超過 kMaxIndex 格的點夾到 ±kMaxIndex（不會溢位）。
    static constexpr double kMaxIndex = 1.0e8;
    LatticeIndex nearestCell(Point2D p) const noexcept {
        const std::optional<Point2D> c = coordinates(p);
        if (!c) return {};
        return {roundIndex(c->x), hasV ? roundIndex(c->y) : 0};
    }

    static int roundIndex(double a) noexcept {
        if (!(a > -kMaxIndex)) return static_cast<int>(-kMaxIndex);  // 也擋掉 NaN
        if (!(a < kMaxIndex)) return static_cast<int>(kMaxIndex);
        return static_cast<int>(std::lround(a));
    }
};

}  // namespace aoi
