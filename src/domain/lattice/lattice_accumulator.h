#pragma once

#include <cstddef>
#include <optional>

#include "domain/geometry.h"
#include "domain/lattice/lattice_basis.h"

// domain 層：只用 C++ 標準庫。
//
// 晶格的最小平方法，以「正規方程累加器」實作：模型是
//     x = ox + i·ux + j·vx，  y = oy + i·uy + j·vy      （每個座標軸 3 個參數，和 S00 示範頁的 fit() 相同）
// 一維（所有點的 j 相同）時每個軸 2 個參數（o、u），v 不存在。
//
// 只存 16 個總和，不存點：加一個點、拿掉一個點、求解都是 O(1)。
// 用途：估計器的精修、漸進驗證時「每找到一個就重新擬合」（S00 示範頁的 Refit while verifying），
// 以及 use case 在視窗驗證之後把新找到的元件加進去、修正推算位置。
//
// 數值穩定：影像座標可能上萬像素、索引可能上百，平方再加總會吃掉有效位數，所以總和都相對於一個參考點與參考格子
// （建構時指定；離資料越近越好，例如第一個點）。索引是整數，與索引有關的總和在 2^53 以內是精確的。
namespace aoi {

namespace detail {

// LatticeAccumulator 的狀態（R05.2）：參考點、參考格子、16 個總和。兩種實作直接對它操作（C++：lattice/legacy/
// lattice_accumulator_legacy.cpp 的自由函式；Rust：欄位順序和 C 的 AoiRsLatticeSums 相同，lattice_rs_bridge.cpp 逐欄檢查）。
struct LatticeSums {
    Point2D referencePoint;
    LatticeIndex referenceCell;
    long long count = 0;
    // 索引的總和（相對於參考格子；整數，精確）
    double n = 0.0, si = 0.0, sj = 0.0, sii = 0.0, sij = 0.0, sjj = 0.0;
    // 座標的總和（相對於參考點）
    double sx = 0.0, six = 0.0, sjx = 0.0, sxx = 0.0;
    double sy = 0.0, siy = 0.0, sjy = 0.0, syy = 0.0;
};

struct LatticeAccumulatorAccess;  // lattice_accumulator_access.h（兩種實作之間換狀態）

}  // namespace detail

class LatticeAccumulator {
public:
    LatticeAccumulator() noexcept = default;
    LatticeAccumulator(Point2D referencePoint, LatticeIndex referenceCell) noexcept : sums_{referencePoint, referenceCell} {}

    void add(LatticeIndex cell, Point2D p) noexcept { accumulate(cell, p, 1.0); }
    // 拿掉一個先前加過的點（同樣的格子與座標）。拿掉沒加過的點，結果沒有意義。
    void remove(LatticeIndex cell, Point2D p) noexcept { accumulate(cell, p, -1.0); }

    std::size_t count() const noexcept { return sums_.count > 0 ? static_cast<std::size_t>(sums_.count) : 0u; }

    // j 有變化而且 (i, j) 不共線：可以解出 v（二維）。
    bool twoDimensional() const noexcept;

    // 最小平方解。二維（twoDimensional()）時解 o、u、v；否則 i 有變化時解一維（o、u，hasV ＝ false）；
    // 少於 2 個點、或 i 沒有變化（所有點同一個 i：一維晶格要沿 i 編號）時回 nullopt。O(1)。
    std::optional<LatticeBasis> solve() const noexcept;

    // 殘差平方和 Σ|p − basis.at(cell)|²，從總和直接算（不回頭看點）。O(1)。
    // 有效位數取決於參考點：點離參考點越遠，相消誤差越大（spec.md「數值」一節）。
    double residualSumOfSquares(const LatticeBasis& basis) const noexcept;

    Point2D referencePoint() const noexcept { return sums_.referencePoint; }
    LatticeIndex referenceCell() const noexcept { return sums_.referenceCell; }

private:
    friend struct detail::LatticeAccumulatorAccess;

    void accumulate(LatticeIndex cell, Point2D p, double weight) noexcept;

    detail::LatticeSums sums_;
};

}  // namespace aoi
