// AOI_DOMAIN_IMPL=rust：轉呼叫 Rust 的實作（rust/aoi_domain_rs，C ABI 在 domain/ffi/aoi_domain_rs.h；ADR-031）。
// 整個估計一次呼叫（找基底、RANSAC、精修、連通、接受規則都在 Rust 那一側）；內建的分群策略用列舉，
// 其他的（測試注入的 CountingClustering 等）經回呼呼叫回 C++（lattice_rs_bridge.h 的 ClusteringBridge）。
#include "domain/lattice/lattice_estimator.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "domain/lattice/lattice_rs_bridge.h"

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
    rs::checkAbi();
    const AoiRsLatticeOptions o = rs::toRs(options_);
    const rs::ClusteringBridge bridge(*clustering_);
    const std::vector<AoiRsPoint> pts = rs::toRs(points);
    AoiRsLatticeFit* handle = nullptr;
    bridge.check(aoi_rs_lattice_fit(&o, bridge.spec(), pts.data(), pts.size(), maxModels, &handle), "aoi_rs_lattice_fit");
    const std::unique_ptr<AoiRsLatticeFit, void (*)(AoiRsLatticeFit*)> owner(handle, &aoi_rs_lattice_fit_free);

    int32_t reason = 0;
    double rejected = 0.0;
    std::size_t count = 0;
    rs::check(aoi_rs_lattice_fit_summary(handle, &reason, &rejected, &count), "aoi_rs_lattice_fit_summary");
    LatticeFit fit;
    fit.reason = static_cast<NoLatticeReason>(reason);
    fit.rejectedConfidence = rejected;
    fit.models.reserve(count);
    for (std::size_t k = 0; k < count; ++k) {
        AoiRsLatticeModel m{};
        rs::check(aoi_rs_lattice_fit_model(handle, k, &m), "aoi_rs_lattice_fit_model");
        fit.models.push_back(rs::toModel(m));
    }
    return fit;
}

}  // namespace aoi
