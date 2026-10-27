// AOI_DOMAIN_IMPL=rust：轉呼叫 Rust 的實作（rust/aoi_domain_rs，C ABI 在 domain/ffi/aoi_domain_rs.h；ADR-031）。
// 契約和 C++ 的實作相同（non_max_suppression.h）：noexcept 的函式遇到 Rust 回報錯誤時回安全值（0），
// 其他的配置失敗丟 std::bad_alloc；Rust 的 panic 是 bug，丟 std::logic_error（panic 本身不跨過 FFI）。
#include "domain/lattice/non_max_suppression.h"

#include <new>
#include <stdexcept>
#include <string>

#include "domain/ffi/aoi_domain_rs.h"

namespace aoi {

namespace {

static_assert(sizeof(AoiRsRect) == 5 * sizeof(double) && sizeof(AoiRsScoredBox) == 6 * sizeof(double), "AoiRsRect must match #[repr(C)]");
static_assert(sizeof(std::size_t) == sizeof(uintptr_t), "size_t is usize on the Rust side");

AoiRsRect toRs(const RectObj& r) noexcept { return {r.pose.x, r.pose.y, r.pose.angleDeg, r.width, r.height}; }

int32_t toRs(OverlapMeasure m) noexcept {
    return m == OverlapMeasure::IntersectionOverUnion ? AOI_RS_MEASURE_IOU : AOI_RS_MEASURE_SMALLER;
}

// 連到 ABI 不同的程式庫（例如舊的建置）時第一次呼叫就失敗，不要算出看起來合理的錯誤數字。
void checkAbi() {
    static const bool ok = aoi_rs_abi_version() == AOI_RS_ABI_VERSION;
    if (!ok) throw std::logic_error("aoi_domain_rs: ABI version mismatch (rebuild rust/aoi_domain_rs)");
}

void check(AoiRsStatus status, const char* what) {
    if (status == AOI_RS_OK) return;
    if (status == AOI_RS_NO_MEMORY) throw std::bad_alloc();
    throw std::logic_error(std::string("aoi_domain_rs: ") + what + (status == AOI_RS_PANIC ? " panicked" : " rejected its arguments"));
}

}  // namespace

double overlapArea(const RectObj& a, const RectObj& b) noexcept {
    const AoiRsRect ra = toRs(a);
    const AoiRsRect rb = toRs(b);
    double out = 0.0;
    return aoi_rs_overlap_area(&ra, &rb, &out) == AOI_RS_OK ? out : 0.0;
}

double overlapRatio(const RectObj& a, const RectObj& b, OverlapMeasure measure) noexcept {
    const AoiRsRect ra = toRs(a);
    const AoiRsRect rb = toRs(b);
    double out = 0.0;
    return aoi_rs_overlap_ratio(&ra, &rb, toRs(measure), &out) == AOI_RS_OK ? out : 0.0;
}

// Rust 物件的 handle：解構時釋放（誰配置誰釋放）。
struct OverlapIndex::Impl {
    AoiRsOverlapIndex* handle = nullptr;
    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    ~Impl() { aoi_rs_overlap_index_free(handle); }
};

OverlapIndex::OverlapIndex(double cellSize, double maxOverlap, OverlapMeasure measure) : impl_(std::make_unique<Impl>()) {
    checkAbi();
    check(aoi_rs_overlap_index_new(cellSize, maxOverlap, toRs(measure), &impl_->handle), "aoi_rs_overlap_index_new");
}

OverlapIndex::~OverlapIndex() = default;
OverlapIndex::OverlapIndex(OverlapIndex&&) noexcept = default;
OverlapIndex& OverlapIndex::operator=(OverlapIndex&&) noexcept = default;

bool OverlapIndex::overlapsAny(const RectObj& box) const {
    if (!impl_) return false;
    const AoiRsRect r = toRs(box);
    int32_t hit = 0;
    check(aoi_rs_overlap_index_overlaps_any(impl_->handle, &r, &hit), "aoi_rs_overlap_index_overlaps_any");
    return hit != 0;
}

void OverlapIndex::add(const RectObj& box) {
    if (!impl_) return;
    const AoiRsRect r = toRs(box);
    check(aoi_rs_overlap_index_add(impl_->handle, &r), "aoi_rs_overlap_index_add");
}

std::size_t OverlapIndex::size() const noexcept {
    std::size_t n = 0;
    return impl_ && aoi_rs_overlap_index_size(impl_->handle, &n) == AOI_RS_OK ? n : 0;
}

std::vector<std::size_t> NonMaxSuppression::keep(std::span<const ScoredBox> boxes) const {
    checkAbi();
    // 複製成 C 的結構（RectObj 的欄位順序相同，但不同型別之間轉指標在 C++ 不合法）：1 萬個框約 0.5 MB，和 NMS 本身比很小。
    std::vector<AoiRsScoredBox> in;
    in.reserve(boxes.size());
    for (const ScoredBox& b : boxes) in.push_back({toRs(b.rect), b.score});
    std::vector<std::size_t> out(boxes.size());
    std::size_t count = 0;
    check(aoi_rs_nms_keep(in.data(), in.size(), options_.maxOverlap, toRs(options_.measure), out.data(), &count), "aoi_rs_nms_keep");
    out.resize(count);
    return out;
}

}  // namespace aoi
