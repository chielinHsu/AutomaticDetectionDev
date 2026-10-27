// AOI_DOMAIN_IMPL=rust：轉呼叫 Rust 的實作（rust/aoi_domain_rs/src/match_region.rs，C ABI 在 domain/ffi/aoi_domain_rs.h；ADR-031）。
// 契約和 C++ 的實作相同（match_region.h）：noexcept 的 PixelMask::count 遇到 Rust 回報錯誤時回 0；其他的配置失敗丟 std::bad_alloc，
// Rust 的 panic 是 bug，丟 std::logic_error（panic 本身不跨過 FFI）。operator== 在 match_region_equality.cpp（兩種建置共用）。
#include "domain/match_region.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include "domain/lattice/lattice_rs_bridge.h"
#include "domain/match_region_rs_bridge.h"

namespace aoi {

namespace {

// C 的結構和 Rust 的 #[repr(C)] 一樣大（Rust 那一側也檢查同樣的數字，rust/aoi_domain_rs/src/ffi_match.rs）。
static_assert(sizeof(AoiRsMaskRun) == 12 && sizeof(AoiRsPixelMaskView) == 32 && sizeof(AoiRsRegionUm) == 32, "AoiRs* must match #[repr(C)]");
static_assert(sizeof(AoiRsMatchRegion) == 56 && sizeof(AoiRsImage) == 32, "AoiRs* must match #[repr(C)]");
// asRs：MaskRun 與 AoiRsMaskRun 逐欄同樣的位置與大小（runs 直接交出 std::vector 的位址）。
static_assert(std::is_standard_layout_v<MaskRun> && sizeof(MaskRun) == sizeof(AoiRsMaskRun), "MaskRun must match AoiRsMaskRun");
static_assert(offsetof(MaskRun, row) == offsetof(AoiRsMaskRun, row));
static_assert(offsetof(MaskRun, colBegin) == offsetof(AoiRsMaskRun, col_begin));
static_assert(offsetof(MaskRun, colEnd) == offsetof(AoiRsMaskRun, col_end));

// C++ 這邊從不經過 AoiRsMaskRun 讀寫（只在 Rust 那一側），所以沒有型別別名的問題（和 lattice_rs_bridge.h 的 asRs 相同）。
const AoiRsMaskRun* asRs(const std::vector<MaskRun>& runs) noexcept { return reinterpret_cast<const AoiRsMaskRun*>(runs.data()); }

AoiRsMatchRegion toRs(const MatchRegion& r) noexcept {
    AoiRsMatchRegion out{};
    out.rect = {r.centerXUm, r.centerYUm, r.widthUm, r.heightUm};
    out.mask_width = r.mask.width;
    out.mask_height = r.mask.height;
    out.runs = asRs(r.mask.runs);
    out.run_count = r.mask.runs.size();
    return out;
}

}  // namespace

namespace rs {

AoiRsPixelMaskView viewOf(const PixelMask& mask) noexcept {
    return {mask.x, mask.y, mask.width, mask.height, mask.bits.data(), mask.bits.size()};
}

PixelMask takeMask(AoiRsPixelMask* handle) {
    const std::unique_ptr<AoiRsPixelMask, void (*)(AoiRsPixelMask*)> owner(handle, &aoi_rs_pixel_mask_free);
    PixelMask out;
    if (handle == nullptr) return out;
    AoiRsPixelMaskView view{};
    check(aoi_rs_pixel_mask_get(handle, &view), "aoi_rs_pixel_mask_get");
    out.x = view.x;
    out.y = view.y;
    out.width = view.width;
    out.height = view.height;
    out.bits.assign(view.bits, view.bits + view.bit_count);
    return out;
}

MatchMask takeRuns(AoiRsMaskRuns* handle) {
    const std::unique_ptr<AoiRsMaskRuns, void (*)(AoiRsMaskRuns*)> owner(handle, &aoi_rs_mask_runs_free);
    MatchMask out;
    if (handle == nullptr) return out;
    const AoiRsMaskRun* runs = nullptr;
    std::size_t count = 0;
    check(aoi_rs_mask_runs_get(handle, &out.width, &out.height, &runs, &count), "aoi_rs_mask_runs_get");
    out.runs.reserve(count);
    for (std::size_t k = 0; k < count; ++k) out.runs.push_back({runs[k].row, runs[k].col_begin, runs[k].col_end});
    return out;
}

}  // namespace rs

std::size_t PixelMask::count() const noexcept {
    const AoiRsPixelMaskView view = rs::viewOf(*this);
    std::size_t out = 0;
    return aoi_rs_mask_count(&view, &out) == AOI_RS_OK ? out : 0;
}

MatchMask runsFromMask(const PixelMask& mask) {
    rs::checkAbi();
    const AoiRsPixelMaskView view = rs::viewOf(mask);
    AoiRsMaskRuns* handle = nullptr;
    rs::check(aoi_rs_runs_from_mask(&view, &handle), "aoi_rs_runs_from_mask");
    return rs::takeRuns(handle);
}

PixelMask maskFromRuns(const MatchMask& runs, int x, int y) {
    rs::checkAbi();
    PixelMask out;
    // bits 由呼叫端配置（寬 × 高），所以先要知道大小：格子的寬或高 ≤ 0 時和 C++ 的實作一樣回空的遮罩。
    if (runs.width <= 0 || runs.height <= 0) return out;
    out.x = x;
    out.y = y;
    out.width = runs.width;
    out.height = runs.height;
    out.bits.resize(static_cast<std::size_t>(runs.width) * static_cast<std::size_t>(runs.height));
    rs::check(aoi_rs_mask_from_runs(asRs(runs.runs), runs.runs.size(), runs.width, runs.height, out.bits.data(), out.bits.size()),
              "aoi_rs_mask_from_runs");
    return out;
}

MatchSpec matchSpecFor(const MatchSettings& settings, Point2D bodyCenterPx, double umPerPixel) {
    rs::checkAbi();
    MatchSpec spec;
    spec.mode = settings.mode;
    const AoiRsMatchRegion region = settings.region ? toRs(*settings.region) : AoiRsMatchRegion{};
    int32_t hasRect = 0;
    AoiRsBounds rect{};
    AoiRsPixelMask* mask = nullptr;
    rs::check(aoi_rs_match_spec_for(settings.region ? &region : nullptr, rs::toRs(bodyCenterPx), umPerPixel, &hasRect, &rect, &mask),
              "aoi_rs_match_spec_for");
    spec.mask = rs::takeMask(mask);
    if (hasRect != 0) spec.rect = Rect{rect.x, rect.y, rect.w, rect.h};
    return spec;
}

MatchSettings matchSettingsFromPixels(const Rect& regionPx, const PixelMask* mask, MatchMode mode, Point2D bodyCenterPx,
                                      double umPerPixel) {
    rs::checkAbi();
    MatchSettings settings;
    settings.mode = mode;
    const AoiRsPixelMaskView view = mask != nullptr ? rs::viewOf(*mask) : AoiRsPixelMaskView{};
    int32_t hasRegion = 0;
    AoiRsRegionUm rect{};
    AoiRsMaskRuns* runs = nullptr;
    rs::check(aoi_rs_match_settings_from_pixels(rs::toRs(regionPx), mask != nullptr ? &view : nullptr, rs::toRs(bodyCenterPx), umPerPixel,
                                                &hasRegion, &rect, &runs),
              "aoi_rs_match_settings_from_pixels");
    MatchMask runsOut = rs::takeRuns(runs);
    if (hasRegion == 0) return settings;
    MatchRegion region;
    region.centerXUm = rect.center_x_um;
    region.centerYUm = rect.center_y_um;
    region.widthUm = rect.width_um;
    region.heightUm = rect.height_um;
    region.mask = std::move(runsOut);
    settings.region = std::move(region);
    return settings;
}

}  // namespace aoi
