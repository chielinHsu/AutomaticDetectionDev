// AOI_DOMAIN_IMPL=rust：轉呼叫 Rust 的實作（rust/aoi_domain_rs，C ABI 在 domain/ffi/aoi_domain_rs.h；ADR-031）。
// 狀態（detail::LatticeSums）和 C 的 AoiRsLatticeSums 欄位一一相同（lattice_rs_bridge.cpp 逐欄 static_assert），
// 直接把它的位址交給 Rust，不複製。不配置記憶體；Rust 回報錯誤（只有 panic，是 bug）時 noexcept 的函式回安全值
// （狀態不變、不是二維、解不出、殘差 0）。
#include "domain/lattice/lattice_accumulator.h"

#include "domain/lattice/lattice_rs_bridge.h"

namespace aoi {

void LatticeAccumulator::accumulate(LatticeIndex cell, Point2D p, double weight) noexcept {
    (void)aoi_rs_lattice_sums_accumulate(rs::asRs(sums_), rs::toRs(cell), rs::toRs(p), weight);
}

bool LatticeAccumulator::twoDimensional() const noexcept {
    int32_t out = 0;
    return aoi_rs_lattice_sums_two_dimensional(rs::asRs(sums_), &out) == AOI_RS_OK && out != 0;
}

std::optional<LatticeBasis> LatticeAccumulator::solve() const noexcept {
    AoiRsBasis basis{};
    int32_t solved = 0;
    if (aoi_rs_lattice_sums_solve(rs::asRs(sums_), &basis, &solved) != AOI_RS_OK || solved == 0) return std::nullopt;
    return rs::fromRs(basis);
}

double LatticeAccumulator::residualSumOfSquares(const LatticeBasis& basis) const noexcept {
    const AoiRsBasis b = rs::toRs(basis);
    double out = 0.0;
    return aoi_rs_lattice_sums_residual_sum_of_squares(rs::asRs(sums_), &b, &out) == AOI_RS_OK ? out : 0.0;
}

}  // namespace aoi
