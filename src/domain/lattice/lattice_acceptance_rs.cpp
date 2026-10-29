// AOI_DOMAIN_IMPL=rust：轉呼叫 Rust 的實作（rust/aoi_domain_rs，C ABI 在 domain/ffi/aoi_domain_rs.h；ADR-031）。
// 估計器本身整個在 Rust 那一側（lattice_estimator_rs.cpp 一次呼叫）；這裡是給直接測規則的呼叫端（tst_lattice_rules、差異測試）。
#include "domain/lattice/lattice_acceptance.h"

#include "domain/lattice/lattice_rs_bridge.h"

namespace aoi::detail {

namespace {

constexpr int32_t kTwoDimensionalIsValid = 0;
constexpr int32_t kEnoughEvidence = 1;

bool candidateRule(int32_t which, const LatticeCandidate& c, std::span<const Point2D> points, const LatticeOptions& o) {
    rs::checkAbi();
    const rs::CandidateView view(c);
    const std::vector<AoiRsPoint> pts = rs::toRs(points);
    const AoiRsLatticeOptions options = rs::toRs(o);
    int32_t out = 0;
    rs::check(aoi_rs_lattice_candidate_rule(which, view.get(), pts.data(), pts.size(), &options, &out), "aoi_rs_lattice_candidate_rule");
    return out != 0;
}

}  // namespace

bool twoDimensionalIsValid(const LatticeCandidate& c, std::span<const Point2D> points, const LatticeOptions& o) {
    return candidateRule(kTwoDimensionalIsValid, c, points, o);
}

bool enoughEvidence(const LatticeCandidate& c, std::span<const Point2D> points, const LatticeOptions& o) {
    return candidateRule(kEnoughEvidence, c, points, o);
}

bool holesAreBacked(const LatticeModel& m) {
    rs::checkAbi();
    // 內點數、索引範圍與內點的格子（R07.2 起索引區域要看格子）；ModelView 也複製內點，和走訪的格子數比很小。
    const rs::ModelView view(m);
    int32_t out = 0;
    rs::check(aoi_rs_lattice_holes_are_backed(view.get(), &out), "aoi_rs_lattice_holes_are_backed");
    return out != 0;
}

double consistency(const LatticeModel& m, std::span<const Point2D> points) {
    rs::checkAbi();
    const rs::ModelView view(m);
    const std::vector<AoiRsPoint> pts = rs::toRs(points);
    double out = 0.0;
    rs::check(aoi_rs_lattice_consistency(view.get(), pts.data(), pts.size(), &out), "aoi_rs_lattice_consistency");
    return out;
}

}  // namespace aoi::detail
