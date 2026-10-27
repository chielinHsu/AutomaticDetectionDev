// AOI_DOMAIN_IMPL=cpp：轉呼叫 C++ 的實作（aoi::legacy，lattice/legacy/vector_clustering_legacy.cpp）。
// AOI_DOMAIN_IMPL=rust 時編的是 vector_clustering_rs.cpp（ADR-031）。
#include "domain/lattice/vector_clustering.h"

#include "domain/lattice/legacy/vector_clustering_legacy.h"

namespace aoi {

VectorClusters GridVoteClustering::cluster(std::span<const Point2D> vectors, double scale) const {
    return legacy::GridVoteClustering(binFraction_).cluster(vectors, scale);
}

VectorClusters GreedyClustering::cluster(std::span<const Point2D> vectors, double scale) const {
    return legacy::GreedyClustering(tolerance_).cluster(vectors, scale);
}

}  // namespace aoi
