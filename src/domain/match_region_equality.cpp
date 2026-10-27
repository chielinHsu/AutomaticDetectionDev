// 比對區型別的 operator==（match_region.h）。型別本身的比較，不是搬進 Rust 的演算法（ADR-031：型別留在 C++），
// 所以 AOI_DOMAIN_IMPL=cpp 與 rust 都編這個檔案（R05.3 從 match_region.cpp 分出來，內容沒有改）。
#include "domain/match_region.h"

namespace aoi {

bool operator==(const MaskRun& a, const MaskRun& b) noexcept {
    return a.row == b.row && a.colBegin == b.colBegin && a.colEnd == b.colEnd;
}

bool operator==(const MatchMask& a, const MatchMask& b) noexcept {
    return a.width == b.width && a.height == b.height && a.runs == b.runs;
}

bool operator==(const MatchRegion& a, const MatchRegion& b) noexcept {
    return a.centerXUm == b.centerXUm && a.centerYUm == b.centerYUm && a.widthUm == b.widthUm && a.heightUm == b.heightUm &&
           a.mask == b.mask;
}

bool operator==(const MatchSettings& a, const MatchSettings& b) noexcept { return a.region == b.region && a.mode == b.mode; }

}  // namespace aoi
