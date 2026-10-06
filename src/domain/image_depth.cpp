#include "domain/image_depth.h"

#include <cstdint>
#include <cstring>

namespace aoi {

int detectSignificantBits(const ImageView& image) noexcept {
    if (!image.valid() || image.format != PixelFormat::Gray16) return 0;
    std::uint16_t maxValue = 0;
    for (int y = 0; y < image.height && maxValue < 16384; ++y) {
        const std::uint8_t* row = image.data + static_cast<std::size_t>(y) * static_cast<std::size_t>(image.strideBytes);
        for (int x = 0; x < image.width; ++x) {
            std::uint16_t v = 0;
            std::memcpy(&v, row + 2 * static_cast<std::size_t>(x), sizeof v);  // 列的起點不保證 2 byte 對齊
            if (v > maxValue) maxValue = v;
        }
    }
    if (maxValue <= 255) return 8;
    if (maxValue <= 1023) return 10;
    if (maxValue <= 4095) return 12;
    if (maxValue <= 16383) return 14;
    return 16;
}

int effectiveBits(const ImageView& image) noexcept {
    if (image.significantBits > 0) return image.significantBits;
    return image.format == PixelFormat::Gray16 ? 16 : 8;
}

}  // namespace aoi
