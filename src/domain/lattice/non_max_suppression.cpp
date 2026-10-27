// AOI_DOMAIN_IMPL=cpp：轉呼叫 C++ 的實作（aoi::legacy，lattice/legacy/non_max_suppression_legacy.cpp）。
// AOI_DOMAIN_IMPL=rust 時編的是 non_max_suppression_rs.cpp（ADR-031）。
#include "domain/lattice/non_max_suppression.h"

#include "domain/lattice/legacy/non_max_suppression_legacy.h"

namespace aoi {

double overlapArea(const RectObj& a, const RectObj& b) noexcept { return legacy::overlapArea(a, b); }

double overlapRatio(const RectObj& a, const RectObj& b, OverlapMeasure measure) noexcept { return legacy::overlapRatio(a, b, measure); }

struct OverlapIndex::Impl {
    legacy::OverlapIndex index;
};

OverlapIndex::OverlapIndex(double cellSize, double maxOverlap, OverlapMeasure measure)
    : impl_(std::make_unique<Impl>(Impl{legacy::OverlapIndex(cellSize, maxOverlap, measure)})) {}

OverlapIndex::~OverlapIndex() = default;
OverlapIndex::OverlapIndex(OverlapIndex&&) noexcept = default;
OverlapIndex& OverlapIndex::operator=(OverlapIndex&&) noexcept = default;

bool OverlapIndex::overlapsAny(const RectObj& box) const { return impl_ && impl_->index.overlapsAny(box); }

void OverlapIndex::add(const RectObj& box) {
    if (impl_) impl_->index.add(box);
}

std::size_t OverlapIndex::size() const noexcept { return impl_ ? impl_->index.size() : 0; }

std::vector<std::size_t> NonMaxSuppression::keep(std::span<const ScoredBox> boxes) const { return legacy::keep(boxes, options_); }

}  // namespace aoi
