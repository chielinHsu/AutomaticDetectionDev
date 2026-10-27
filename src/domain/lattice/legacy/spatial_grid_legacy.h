#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "domain/geometry.h"
#include "domain/lattice/spatial_grid.h"

// domain 層：只用 C++ 標準庫。
//
// 空間格子的 C++ 實作（S05 起的那一份，R05.2 從 spatial_grid.cpp 原樣搬過來，ADR-031）。用在哪裡、不要改行為的規則
// 見 lattice_accumulator_legacy.h。說明在 spatial_grid.h。Neighbor 用公開的型別（兩邊的輸出直接比）。
namespace aoi::legacy {

class SpatialGrid {
public:
    using Neighbor = aoi::SpatialGrid::Neighbor;

    SpatialGrid(std::span<const Point2D> points, double cellSize);
    void nearest(std::size_t self, std::size_t k, double minDistance, std::vector<Neighbor>& out) const;
    double cellSize() const noexcept { return cell_; }

private:
    int cellX(double x) const noexcept;
    int cellY(double y) const noexcept;

    std::span<const Point2D> points_;
    double cell_ = 1.0;
    double x0_ = 0.0;
    double y0_ = 0.0;
    int nx_ = 1;
    int ny_ = 1;
    std::vector<std::uint32_t> start_;  // 每格在 items_ 的起點（長度 nx·ny ＋ 1）
    std::vector<std::uint32_t> items_;  // 依格子排好的點索引
};

}  // namespace aoi::legacy
