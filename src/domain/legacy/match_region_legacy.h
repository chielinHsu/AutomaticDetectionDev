#pragma once

#include <cstddef>

#include "domain/geometry.h"
#include "domain/match_region.h"

// domain 層：只用 C++ 標準庫。
//
// 比對區換算的 C++ 實作（ADR-028 起的那一份，R05.3 從 match_region.cpp 原樣搬過來，ADR-031）。用在兩個地方：
//   - AOI_DOMAIN_IMPL=cpp 的建置：aoi_domain 編進它，match_region.cpp 轉呼叫這裡；
//   - AOI_DOMAIN_IMPL=rust 的建置：只有差異測試用的 aoi_domain_legacy 程式庫編進它（tests/domain/tst_domain_rs_diff 拿它當對照組）。
// 說明在 match_region.h；這裡不要改行為（要改，Rust 那一份 rust/aoi_domain_rs/src/match_region.rs 與差異測試一起改）。
// operator== 是型別本身的比較，兩種建置共用 match_region_equality.cpp，不在這裡。
namespace aoi::legacy {

std::size_t maskCount(const PixelMask& mask) noexcept;  // PixelMask::count
MatchSpec matchSpecFor(const MatchSettings& settings, Point2D bodyCenterPx, double umPerPixel);
MatchSettings matchSettingsFromPixels(const Rect& regionPx, const PixelMask* mask, MatchMode mode, Point2D bodyCenterPx,
                                      double umPerPixel);
MatchMask runsFromMask(const PixelMask& mask);
PixelMask maskFromRuns(const MatchMask& runs, int x, int y);

}  // namespace aoi::legacy
