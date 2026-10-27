// AOI_DOMAIN_IMPL=rust：轉呼叫 Rust 的實作（rust/aoi_domain_rs/src/feature_extract.rs，C ABI 在 domain/ffi/aoi_domain_rs.h；ADR-031）。
// 契約和 C++ 的實作相同（feature_extract.h）：配置失敗丟 std::bad_alloc，Rust 的 panic 是 bug，丟 std::logic_error。
// 影像的像素直接借給 Rust（不複製）；有效位元數由 C++ 的 effectiveBits 算（image_depth 留在 C++）。
#include "domain/feature_extract.h"

#include <utility>

#include "domain/image_depth.h"
#include "domain/lattice/lattice_rs_bridge.h"
#include "domain/match_region_rs_bridge.h"

namespace aoi {

// PixelFormat 的數值是 C ABI 的一部分（AoiRsImage::format）。
static_assert(static_cast<int>(PixelFormat::Gray8) == AOI_RS_PIXEL_GRAY8 && static_cast<int>(PixelFormat::Rgb24) == AOI_RS_PIXEL_RGB24 &&
                  static_cast<int>(PixelFormat::Bgrx32) == AOI_RS_PIXEL_BGRX32 && static_cast<int>(PixelFormat::Bgr24) == AOI_RS_PIXEL_BGR24 &&
                  static_cast<int>(PixelFormat::Gray16) == AOI_RS_PIXEL_GRAY16,
              "PixelFormat must match the Rust constants");

std::optional<BlobPick> pickBrightBlob(const ImageView& image, const Rect& within, Point2D click) {
    rs::checkAbi();
    const AoiRsImage raw{image.data, image.width, image.height, image.strideBytes, static_cast<int32_t>(image.format), effectiveBits(image)};
    int32_t found = 0;
    int32_t threshold = 0;
    AoiRsPixelMask* blob = nullptr;
    rs::check(aoi_rs_pick_bright_blob(&raw, rs::toRs(within), rs::toRs(click), &found, &threshold, &blob), "aoi_rs_pick_bright_blob");
    PixelMask mask = rs::takeMask(blob);
    if (found == 0) return std::nullopt;
    BlobPick pick;
    pick.blob = std::move(mask);
    pick.threshold8 = threshold;
    return pick;
}

PixelMask growMask(const PixelMask& blob, int radiusPx, const Rect& clip) {
    rs::checkAbi();
    const AoiRsPixelMaskView view = rs::viewOf(blob);
    AoiRsPixelMask* out = nullptr;
    rs::check(aoi_rs_grow_mask(&view, radiusPx, rs::toRs(clip), &out), "aoi_rs_grow_mask");
    return rs::takeMask(out);
}

}  // namespace aoi
