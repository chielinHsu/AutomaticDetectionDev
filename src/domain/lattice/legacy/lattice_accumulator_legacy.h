#pragma once

#include <cstddef>
#include <optional>

#include "domain/geometry.h"
#include "domain/lattice/lattice_accumulator.h"
#include "domain/lattice/lattice_basis.h"

// domain 層：只用 C++ 標準庫。
//
// 正規方程累加器的 C++ 實作（S05 起的那一份，R05.2 從 lattice_accumulator.cpp 原樣搬過來，ADR-031）。
// 用在兩個地方：
//   - AOI_DOMAIN_IMPL=cpp 的建置：aoi_domain 編進它，公開的 LatticeAccumulator 轉呼叫這裡；
//   - AOI_DOMAIN_IMPL=rust 的建置：只有差異測試用的 aoi_domain_legacy 程式庫編進它（tests/domain/tst_domain_rs_diff 的對照組）。
// 演算法與規則的說明在 lattice_accumulator.h；這裡不要改行為（要改，Rust 那一份與差異測試一起改）。
// lattice/legacy/ 的其他檔案（晶格的每一部分）都照這個規則。
//
// 運算是直接對 detail::LatticeSums 操作的自由函式：公開的類別（cpp 建置）把自己的狀態交進來，不必複製
// （R05.2 先做了「每次換成 legacy 物件再換回來」的版本，逐點加入的量測 INC／FULL 慢了 2–4 倍）。
// legacy::LatticeAccumulator 是 legacy 的估計器用的類別，只是包住這些函式。
namespace aoi::legacy {

void accumulate(detail::LatticeSums& sums, LatticeIndex cell, Point2D p, double weight) noexcept;
bool twoDimensional(const detail::LatticeSums& sums) noexcept;
std::optional<LatticeBasis> solve(const detail::LatticeSums& sums) noexcept;
double residualSumOfSquares(const detail::LatticeSums& sums, const LatticeBasis& basis) noexcept;

class LatticeAccumulator {
public:
    LatticeAccumulator() noexcept = default;
    LatticeAccumulator(Point2D referencePoint, LatticeIndex referenceCell) noexcept : sums_{referencePoint, referenceCell} {}

    void add(LatticeIndex cell, Point2D p) noexcept { legacy::accumulate(sums_, cell, p, 1.0); }
    void remove(LatticeIndex cell, Point2D p) noexcept { legacy::accumulate(sums_, cell, p, -1.0); }

    std::size_t count() const noexcept { return sums_.count > 0 ? static_cast<std::size_t>(sums_.count) : 0u; }
    bool twoDimensional() const noexcept { return legacy::twoDimensional(sums_); }
    std::optional<LatticeBasis> solve() const noexcept { return legacy::solve(sums_); }
    double residualSumOfSquares(const LatticeBasis& basis) const noexcept { return legacy::residualSumOfSquares(sums_, basis); }

    Point2D referencePoint() const noexcept { return sums_.referencePoint; }
    LatticeIndex referenceCell() const noexcept { return sums_.referenceCell; }

    const detail::LatticeSums& sums() const noexcept { return sums_; }

private:
    detail::LatticeSums sums_;
};

}  // namespace aoi::legacy
