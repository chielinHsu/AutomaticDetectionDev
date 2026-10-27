// AOI_DOMAIN_IMPL=cpp：轉呼叫 C++ 的實作（aoi::legacy，lattice/legacy/lattice_accumulator_legacy.cpp），直接對自己的狀態操作。
// AOI_DOMAIN_IMPL=rust 時編的是 lattice_accumulator_rs.cpp（ADR-031）。
#include "domain/lattice/lattice_accumulator.h"

#include "domain/lattice/legacy/lattice_accumulator_legacy.h"

namespace aoi {

void LatticeAccumulator::accumulate(LatticeIndex cell, Point2D p, double weight) noexcept { legacy::accumulate(sums_, cell, p, weight); }

bool LatticeAccumulator::twoDimensional() const noexcept { return legacy::twoDimensional(sums_); }

std::optional<LatticeBasis> LatticeAccumulator::solve() const noexcept { return legacy::solve(sums_); }

double LatticeAccumulator::residualSumOfSquares(const LatticeBasis& basis) const noexcept {
    return legacy::residualSumOfSquares(sums_, basis);
}

}  // namespace aoi
