#pragma once

#include "domain/ffi/aoi_domain_rs.h"
#include "domain/match_region.h"

// domain 層：只用 C++ 標準庫。AOI_DOMAIN_IMPL=rust 才編（R05.3，ADR-031）：match_region_rs.cpp 與 feature_extract_rs.cpp
// 共用的轉換（定義在 match_region_rs.cpp）。rs::checkAbi、rs::check 在 lattice/lattice_rs_bridge.h（R05.2 起所有 *_rs.cpp 共用）。
//
// 輸入不複製：遮罩的 bits 與 runs（MaskRun 和 AoiRsMaskRun 逐欄相同，match_region_rs.cpp 的 static_assert）直接交出位址。
// 輸出的 handle 由 take* 複製進 C++ 的型別後釋放（null ＝ 空的遮罩、沒有 runs）。
namespace aoi::rs {

// 檢視（bits 指向 mask 的陣列；mask 活著時有效）。
AoiRsPixelMaskView viewOf(const PixelMask& mask) noexcept;
// 複製 handle 的內容並釋放它（例外安全：複製失敗也會釋放）。null → PixelMask{}。
PixelMask takeMask(AoiRsPixelMask* handle);
// 同上；null → MatchMask{}。
MatchMask takeRuns(AoiRsMaskRuns* handle);

}  // namespace aoi::rs
