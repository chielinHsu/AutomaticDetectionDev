#pragma once

#include <span>

#include "domain/geometry.h"
#include "domain/lattice/vector_clustering.h"

// domain 層：只用 C++ 標準庫。
//
// 兩個內建分群策略的 C++ 實作（S05 起的那一份，R05.2 從 vector_clustering.cpp 原樣搬過來，ADR-031）。用在哪裡、
// 不要改行為的規則見 lattice_accumulator_legacy.h。說明與契約在 vector_clustering.h。
// 它們實作公開的 IVectorClustering：C++ 的估計器（aoi::legacy）注入它們，差異測試也拿它們當自訂策略的內層。
namespace aoi::legacy {

class GridVoteClustering final : public IVectorClustering {
public:
    explicit GridVoteClustering(double binFraction = 0.1) noexcept : binFraction_(binFraction) {}
    VectorClusters cluster(std::span<const Point2D> vectors, double scale) const override;

private:
    double binFraction_;
};

class GreedyClustering final : public IVectorClustering {
public:
    explicit GreedyClustering(double tolerance = 0.2) noexcept : tolerance_(tolerance) {}
    VectorClusters cluster(std::span<const Point2D> vectors, double scale) const override;

private:
    double tolerance_;
};

}  // namespace aoi::legacy
