// AOI_DOMAIN_IMPL=cpp：轉呼叫 C++ 的實作（aoi::legacy，lattice/legacy/lattice_acceptance_legacy.cpp）。
// AOI_DOMAIN_IMPL=rust 時編的是 lattice_acceptance_rs.cpp（ADR-031）。
#include "domain/lattice/lattice_acceptance.h"

#include "domain/lattice/legacy/lattice_acceptance_legacy.h"

namespace aoi::detail {

bool twoDimensionalIsValid(const LatticeCandidate& c, std::span<const Point2D> points, const LatticeOptions& o) {
    return legacy::twoDimensionalIsValid(c, points, o);
}

bool enoughEvidence(const LatticeCandidate& c, std::span<const Point2D> points, const LatticeOptions& o) {
    return legacy::enoughEvidence(c, points, o);
}

bool holesAreBacked(const LatticeModel& m) { return legacy::holesAreBacked(m); }

double consistency(const LatticeModel& m, std::span<const Point2D> points) { return legacy::consistency(m, points); }

}  // namespace aoi::detail
