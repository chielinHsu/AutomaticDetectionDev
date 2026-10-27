// AOI_DOMAIN_IMPL=cpp：轉呼叫 C++ 的實作（aoi::legacy，lattice/legacy/lattice_estimator_legacy.cpp）。
// AOI_DOMAIN_IMPL=rust 時編的是 lattice_estimator_rs.cpp（ADR-031）。
#include "domain/lattice/lattice_estimator.h"

#include <algorithm>
#include <utility>

#include "domain/lattice/legacy/lattice_estimator_legacy.h"

namespace aoi {

LatticeEstimator::LatticeEstimator(LatticeOptions options, std::shared_ptr<const IVectorClustering> clustering)
    : options_(options), clustering_(clustering ? std::move(clustering) : std::make_shared<GridVoteClustering>()) {}

std::optional<LatticeModel> LatticeEstimator::fit(std::span<const Point2D> points) const {
    LatticeFit f = run(points, 1);
    if (f.models.empty()) return std::nullopt;
    return std::move(f.models.front());
}

LatticeFit LatticeEstimator::fitAll(std::span<const Point2D> points) const {
    return run(points, std::max<std::size_t>(options_.maxLattices, 1));
}

LatticeFit LatticeEstimator::run(std::span<const Point2D> points, std::size_t maxModels) const {
    return legacy::fitLattices(options_, *clustering_, points, maxModels);
}

}  // namespace aoi
