#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "domain/geometry.h"

// domain 層：只用 C++ 標準庫。
//
// 均勻格子的空間索引（2D 點）：找最近的 k 個鄰居，期望成本與點數無關（O(1)／次）。
// 取代 S03 暫代版的「每個點把其餘點的距離全算一遍」（O(n²)：4 900 點 97–134 ms）。
// 只存索引，不複製點；points 要活得比它久。
namespace aoi {

class SpatialGrid {
public:
    struct Neighbor {
        std::size_t index = 0;
        double distance = 0.0;
    };

    // cellSize ≤ 0 或不是有限值時，自己依點的範圍與點數決定（平均每格約一點）。格子數最多約 4n，太多時放大格子。
    SpatialGrid(std::span<const Point2D> points, double cellSize);
    ~SpatialGrid();
    SpatialGrid(SpatialGrid&&) noexcept;
    SpatialGrid& operator=(SpatialGrid&&) noexcept;

    // points[self] 最近的 k 個鄰居（距離 > minDistance：重疊的點不算鄰居），由近到遠寫進 out（先清空）。
    void nearest(std::size_t self, std::size_t k, double minDistance, std::vector<Neighbor>& out) const;

    double cellSize() const noexcept;

private:
    // R05.2（ADR-031）：實作在 AOI_DOMAIN_IMPL 選的那一份（C++：lattice/legacy/spatial_grid_legacy.cpp；
    // Rust：rust/aoi_domain_rs 的 handle，自己保存一份點）。兩種建置的標頭相同；因此不可複製、可以移動。
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace aoi
