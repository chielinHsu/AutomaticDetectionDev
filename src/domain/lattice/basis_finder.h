#pragma once

#include <optional>
#include <span>
#include <vector>

#include "domain/geometry.h"
#include "domain/lattice/lattice_estimator.h"
#include "domain/lattice/vector_clustering.h"

// domain 層：只用 C++ 標準庫。晶格估計的第一步（LatticeEstimator 內部用）：從點找出基底向量的候選。
namespace aoi::detail {

class BasisFinder {
public:
    // minDistance：比這個短的位移向量當成重疊（沒有方向）。points 要活得比它久。
    BasisFinder(const LatticeOptions& options, const IVectorClustering& clustering, std::span<const Point2D> points, double minDistance);

    // 方向的候選：每個點（抽樣）到最近 neighbors 個鄰居的位移向量分群後，票數至少是最高票 supportFraction 的群，
    // 由短到長。第一個就是 u 的首選。
    std::vector<Point2D> directions() const;

    // u 之外的第二個方向 v（列距最小的），已對 u 取餘、cross(u, v) > 0。沒有（只有一列）就回 nullopt：一維。
    //  1. 一般的陣列（兩個方向的格距差不到 2 倍）：v 已經在 directions 裡（最近的鄰居是 ±u、±v）。
    //  2. 找不到時（例如間距差 20 倍，最近的鄰居都在同一列）：沿 u 把點分成列，看相鄰兩列之間的位移向量。
    std::optional<Point2D> secondDirection(Point2D u, const std::vector<Point2D>& directions) const;

private:
    static std::optional<Point2D> fromDirections(Point2D u, const std::vector<Point2D>& directions);
    std::optional<Point2D> betweenRows(Point2D u) const;

    const LatticeOptions& o_;
    const IVectorClustering& clustering_;
    std::span<const Point2D> pts_;
    double minDistance_;
};

}  // namespace aoi::detail
