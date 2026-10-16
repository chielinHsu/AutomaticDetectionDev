#include "domain/feature_extract.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include "domain/image_depth.h"

namespace aoi {

namespace {

// 一個像素的亮度（0..255）。公式和 GenerateObjectsFromSample 的對比檢查相同；Gray16 依有效位元數換算。
int luminance8(const ImageView& image, const std::uint8_t* row, int x, double gray16Scale) {
    double v = 0.0;
    switch (image.format) {
        case PixelFormat::Gray8:
            v = row[x];
            break;
        case PixelFormat::Rgb24: {
            const std::uint8_t* p = row + 3 * x;
            v = (77.0 * p[0] + 150.0 * p[1] + 29.0 * p[2]) / 256.0;
            break;
        }
        case PixelFormat::Bgrx32: {
            const std::uint8_t* p = row + 4 * x;
            v = (77.0 * p[2] + 150.0 * p[1] + 29.0 * p[0]) / 256.0;
            break;
        }
        case PixelFormat::Bgr24: {
            const std::uint8_t* p = row + 3 * x;
            v = (77.0 * p[2] + 150.0 * p[1] + 29.0 * p[0]) / 256.0;
            break;
        }
        case PixelFormat::Gray16: {
            std::uint16_t g = 0;
            std::memcpy(&g, row + 2 * x, sizeof g);  // 列的起點不保證 2 byte 對齊
            v = g * gray16Scale;
            break;
        }
    }
    return std::clamp(static_cast<int>(std::lround(v)), 0, 255);
}

// Otsu：讓兩類的類間變異最大的門檻（亮度 > 門檻算前景）。直方圖只有一個值時回 −1（沒有可分的）。
int otsuThreshold(const std::array<std::size_t, 256>& histogram, std::size_t total) {
    double sumAll = 0.0;
    for (int i = 0; i < 256; ++i) sumAll += static_cast<double>(i) * static_cast<double>(histogram[static_cast<std::size_t>(i)]);
    double sumBack = 0.0;
    std::size_t weightBack = 0;
    double best = -1.0;
    int threshold = -1;
    for (int t = 0; t < 255; ++t) {
        weightBack += histogram[static_cast<std::size_t>(t)];
        if (weightBack == 0) continue;
        const std::size_t weightFore = total - weightBack;
        if (weightFore == 0) break;
        sumBack += static_cast<double>(t) * static_cast<double>(histogram[static_cast<std::size_t>(t)]);
        const double meanBack = sumBack / static_cast<double>(weightBack);
        const double meanFore = (sumAll - sumBack) / static_cast<double>(weightFore);
        const double between = static_cast<double>(weightBack) * static_cast<double>(weightFore) * (meanBack - meanFore) * (meanBack - meanFore);
        if (between > best) {
            best = between;
            threshold = t;
        }
    }
    return threshold;
}

struct IntRect {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;  // 不含
    int y1 = 0;
    bool empty() const { return x1 <= x0 || y1 <= y0; }
};

// 浮點矩形的外接整數像素，裁到 [0, w) × [0, h)。
IntRect pixelsOf(const Rect& r, int w, int h) {
    IntRect out;
    out.x0 = std::max(0, static_cast<int>(std::floor(r.x)));
    out.y0 = std::max(0, static_cast<int>(std::floor(r.y)));
    out.x1 = std::min(w, static_cast<int>(std::ceil(r.right())));
    out.y1 = std::min(h, static_cast<int>(std::ceil(r.bottom())));
    return out;
}

}  // namespace

std::optional<BlobPick> pickBrightBlob(const ImageView& image, const Rect& within, Point2D click) {
    if (!image.valid()) return std::nullopt;
    const IntRect area = pixelsOf(within, image.width, image.height);
    if (area.empty()) return std::nullopt;
    const int w = area.x1 - area.x0;
    const int h = area.y1 - area.y0;
    const std::size_t total = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);

    const double gray16Scale = 255.0 / normalizedMax(effectiveBits(image));
    std::vector<std::uint8_t> lum(total);
    std::array<std::size_t, 256> histogram{};
    for (int y = 0; y < h; ++y) {
        const std::uint8_t* row = image.data + static_cast<std::size_t>(area.y0 + y) * static_cast<std::size_t>(image.strideBytes);
        for (int x = 0; x < w; ++x) {
            const int v = luminance8(image, row, area.x0 + x, gray16Scale);
            lum[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(v);
            ++histogram[static_cast<std::size_t>(v)];
        }
    }
    const int threshold = otsuThreshold(histogram, total);
    if (threshold < 0) return std::nullopt;

    // 起點：點擊的像素是前景就用它，否則用最近的前景像素（同距離取先掃到的：由上而下、由左而右）。
    const int cx = static_cast<int>(std::floor(click.x)) - area.x0;
    const int cy = static_cast<int>(std::floor(click.y)) - area.y0;
    const auto foreground = [&](int x, int y) {
        return lum[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] > threshold;
    };
    int sx = -1;
    int sy = -1;
    if (cx >= 0 && cy >= 0 && cx < w && cy < h && foreground(cx, cy)) {
        sx = cx;
        sy = cy;
    } else {
        double bestD2 = std::numeric_limits<double>::max();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (!foreground(x, y)) continue;
                const double dx = (x + 0.5) - (click.x - area.x0);
                const double dy = (y + 0.5) - (click.y - area.y0);
                const double d2 = dx * dx + dy * dy;
                if (d2 < bestD2) {
                    bestD2 = d2;
                    sx = x;
                    sy = y;
                }
            }
        }
    }
    if (sx < 0) return std::nullopt;

    // 8 連通的填色（堆疊，不遞迴）。
    std::vector<std::uint8_t> inBlob(total, 0);
    std::vector<int> stack;
    stack.push_back(sy * w + sx);
    inBlob[static_cast<std::size_t>(sy * w + sx)] = 1;
    int minX = sx;
    int minY = sy;
    int maxX = sx;
    int maxY = sy;
    while (!stack.empty()) {
        const int p = stack.back();
        stack.pop_back();
        const int px = p % w;
        const int py = p / w;
        minX = std::min(minX, px);
        maxX = std::max(maxX, px);
        minY = std::min(minY, py);
        maxY = std::max(maxY, py);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = px + dx;
                const int ny = py + dy;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                const std::size_t k = static_cast<std::size_t>(ny) * static_cast<std::size_t>(w) + static_cast<std::size_t>(nx);
                if (inBlob[k] != 0 || !foreground(nx, ny)) continue;
                inBlob[k] = 1;
                stack.push_back(ny * w + nx);
            }
        }
    }

    BlobPick pick;
    pick.threshold8 = threshold;
    PixelMask& blob = pick.blob;
    blob.x = area.x0 + minX;
    blob.y = area.y0 + minY;
    blob.width = maxX - minX + 1;
    blob.height = maxY - minY + 1;
    blob.bits.assign(static_cast<std::size_t>(blob.width) * static_cast<std::size_t>(blob.height), 0);
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            if (inBlob[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] == 0) continue;
            blob.bits[static_cast<std::size_t>(y - minY) * static_cast<std::size_t>(blob.width) + static_cast<std::size_t>(x - minX)] = 1;
        }
    }
    return pick;
}

PixelMask growMask(const PixelMask& blob, int radiusPx, const Rect& clip) {
    if (blob.empty()) return {};
    const int r = std::max(0, radiusPx);
    const IntRect limit{static_cast<int>(std::floor(clip.x)), static_cast<int>(std::floor(clip.y)),
                        static_cast<int>(std::ceil(clip.right())), static_cast<int>(std::ceil(clip.bottom()))};
    const IntRect out{std::max(limit.x0, blob.x - r), std::max(limit.y0, blob.y - r), std::min(limit.x1, blob.x + blob.width + r),
                      std::min(limit.y1, blob.y + blob.height + r)};
    if (out.empty()) return {};
    const int w = out.x1 - out.x0;
    const int h = out.y1 - out.y0;
    std::vector<std::uint8_t> bits(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0);

    // 圓盤的位移（含邊界：dx² + dy² ≤ r²）。
    std::vector<std::pair<int, int>> disc;
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy * dy <= r * r) disc.emplace_back(dx, dy);
        }
    }
    for (int by = 0; by < blob.height; ++by) {
        for (int bx = 0; bx < blob.width; ++bx) {
            if (blob.bits[static_cast<std::size_t>(by) * static_cast<std::size_t>(blob.width) + static_cast<std::size_t>(bx)] == 0) continue;
            const int px = blob.x + bx;
            const int py = blob.y + by;
            for (const auto& [dx, dy] : disc) {
                const int x = px + dx - out.x0;
                const int y = py + dy - out.y0;
                if (x < 0 || y < 0 || x >= w || y >= h) continue;
                bits[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] = 1;
            }
        }
    }

    // 裁到實際有像素的外接矩形（裁切後可能縮小）。
    int minX = w;
    int minY = h;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (bits[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)] == 0) continue;
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    }
    if (maxX < 0) return {};
    PixelMask grown;
    grown.x = out.x0 + minX;
    grown.y = out.y0 + minY;
    grown.width = maxX - minX + 1;
    grown.height = maxY - minY + 1;
    grown.bits.assign(static_cast<std::size_t>(grown.width) * static_cast<std::size_t>(grown.height), 0);
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            grown.bits[static_cast<std::size_t>(y - minY) * static_cast<std::size_t>(grown.width) + static_cast<std::size_t>(x - minX)] =
                bits[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)];
        }
    }
    return grown;
}

}  // namespace aoi
