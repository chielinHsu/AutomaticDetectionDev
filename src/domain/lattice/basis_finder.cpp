// AOI_DOMAIN_IMPL=cpp：轉呼叫 C++ 的實作（aoi::legacy，lattice/legacy/basis_finder_legacy.cpp）。
// AOI_DOMAIN_IMPL=rust 時編的是 basis_finder_rs.cpp（ADR-031）。私有的 fromDirections、betweenRows 不再定義（沒有人呼叫）。
#include "domain/lattice/basis_finder.h"

#include "domain/lattice/legacy/basis_finder_legacy.h"

namespace aoi::detail {

BasisFinder::BasisFinder(const LatticeOptions& options, const IVectorClustering& clustering, std::span<const Point2D> points,
                         double minDistance)
    : o_(options), clustering_(clustering), pts_(points), minDistance_(minDistance) {}

std::vector<Point2D> BasisFinder::directions() const { return legacy::BasisFinder(o_, clustering_, pts_, minDistance_).directions(); }

std::optional<Point2D> BasisFinder::secondDirection(Point2D u, const std::vector<Point2D>& directions) const {
    return legacy::BasisFinder(o_, clustering_, pts_, minDistance_).secondDirection(u, directions);
}

}  // namespace aoi::detail
