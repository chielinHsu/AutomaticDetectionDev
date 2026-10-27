#include "domain/lattice/lattice_rs_bridge.h"

#include <cstddef>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace aoi::rs {

// C 的結構和 Rust 的 #[repr(C)] 一樣大（Rust 那一側也檢查同樣的數字，rust/aoi_domain_rs/src/ffi_lattice.rs）。
static_assert(sizeof(AoiRsPoint) == 16 && sizeof(AoiRsCell) == 8 && sizeof(AoiRsBasis) == 56, "AoiRs* must match #[repr(C)]");
static_assert(sizeof(AoiRsLatticeSums) == 16 + 8 + 8 + 14 * 8, "AoiRsLatticeSums must match #[repr(C)]");
static_assert(sizeof(AoiRsLatticeOptions) == 112, "AoiRsLatticeOptions must match #[repr(C)]");
static_assert(sizeof(AoiRsClustering) == 32 && sizeof(AoiRsVectorCluster) == 24, "AoiRsClustering must match #[repr(C)]");
static_assert(sizeof(AoiRsLatticeModel) == 296 && sizeof(AoiRsLatticeCandidate) == 96, "AoiRsLatticeModel must match #[repr(C)]");
static_assert(sizeof(AoiRsPredictedCell) == 24 && sizeof(AoiRsNeighbor) == 16, "AoiRsPredictedCell must match #[repr(C)]");
// asRs：detail::LatticeSums 與 AoiRsLatticeSums 逐欄同樣的位置與大小（累加器直接交出位址）。
static_assert(std::is_standard_layout_v<detail::LatticeSums> && sizeof(detail::LatticeSums) == sizeof(AoiRsLatticeSums), "LatticeSums must match AoiRsLatticeSums");
static_assert(offsetof(detail::LatticeSums, referencePoint) == offsetof(AoiRsLatticeSums, reference_point));
static_assert(offsetof(detail::LatticeSums, referenceCell) == offsetof(AoiRsLatticeSums, reference_cell));
static_assert(offsetof(detail::LatticeSums, count) == offsetof(AoiRsLatticeSums, count));
static_assert(offsetof(detail::LatticeSums, n) == offsetof(AoiRsLatticeSums, n));
static_assert(offsetof(detail::LatticeSums, si) == offsetof(AoiRsLatticeSums, si));
static_assert(offsetof(detail::LatticeSums, sj) == offsetof(AoiRsLatticeSums, sj));
static_assert(offsetof(detail::LatticeSums, sii) == offsetof(AoiRsLatticeSums, sii));
static_assert(offsetof(detail::LatticeSums, sij) == offsetof(AoiRsLatticeSums, sij));
static_assert(offsetof(detail::LatticeSums, sjj) == offsetof(AoiRsLatticeSums, sjj));
static_assert(offsetof(detail::LatticeSums, sx) == offsetof(AoiRsLatticeSums, sx));
static_assert(offsetof(detail::LatticeSums, six) == offsetof(AoiRsLatticeSums, six));
static_assert(offsetof(detail::LatticeSums, sjx) == offsetof(AoiRsLatticeSums, sjx));
static_assert(offsetof(detail::LatticeSums, sxx) == offsetof(AoiRsLatticeSums, sxx));
static_assert(offsetof(detail::LatticeSums, sy) == offsetof(AoiRsLatticeSums, sy));
static_assert(offsetof(detail::LatticeSums, siy) == offsetof(AoiRsLatticeSums, siy));
static_assert(offsetof(detail::LatticeSums, sjy) == offsetof(AoiRsLatticeSums, sjy));
static_assert(offsetof(detail::LatticeSums, syy) == offsetof(AoiRsLatticeSums, syy));
// NoLatticeReason 的數值是 C ABI 的一部分（aoi_rs_lattice_fit_summary）。
static_assert(static_cast<int>(NoLatticeReason::None) == 0 && static_cast<int>(NoLatticeReason::TooFewPoints) == 1 &&
                  static_cast<int>(NoLatticeReason::NoDirection) == 2 && static_cast<int>(NoLatticeReason::NoConsistentLattice) == 3 &&
                  static_cast<int>(NoLatticeReason::TooManyHoles) == 4 && static_cast<int>(NoLatticeReason::LowConfidence) == 5,
              "NoLatticeReason must match the Rust enum");

void checkAbi() {
    static const bool ok = aoi_rs_abi_version() == AOI_RS_ABI_VERSION;
    if (!ok) throw std::logic_error("aoi_domain_rs: ABI version mismatch (rebuild rust/aoi_domain_rs)");
}

void check(AoiRsStatus status, const char* what) {
    if (status == AOI_RS_OK) return;
    if (status == AOI_RS_NO_MEMORY) throw std::bad_alloc();
    const char* why = status == AOI_RS_PANIC ? " panicked" : status == AOI_RS_CALLBACK_FAILED ? " failed in the clustering callback"
                                                                                               : " rejected its arguments";
    throw std::logic_error(std::string("aoi_domain_rs: ") + what + why);
}

AoiRsBasis toRs(const LatticeBasis& b) noexcept {
    AoiRsBasis r{};
    r.origin = toRs(b.origin);
    r.u = toRs(b.u);
    r.v = toRs(b.v);
    r.has_v = b.hasV ? 1 : 0;
    return r;
}

LatticeBasis fromRs(const AoiRsBasis& b) noexcept {
    LatticeBasis r;
    r.origin = fromRs(b.origin);
    r.u = fromRs(b.u);
    r.v = fromRs(b.v);
    r.hasV = b.has_v != 0;
    return r;
}

AoiRsLatticeSums toRs(const detail::LatticeSums& s) noexcept {
    AoiRsLatticeSums r{};
    r.reference_point = toRs(s.referencePoint);
    r.reference_cell = toRs(s.referenceCell);
    r.count = s.count;
    r.n = s.n;
    r.si = s.si;
    r.sj = s.sj;
    r.sii = s.sii;
    r.sij = s.sij;
    r.sjj = s.sjj;
    r.sx = s.sx;
    r.six = s.six;
    r.sjx = s.sjx;
    r.sxx = s.sxx;
    r.sy = s.sy;
    r.siy = s.siy;
    r.sjy = s.sjy;
    r.syy = s.syy;
    return r;
}

detail::LatticeSums fromRs(const AoiRsLatticeSums& s) noexcept {
    detail::LatticeSums r;
    r.referencePoint = fromRs(s.reference_point);
    r.referenceCell = fromRs(s.reference_cell);
    r.count = s.count;
    r.n = s.n;
    r.si = s.si;
    r.sj = s.sj;
    r.sii = s.sii;
    r.sij = s.sij;
    r.sjj = s.sjj;
    r.sx = s.sx;
    r.six = s.six;
    r.sjx = s.sjx;
    r.sxx = s.sxx;
    r.sy = s.sy;
    r.siy = s.siy;
    r.sjy = s.sjy;
    r.syy = s.syy;
    return r;
}

AoiRsLatticeOptions toRs(const LatticeOptions& o) noexcept {
    AoiRsLatticeOptions r{};
    r.inlier_fraction = o.inlierFraction;
    r.neighbors = o.neighbors;
    r.max_samples = o.maxSamples;
    r.support_fraction = o.supportFraction;
    r.ransac_trials = o.ransacTrials;
    r.ransac_index_radius = o.ransacIndexRadius;
    r.max_gap_cells = o.maxGapCells;
    r.min_second_row_inliers = o.minSecondRowInliers;
    r.min_row_consistency = o.minRowConsistency;
    r.min_inliers = o.minInliers;
    r.min_confidence = o.minConfidence;
    r.check_below = o.checkBelow;
    r.max_lattices = o.maxLattices;
    r.min_extra_lattice_inliers = o.minExtraLatticeInliers;
    r.seed = o.seed;
    return r;
}

std::vector<AoiRsPoint> toRs(std::span<const Point2D> points) {
    std::vector<AoiRsPoint> out;
    out.reserve(points.size());
    for (const Point2D& p : points) out.push_back(toRs(p));
    return out;
}

std::vector<AoiRsCell> toRs(const std::vector<LatticeIndex>& cells) {
    std::vector<AoiRsCell> out;
    out.reserve(cells.size());
    for (const LatticeIndex& c : cells) out.push_back(toRs(c));
    return out;
}

ClusteringBridge::ClusteringBridge(const IVectorClustering& clustering) noexcept : clustering_(clustering) {
    // 只認得「正好是」內建的類別（兩個都是 final）；其他的一律用回呼。
    if (const auto* grid = dynamic_cast<const GridVoteClustering*>(&clustering)) {
        spec_.kind = AOI_RS_CLUSTER_GRID_VOTE;
        spec_.parameter = detail::ClusteringAccess::binFraction(*grid);
    } else if (const auto* greedy = dynamic_cast<const GreedyClustering*>(&clustering)) {
        spec_.kind = AOI_RS_CLUSTER_GREEDY;
        spec_.parameter = detail::ClusteringAccess::tolerance(*greedy);
    } else {
        spec_.kind = AOI_RS_CLUSTER_CALLBACK;
        spec_.callback = &ClusteringBridge::trampoline;
        spec_.context = this;
    }
}

void ClusteringBridge::check(AoiRsStatus status, const char* what) const {
    if (status == AOI_RS_CALLBACK_FAILED) {
        if (error_) std::rethrow_exception(std::exchange(error_, nullptr));
        throw std::logic_error(std::string("aoi_domain_rs: ") + what +
                               ": the IVectorClustering output breaks the contract (a label is not below the cluster count)");
    }
    rs::check(status, what);
}

AoiRsStatus ClusteringBridge::trampoline(void* context, const AoiRsPoint* vectors, std::size_t count, double scale,
                                         AoiRsVectorCluster* clusters, std::size_t* clusterCount, int32_t* labels) noexcept {
    auto* self = static_cast<ClusteringBridge*>(context);
    try {
        std::vector<Point2D> in;
        in.reserve(count);
        for (std::size_t k = 0; k < count; ++k) in.push_back(fromRs(vectors[k]));
        const VectorClusters out = self->clustering_.cluster(in, scale);
        if (out.clusters.size() > count || out.labels.size() != count)
            throw std::logic_error("IVectorClustering::cluster breaks the contract (more clusters than vectors, or labels.size() != vectors.size())");
        for (std::size_t c = 0; c < out.clusters.size(); ++c)
            clusters[c] = {toRs(out.clusters[c].center), out.clusters[c].members};
        for (std::size_t k = 0; k < count; ++k) labels[k] = out.labels[k];
        *clusterCount = out.clusters.size();
        return AOI_RS_OK;
    } catch (...) {
        self->error_ = std::current_exception();
        return AOI_RS_CALLBACK_FAILED;
    }
}

ModelView::ModelView(const LatticeModel& m) : inliers_(m.inliers), cells_(toRs(m.cells)) {
    view_.basis = toRs(m.basis);
    view_.rms_px = m.rmsPx;
    view_.tolerance_px = m.tolerancePx;
    view_.coverage = m.coverage;
    view_.consistency = m.consistency;
    view_.confidence = m.confidence;
    view_.suggest_check = m.suggestCheck ? 1 : 0;
    view_.min_i = m.minI;
    view_.max_i = m.maxI;
    view_.min_j = m.minJ;
    view_.max_j = m.maxJ;
    view_.equations = toRs(detail::LatticeAccumulatorAccess::get(m.equations));
    view_.inliers = inliers_.data();
    view_.inlier_count = inliers_.size();
    view_.cells = cells_.data();
    view_.cell_count = cells_.size();
}

CandidateView::CandidateView(const detail::LatticeCandidate& c) : members_(c.members), cells_(toRs(c.cells)) {
    view_.basis = toRs(c.basis);
    view_.tolerance = c.tolerance;
    view_.members = members_.data();
    view_.member_count = members_.size();
    view_.cells = cells_.data();
    view_.cell_count = cells_.size();
}

LatticeModel toModel(const AoiRsLatticeModel& m) {
    LatticeModel out;
    out.basis = fromRs(m.basis);
    out.rmsPx = m.rms_px;
    out.tolerancePx = m.tolerance_px;
    out.coverage = m.coverage;
    out.consistency = m.consistency;
    out.confidence = m.confidence;
    out.suggestCheck = m.suggest_check != 0;
    out.minI = m.min_i;
    out.maxI = m.max_i;
    out.minJ = m.min_j;
    out.maxJ = m.max_j;
    out.inliers.assign(m.inliers, m.inliers + m.inlier_count);
    out.cells.reserve(m.cell_count);
    for (std::size_t k = 0; k < m.cell_count; ++k) out.cells.push_back(fromRs(m.cells[k]));
    out.equations = detail::LatticeAccumulatorAccess::make(fromRs(m.equations));
    return out;
}

}  // namespace aoi::rs
