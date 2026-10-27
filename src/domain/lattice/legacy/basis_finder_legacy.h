#pragma once

#include <optional>
#include <span>
#include <vector>

#include "domain/geometry.h"
#include "domain/lattice/lattice_estimator.h"
#include "domain/lattice/vector_clustering.h"

// domain 層：只用 C++ 標準庫。
//
// 找基底（detail::BasisFinder）的 C++ 實作（S05 起的那一份，R05.2 從 basis_finder.cpp 原樣搬過來，ADR-031）。
// 用在哪裡、不要改行為的規則見 lattice_accumulator_legacy.h。說明在 basis_finder.h。
namespace aoi::legacy {

class BasisFinder {
public:
    BasisFinder(const LatticeOptions& options, const IVectorClustering& clustering, std::span<const Point2D> points, double minDistance);

    std::vector<Point2D> directions() const;
    std::optional<Point2D> secondDirection(Point2D u, const std::vector<Point2D>& directions) const;

private:
    static std::optional<Point2D> fromDirections(Point2D u, const std::vector<Point2D>& directions);
    std::optional<Point2D> betweenRows(Point2D u) const;

    const LatticeOptions& o_;
    const IVectorClustering& clustering_;
    std::span<const Point2D> pts_;
    double minDistance_;
};

}  // namespace aoi::legacy
