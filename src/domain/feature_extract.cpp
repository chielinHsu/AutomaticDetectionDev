// AOI_DOMAIN_IMPL=cpp：轉呼叫 C++ 的實作（aoi::legacy，legacy/feature_extract_legacy.cpp）。
// AOI_DOMAIN_IMPL=rust 時編的是 feature_extract_rs.cpp（ADR-031）。
#include "domain/feature_extract.h"

#include "domain/legacy/feature_extract_legacy.h"

namespace aoi {

std::optional<BlobPick> pickBrightBlob(const ImageView& image, const Rect& within, Point2D click) {
    return legacy::pickBrightBlob(image, within, click);
}

PixelMask growMask(const PixelMask& blob, int radiusPx, const Rect& clip) { return legacy::growMask(blob, radiusPx, clip); }

}  // namespace aoi
