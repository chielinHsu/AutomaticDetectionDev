#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "domain/geometry.h"

// domain 層：只用 C++ 標準庫。
namespace aoi {

// 搜尋器（IPatternSearcher）會遵守的參數。每個階段由 use case 填自己的 minScore（種子階段嚴、驗證階段鬆）。
// 預設值是起點，不是量測出來的事實；S07 用真實 back end 校準。
struct SearchParams {
    // 只回傳分數 ≥ minScore 的匹配（0..1）。
    double minScore = 0.80;
    // 搜尋的角度範圍（度，座標慣例見 geometry.h）。預設只容許小角度，因為旋轉搜尋成本高（總體計畫風險 4）。
    double angleMinDeg = -5.0;
    double angleMaxDeg = 5.0;
    // 最多回傳幾個匹配（依分數由高到低）。
    std::size_t maxMatches = 20000;
};

// 搜尋區域：整張影像，或一組視窗（影像像素的軸對齊矩形）。
// 契約（S09 鎖住）：匹配的「中心」必須落在區域內；視窗搜尋只掃視窗，不掃整張影像。
struct SearchRegion {
    enum class Kind { WholeImage, Windows };
    Kind kind = Kind::WholeImage;
    std::vector<Rect> windows;  // 只有 Windows 用

    static SearchRegion wholeImage() { return {}; }
    static SearchRegion inWindows(std::vector<Rect> w) { return {Kind::Windows, std::move(w)}; }
};

}  // namespace aoi
