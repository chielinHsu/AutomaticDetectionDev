// AOI_DOMAIN_IMPL=cpp：轉呼叫 C++ 的實作（aoi::legacy，legacy/match_region_legacy.cpp）。
// AOI_DOMAIN_IMPL=rust 時編的是 match_region_rs.cpp（ADR-031）。operator== 在 match_region_equality.cpp（兩種建置共用）。
#include "domain/match_region.h"

#include "domain/legacy/match_region_legacy.h"

namespace aoi {

std::size_t PixelMask::count() const noexcept { return legacy::maskCount(*this); }

MatchMask runsFromMask(const PixelMask& mask) { return legacy::runsFromMask(mask); }

PixelMask maskFromRuns(const MatchMask& runs, int x, int y) { return legacy::maskFromRuns(runs, x, y); }

MatchSpec matchSpecFor(const MatchSettings& settings, Point2D bodyCenterPx, double umPerPixel) {
    return legacy::matchSpecFor(settings, bodyCenterPx, umPerPixel);
}

MatchSettings matchSettingsFromPixels(const Rect& regionPx, const PixelMask* mask, MatchMode mode, Point2D bodyCenterPx,
                                      double umPerPixel) {
    return legacy::matchSettingsFromPixels(regionPx, mask, mode, bodyCenterPx, umPerPixel);
}

}  // namespace aoi
