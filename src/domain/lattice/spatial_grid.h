#pragma once

#include <cstddef>
#include <cstdint>
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

    // points[self] 最近的 k 個鄰居（距離 > minDistance：重疊的點不算鄰居），由近到遠寫進 out（先清空）。
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

}  // namespace aoi
