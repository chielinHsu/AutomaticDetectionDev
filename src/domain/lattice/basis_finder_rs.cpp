// AOI_DOMAIN_IMPL=rust：轉呼叫 Rust 的實作（rust/aoi_domain_rs，C ABI 在 domain/ffi/aoi_domain_rs.h；ADR-031）。
// 私有的 fromDirections、betweenRows 在 Rust 那一側（secondDirection 一次呼叫），這裡不定義（沒有人呼叫）。
#include "domain/lattice/basis_finder.h"

#include <memory>

#include "domain/lattice/lattice_rs_bridge.h"

namespace aoi::detail {

BasisFinder::BasisFinder(const LatticeOptions& options, const IVectorClustering& clustering, std::span<const Point2D> points,
                         double minDistance)
    : o_(options), clustering_(clustering), pts_(points), minDistance_(minDistance) {}

std::vector<Point2D> BasisFinder::directions() const {
    rs::checkAbi();
    const AoiRsLatticeOptions o = rs::toRs(o_);
    const rs::ClusteringBridge bridge(clustering_);
    const std::vector<AoiRsPoint> pts = rs::toRs(pts_);
    AoiRsPointList* handle = nullptr;
    bridge.check(aoi_rs_basis_directions(&o, bridge.spec(), pts.data(), pts.size(), minDistance_, &handle), "aoi_rs_basis_directions");
    const std::unique_ptr<AoiRsPointList, void (*)(AoiRsPointList*)> owner(handle, &aoi_rs_point_list_free);
    const AoiRsPoint* data = nullptr;
    std::size_t count = 0;
    rs::check(aoi_rs_point_list_get(handle, &data, &count), "aoi_rs_point_list_get");
    std::vector<Point2D> out;
    out.reserve(count);
    for (std::size_t k = 0; k < count; ++k) out.push_back(rs::fromRs(data[k]));
    return out;
}

std::optional<Point2D> BasisFinder::secondDirection(Point2D u, const std::vector<Point2D>& directions) const {
    rs::checkAbi();
    const AoiRsLatticeOptions o = rs::toRs(o_);
    const rs::ClusteringBridge bridge(clustering_);
    const std::vector<AoiRsPoint> pts = rs::toRs(pts_);
    const std::vector<AoiRsPoint> dirs = rs::toRs(directions);
    AoiRsPoint v{};
    int32_t found = 0;
    bridge.check(aoi_rs_basis_second_direction(&o, bridge.spec(), pts.data(), pts.size(), minDistance_, rs::toRs(u), dirs.data(),
                                               dirs.size(), &v, &found),
                 "aoi_rs_basis_second_direction");
    if (found == 0) return std::nullopt;
    return rs::fromRs(v);
}

}  // namespace aoi::detail
