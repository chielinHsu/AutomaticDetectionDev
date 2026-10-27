// AOI_DOMAIN_IMPL=cpp：轉呼叫 C++ 的實作（aoi::legacy，lattice/legacy/cell_components_legacy.cpp）。
// AOI_DOMAIN_IMPL=rust 時編的是 cell_components_rs.cpp（ADR-031）。
#include "domain/lattice/cell_components.h"

#include "domain/lattice/legacy/cell_components_legacy.h"

namespace aoi::detail {

std::vector<std::size_t> largestComponent(const std::vector<LatticeIndex>& cells, int link, bool twoDimensional) {
    return legacy::largestComponent(cells, link, twoDimensional);
}

}  // namespace aoi::detail
