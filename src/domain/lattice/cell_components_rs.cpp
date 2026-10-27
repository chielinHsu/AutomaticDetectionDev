// AOI_DOMAIN_IMPL=rust：轉呼叫 Rust 的實作（rust/aoi_domain_rs，C ABI 在 domain/ffi/aoi_domain_rs.h；ADR-031）。
#include "domain/lattice/cell_components.h"

#include "domain/lattice/lattice_rs_bridge.h"

namespace aoi::detail {

std::vector<std::size_t> largestComponent(const std::vector<LatticeIndex>& cells, int link, bool twoDimensional) {
    rs::checkAbi();
    const std::vector<AoiRsCell> in = rs::toRs(cells);
    std::vector<std::size_t> out(in.size());
    std::size_t count = 0;
    rs::check(aoi_rs_lattice_largest_component(in.data(), in.size(), link, twoDimensional ? 1 : 0, out.data(), &count),
              "aoi_rs_lattice_largest_component");
    out.resize(count);
    return out;
}

}  // namespace aoi::detail
