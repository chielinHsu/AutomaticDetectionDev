#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "domain/geometry.h"
#include "domain/lattice/flat_map.h"
#include "domain/lattice/lattice_basis.h"

// domain 層：只用 C++ 標準庫。晶格模組內部共用的小工具（估計器、找基底、接受規則用），不是公開的型別。
namespace aoi::detail {

// 熱迴圈裡的小工具：std::hypot、std::lround、std::floor 在這個工具鏈上是函式呼叫，1 000 點的擬合會呼叫上萬次，
// 量到它們佔了大部分的時間（docs/steps/S05-lattice/perf.md），所以這裡用平方根與截斷自己算。像素座標不會溢位。
inline double length(Point2D v) { return std::sqrt(v.x * v.x + v.y * v.y); }
inline double dot(Point2D a, Point2D b) { return a.x * b.x + a.y * b.y; }
inline double cross(Point2D a, Point2D b) { return a.x * b.y - a.y * b.x; }
inline Point2D minus(Point2D a, Point2D b) { return {a.x - b.x, a.y - b.y}; }
inline Point2D times(Point2D a, double s) { return {a.x * s, a.y * s}; }
inline bool finitePoint(Point2D p) { return std::isfinite(p.x) && std::isfinite(p.y); }
inline double squaredDistance(Point2D a, Point2D b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// 四捨五入（遠離 0），超出 ±kMaxIndex 的夾住。與 LatticeBasis::roundIndex 只在恰好 .5 的邊界上可能差一格。
inline int fastRound(double a) {
    constexpr double kMax = LatticeBasis::kMaxIndex;
    if (!(a > -kMax)) return static_cast<int>(-kMax);  // 也擋掉 NaN
    if (!(a < kMax)) return static_cast<int>(kMax);
    return static_cast<int>(a < 0.0 ? a - 0.5 : a + 0.5);
}

// 把位移向量放到同一個半平面：A 看 B 與 B 看 A 是同一個向量。
inline Point2D halfPlane(Point2D d) {
    if (d.x < 0.0 || (d.x == 0.0 && d.y < 0.0)) return {-d.x, -d.y};
    return d;
}

inline int reach(LatticeIndex c) { return std::max(std::abs(c.i), std::abs(c.j)); }

// splitmix64：只用整數運算，各平台結果相同（不用 std 的分佈）。
class SplitMix64 {
public:
    explicit SplitMix64(std::uint64_t seed) : state_(seed) {}
    std::uint64_t next() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    std::size_t below(std::size_t n) { return static_cast<std::size_t>(next() % n); }

private:
    std::uint64_t state_;
};

struct Assigned {
    LatticeIndex cell;
    double residual2 = std::numeric_limits<double>::infinity();  // 到格點距離的平方
};

// 指派器：一組基底的反矩陣先算好，之後每個點只要幾次乘加就換成格子座標（擬合時每個點要指派好幾次）。
// 基底退化（u 長度為 0、或 u、v 平行）時，每個點的殘差都是無限大（不會被當成內點）。
class Assigner {
public:
    explicit Assigner(const LatticeBasis& b) : b_(b) {
        if (!b.hasV) {
            const double uu = dot(b.u, b.u);
            ok_ = uu > 0.0;
            if (ok_) {
                au_ = b.u.x / uu;
                bu_ = b.u.y / uu;
            }
            b_.v = {0.0, 0.0};
        } else {
            const double det = cross(b.u, b.v);
            ok_ = std::abs(det) > 1e-12 * (dot(b.u, b.u) + dot(b.v, b.v));
            if (ok_) {
                au_ = b.v.y / det;
                bu_ = -b.v.x / det;
                av_ = -b.u.y / det;
                bv_ = b.u.x / det;
            }
        }
    }

    // 連續的格子座標 (a, b)；一維時 b 是 0。
    Point2D coordinates(Point2D p) const {
        const double dx = p.x - b_.origin.x;
        const double dy = p.y - b_.origin.y;
        return {dx * au_ + dy * bu_, dx * av_ + dy * bv_};
    }

    Assigned operator()(Point2D p) const {
        if (!ok_) return {};
        const double dx = p.x - b_.origin.x;
        const double dy = p.y - b_.origin.y;
        const int i = fastRound(dx * au_ + dy * bu_);
        const int j = b_.hasV ? fastRound(dx * av_ + dy * bv_) : 0;
        const double ex = dx - i * b_.u.x - j * b_.v.x;
        const double ey = dy - i * b_.u.y - j * b_.v.y;
        return {{i, j}, ex * ex + ey * ey};
    }

private:
    LatticeBasis b_;
    bool ok_ = false;
    double au_ = 0.0, bu_ = 0.0, av_ = 0.0, bv_ = 0.0;
};

// 內點容差：最短格距的一個比例。二維時「最短格距」取 |u| 與列距（垂直 u 的方向上相鄰兩列的距離）的小者。
inline double toleranceFor(const LatticeBasis& b, double fraction) {
    const double lu = length(b.u);
    if (!(lu > 0.0)) return 0.0;
    if (!b.hasV) return fraction * lu;
    const double rowSpacing = std::abs(cross(b.u, b.v)) / lu;
    return fraction * std::min(lu, rowSpacing);
}

// 最小平方法的解換進目前的基底：假設是二維、但這次的點只在一列上（解不出 v）時，保留原本的 v。
inline LatticeBasis mergeSolution(const LatticeBasis& current, const LatticeBasis& solved) {
    if (current.hasV && !solved.hasV) {
        LatticeBasis b = current;
        b.origin = solved.origin;
        b.u = solved.u;
        return b;
    }
    return solved;
}

// 一個假設（一組基底）擬合出來的候選：成員是點的索引，cells 與它一一對應，每格最多一個成員。
struct LatticeCandidate {
    LatticeBasis basis;
    double tolerance = 0.0;
    std::vector<std::size_t> members;
    std::vector<LatticeIndex> cells;
};

// 格子 → 整數的表（−1 ＝ 空）。索引範圍不大時用密集陣列（直接定址，比雜湊表快一個數量級），
// 範圍很大（有離得很遠的點）時用雜湊表。範圍外的格子 get 回 −1；set 只能用範圍內的格子。
class CellTable {
public:
    CellTable(int minI, int maxI, int minJ, int maxJ, std::size_t expected)
        : minI_(minI), maxI_(maxI), minJ_(minJ), maxJ_(maxJ), sparse_(1) {
        const double w = static_cast<double>(maxI) - minI + 1.0;
        const double h = static_cast<double>(maxJ) - minJ + 1.0;
        dense_ = w * h <= 64.0 * static_cast<double>(expected) + 4096.0;
        if (dense_) {
            width_ = static_cast<std::size_t>(w);
            values_.assign(static_cast<std::size_t>(w * h), -1);
        } else {
            sparse_.reset(expected);
        }
    }

    // cells 的每一格放它在 cells 裡的索引。
    static CellTable of(const std::vector<LatticeIndex>& cells) {
        int minI = 0, maxI = 0, minJ = 0, maxJ = 0;
        if (!cells.empty()) {
            minI = maxI = cells.front().i;
            minJ = maxJ = cells.front().j;
        }
        for (const LatticeIndex& c : cells) {
            minI = std::min(minI, c.i);
            maxI = std::max(maxI, c.i);
            minJ = std::min(minJ, c.j);
            maxJ = std::max(maxJ, c.j);
        }
        CellTable t(minI, maxI, minJ, maxJ, cells.size());
        for (std::size_t k = 0; k < cells.size(); ++k) t.set(cells[k].i, cells[k].j, static_cast<std::int32_t>(k));
        return t;
    }

    std::int32_t get(int i, int j) const {
        if (i < minI_ || i > maxI_ || j < minJ_ || j > maxJ_) return -1;
        if (dense_) return values_[index(i, j)];
        const std::int32_t* v = sparse_.find(packKey(i, j));
        return v == nullptr ? -1 : *v - 1;
    }

    void set(int i, int j, std::int32_t value) {
        if (dense_) values_[index(i, j)] = value;
        else sparse_[packKey(i, j)] = value + 1;
    }

private:
    std::size_t index(int i, int j) const {
        return static_cast<std::size_t>(j - minJ_) * width_ + static_cast<std::size_t>(i - minI_);
    }

    int minI_, maxI_, minJ_, maxJ_;
    bool dense_ = true;
    std::size_t width_ = 1;
    std::vector<std::int32_t> values_;
    FlatMap<std::int32_t> sparse_;
};

}  // namespace aoi::detail
