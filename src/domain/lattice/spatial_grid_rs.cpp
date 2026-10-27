// AOI_DOMAIN_IMPL=rust：轉呼叫 Rust 的實作（rust/aoi_domain_rs，C ABI 在 domain/ffi/aoi_domain_rs.h；ADR-031）。
// Rust 的 handle 自己保存一份點（不借用 span）；誰配置誰釋放：解構時呼叫 aoi_rs_spatial_grid_free。
#include "domain/lattice/spatial_grid.h"

#include <algorithm>

#include "domain/lattice/lattice_rs_bridge.h"

namespace aoi {

struct SpatialGrid::Impl {
    AoiRsSpatialGrid* handle = nullptr;
    std::size_t count = 0;
    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    ~Impl() { aoi_rs_spatial_grid_free(handle); }
};

SpatialGrid::SpatialGrid(std::span<const Point2D> points, double cellSize) : impl_(std::make_unique<Impl>()) {
    rs::checkAbi();
    const std::vector<AoiRsPoint> pts = rs::toRs(points);
    impl_->count = pts.size();
    rs::check(aoi_rs_spatial_grid_new(pts.data(), pts.size(), cellSize, &impl_->handle), "aoi_rs_spatial_grid_new");
}

SpatialGrid::~SpatialGrid() = default;
SpatialGrid::SpatialGrid(SpatialGrid&&) noexcept = default;
SpatialGrid& SpatialGrid::operator=(SpatialGrid&&) noexcept = default;

void SpatialGrid::nearest(std::size_t self, std::size_t k, double minDistance, std::vector<Neighbor>& out) const {
    out.clear();
    if (!impl_) return;
    // 最多 min(k, 點數) 個鄰居（自己不算），所以這個容量一定夠。
    std::vector<AoiRsNeighbor> found(std::min(k, impl_->count));
    std::size_t n = 0;
    rs::check(aoi_rs_spatial_grid_nearest(impl_->handle, self, k, minDistance, found.data(), found.size(), &n), "aoi_rs_spatial_grid_nearest");
    out.reserve(n);
    for (std::size_t q = 0; q < n; ++q) out.push_back({found[q].index, found[q].distance});
}

double SpatialGrid::cellSize() const noexcept {
    double out = 1.0;
    return impl_ && aoi_rs_spatial_grid_cell_size(impl_->handle, &out) == AOI_RS_OK ? out : 1.0;
}

}  // namespace aoi
