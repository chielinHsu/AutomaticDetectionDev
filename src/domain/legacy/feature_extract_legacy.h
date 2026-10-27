#pragma once

#include <optional>

#include "domain/feature_extract.h"
#include "domain/geometry.h"
#include "domain/image_view.h"
#include "domain/match_region.h"

// domain 層：只用 C++ 標準庫。
//
// 自動擷取（Otsu、最近的亮塊、外擴）的 C++ 實作（ADR-028 起的那一份，R05.3 從 feature_extract.cpp 原樣搬過來，ADR-031）。
// 用在哪裡、不要改行為的規則見 match_region_legacy.h；說明在 feature_extract.h，Rust 那一份在 rust/aoi_domain_rs/src/feature_extract.rs。
namespace aoi::legacy {

std::optional<BlobPick> pickBrightBlob(const ImageView& image, const Rect& within, Point2D click);
PixelMask growMask(const PixelMask& blob, int radiusPx, const Rect& clip);

}  // namespace aoi::legacy
