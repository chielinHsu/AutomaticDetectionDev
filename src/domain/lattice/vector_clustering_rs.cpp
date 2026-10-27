// AOI_DOMAIN_IMPL=rust：轉呼叫 Rust 的實作（rust/aoi_domain_rs，C ABI 在 domain/ffi/aoi_domain_rs.h；ADR-031）。
// 內建的兩個策略用列舉＋參數交給 Rust（lattice_rs_bridge.h 的 ClusteringBridge）。
#include "domain/lattice/vector_clustering.h"

#include "domain/lattice/lattice_rs_bridge.h"

namespace aoi {

namespace {

VectorClusters clusterInRust(const IVectorClustering& strategy, std::span<const Point2D> vectors, double scale) {
    rs::checkAbi();
    const rs::ClusteringBridge bridge(strategy);
    const std::vector<AoiRsPoint> in = rs::toRs(vectors);
    std::vector<AoiRsVectorCluster> clusters(in.size());
    std::vector<int32_t> labels(in.size());
    std::size_t count = 0;
    bridge.check(aoi_rs_cluster_vectors(bridge.spec(), in.data(), in.size(), scale, clusters.data(), &count, labels.data()),
                 "aoi_rs_cluster_vectors");
    VectorClusters out;
    out.clusters.reserve(count);
    for (std::size_t c = 0; c < count; ++c) out.clusters.push_back({rs::fromRs(clusters[c].center), clusters[c].members});
    out.labels.assign(labels.begin(), labels.end());
    return out;
}

}  // namespace

VectorClusters GridVoteClustering::cluster(std::span<const Point2D> vectors, double scale) const {
    return clusterInRust(*this, vectors, scale);
}

VectorClusters GreedyClustering::cluster(std::span<const Point2D> vectors, double scale) const {
    return clusterInRust(*this, vectors, scale);
}

}  // namespace aoi
