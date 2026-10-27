// AOI_DOMAIN_IMPL=cpp：轉呼叫 C++ 的實作（aoi::legacy，lattice/legacy/spatial_grid_legacy.cpp）。
// AOI_DOMAIN_IMPL=rust 時編的是 spatial_grid_rs.cpp（ADR-031）。
#include "domain/lattice/spatial_grid.h"

#include "domain/lattice/legacy/spatial_grid_legacy.h"

namespace aoi {

struct SpatialGrid::Impl {
    legacy::SpatialGrid grid;
};

SpatialGrid::SpatialGrid(std::span<const Point2D> points, double cellSize)
    : impl_(std::make_unique<Impl>(Impl{legacy::SpatialGrid(points, cellSize)})) {}

SpatialGrid::~SpatialGrid() = default;
SpatialGrid::SpatialGrid(SpatialGrid&&) noexcept = default;
SpatialGrid& SpatialGrid::operator=(SpatialGrid&&) noexcept = default;

void SpatialGrid::nearest(std::size_t self, std::size_t k, double minDistance, std::vector<Neighbor>& out) const {
    if (!impl_) {
        out.clear();
        return;
    }
    impl_->grid.nearest(self, k, minDistance, out);
}

double SpatialGrid::cellSize() const noexcept { return impl_ ? impl_->grid.cellSize() : 1.0; }

}  // namespace aoi
