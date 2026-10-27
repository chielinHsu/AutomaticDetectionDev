// 比對區換算的 C++ 實作（R05.3 從 match_region.cpp 原樣搬過來，ADR-031；用在哪裡見 match_region_legacy.h）。
// 運算一行沒改：只改命名空間（aoi::legacy）、PixelMask::count 變成自由函式 maskCount、呼叫同名函式寫 legacy::（引數是
// aoi 的型別，ADL 也會找到 aoi::maskFromRuns 等而模稜兩可）；operator== 搬到 match_region_equality.cpp（兩種建置共用）。
#include "domain/legacy/match_region_legacy.h"

#include <algorithm>
#include <cmath>

namespace aoi::legacy {

std::size_t maskCount(const PixelMask& mask) noexcept {
    if (mask.empty()) return 0;
    return static_cast<std::size_t>(std::count_if(mask.bits.begin(), mask.bits.end(), [](std::uint8_t b) { return b != 0; }));
}

MatchMask runsFromMask(const PixelMask& mask) {
    MatchMask out;
    if (mask.empty()) return out;
    out.width = mask.width;
    out.height = mask.height;
    for (int r = 0; r < mask.height; ++r) {
        const std::uint8_t* row = mask.bits.data() + static_cast<std::size_t>(r) * static_cast<std::size_t>(mask.width);
        int c = 0;
        while (c < mask.width) {
            while (c < mask.width && row[c] == 0) ++c;
            if (c >= mask.width) break;
            const int begin = c;
            while (c < mask.width && row[c] != 0) ++c;
            out.runs.push_back({r, begin, c});
        }
    }
    return out;
}

PixelMask maskFromRuns(const MatchMask& runs, int x, int y) {
    PixelMask out;
    if (runs.width <= 0 || runs.height <= 0) return out;
    out.x = x;
    out.y = y;
    out.width = runs.width;
    out.height = runs.height;
    out.bits.assign(static_cast<std::size_t>(runs.width) * static_cast<std::size_t>(runs.height), 0);
    for (const MaskRun& run : runs.runs) {
        if (run.row < 0 || run.row >= runs.height) continue;
        const int begin = std::max(0, run.colBegin);
        const int end = std::min(runs.width, run.colEnd);
        for (int c = begin; c < end; ++c) {
            out.bits[static_cast<std::size_t>(run.row) * static_cast<std::size_t>(runs.width) + static_cast<std::size_t>(c)] = 1;
        }
    }
    return out;
}

MatchSpec matchSpecFor(const MatchSettings& settings, Point2D bodyCenterPx, double umPerPixel) {
    MatchSpec spec;
    spec.mode = settings.mode;
    if (!settings.region || !(umPerPixel > 0.0)) return spec;
    const MatchRegion& region = *settings.region;
    const double px = 1.0 / umPerPixel;
    const Rect rect = rectFromCenter({bodyCenterPx.x + region.centerXUm * px, bodyCenterPx.y + region.centerYUm * px},
                                     {region.widthUm * px, region.heightUm * px});
    if (region.mask.empty()) {
        spec.rect = rect;
        return spec;
    }

    // 遮罩：外接矩形四捨五入到整數像素，每個像素取最近的格子（同一個校正值時就是原來的遮罩）。
    const int x0 = static_cast<int>(std::lround(rect.x));
    const int y0 = static_cast<int>(std::lround(rect.y));
    const int w = std::max(1, static_cast<int>(std::lround(rect.w)));
    const int h = std::max(1, static_cast<int>(std::lround(rect.h)));
    const PixelMask grid = legacy::maskFromRuns(region.mask, 0, 0);
    PixelMask mask;
    mask.x = x0;
    mask.y = y0;
    mask.width = w;
    mask.height = h;
    mask.bits.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0);
    for (int j = 0; j < h; ++j) {
        const int gy = std::min(grid.height - 1, static_cast<int>((j + 0.5) * grid.height / h));
        for (int i = 0; i < w; ++i) {
            const int gx = std::min(grid.width - 1, static_cast<int>((i + 0.5) * grid.width / w));
            mask.bits[static_cast<std::size_t>(j) * static_cast<std::size_t>(w) + static_cast<std::size_t>(i)] = grid.at(gx, gy) ? 1 : 0;
        }
    }
    spec.rect = mask.bounds();
    spec.mask = std::move(mask);
    return spec;
}

MatchSettings matchSettingsFromPixels(const Rect& regionPx, const PixelMask* mask, MatchMode mode, Point2D bodyCenterPx,
                                      double umPerPixel) {
    MatchSettings settings;
    settings.mode = mode;
    const bool hasMask = mask != nullptr && !mask->empty();
    const Rect rect = hasMask ? mask->bounds() : regionPx;
    if (rect.empty() || !(umPerPixel > 0.0)) return settings;
    MatchRegion region;
    const Point2D c = rect.center();
    region.centerXUm = (c.x - bodyCenterPx.x) * umPerPixel;
    region.centerYUm = (c.y - bodyCenterPx.y) * umPerPixel;
    region.widthUm = rect.w * umPerPixel;
    region.heightUm = rect.h * umPerPixel;
    if (hasMask) region.mask = legacy::runsFromMask(*mask);
    settings.region = std::move(region);
    return settings;
}

}  // namespace aoi::legacy
