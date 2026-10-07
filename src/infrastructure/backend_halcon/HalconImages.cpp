#include "infrastructure/backend_halcon/HalconImages.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "domain/image_copy.h"
#include "domain/image_depth.h"

namespace aoi::infra {
namespace {

// 和 OpenCV 的 RGB2GRAY（8 bit）同一組定點係數：Y ＝ (4899 R ＋ 9617 G ＋ 1868 B ＋ 2^13) >> 14。
constexpr int kR = 4899;
constexpr int kG = 9617;
constexpr int kB = 1868;

std::uint8_t gray(int r, int g, int b) noexcept { return static_cast<std::uint8_t>((kR * r + kG * g + kB * b + (1 << 13)) >> 14); }

bool inside(const ImageView& view, const PixelRect& roi) noexcept {
    return roi.width > 0 && roi.height > 0 && roi.x >= 0 && roi.y >= 0 && static_cast<std::int64_t>(roi.x) + roi.width <= view.width &&
           static_cast<std::int64_t>(roi.y) + roi.height <= view.height;
}

const std::uint8_t* rowAt(const ImageView& view, int y, int x) noexcept {
    return view.data + static_cast<std::size_t>(y) * static_cast<std::size_t>(view.strideBytes) +
           static_cast<std::size_t>(x) * static_cast<std::size_t>(bytesPerPixel(view.format));
}

}  // namespace

HalconGray toHalconGray(const ImageView& view, const PixelRect& roi, bool nativeGray16) {
    HalconGray out;
    if (!view.valid() || !inside(view, roi)) return out;
    out.originX = roi.x;
    out.originY = roi.y;
    const bool whole = roi.x == 0 && roi.y == 0 && roi.width == view.width && roi.height == view.height;
    const bool native16 = nativeGray16 && view.format == PixelFormat::Gray16;

    // 零複製：HALCON 只讀這塊記憶體（gen_image1_extern 的 ClearProc ＝ 0，HALCON 不釋放它）。
    if (whole && view.packed() && (view.format == PixelFormat::Gray8 || native16)) {
        out.keepAlive = view.owner;
        out.image.GenImage1Extern(native16 ? "uint2" : "byte", view.width, view.height, const_cast<std::uint8_t*>(view.data), nullptr);
        return out;
    }

    const std::size_t w = static_cast<std::size_t>(roi.width);
    const std::size_t h = static_cast<std::size_t>(roi.height);
    if (native16) {
        auto buffer = std::make_shared<std::vector<std::uint16_t>>(w * h);
        for (std::size_t y = 0; y < h; ++y) {
            std::memcpy(buffer->data() + y * w, rowAt(view, roi.y + static_cast<int>(y), roi.x), w * sizeof(std::uint16_t));
        }
        countImageCopy(w * h * sizeof(std::uint16_t));
        out.image.GenImage1Extern("uint2", roi.width, roi.height, buffer->data(), nullptr);
        out.keepAlive = std::move(buffer);
        out.copied = true;
        return out;
    }

    auto buffer = std::make_shared<std::vector<std::uint8_t>>(w * h);
    std::uint8_t* dst = buffer->data();
    switch (view.format) {
        case PixelFormat::Gray8:
            for (std::size_t y = 0; y < h; ++y) std::memcpy(dst + y * w, rowAt(view, roi.y + static_cast<int>(y), roi.x), w);
            break;
        case PixelFormat::Gray16: {
            // 查表：每個值算一次（四捨五入、飽和），像素只查表。S08 實測：每個像素算 lround 的版本 5 MP 要 213 ms。
            // 表的大小是這塊影像裡出現的最大值 ＋ 1（小的一塊，例如樣本，不必算滿 65536 格）。
            const double scale = 255.0 / normalizedMax(effectiveBits(view));
            std::uint16_t maxValue = 0;
            for (std::size_t y = 0; y < h; ++y) {
                const auto* src = reinterpret_cast<const std::uint16_t*>(rowAt(view, roi.y + static_cast<int>(y), roi.x));
                for (std::size_t x = 0; x < w; ++x) maxValue = src[x] > maxValue ? src[x] : maxValue;
            }
            std::vector<std::uint8_t> table(static_cast<std::size_t>(maxValue) + 1);
            for (std::size_t v = 0; v < table.size(); ++v) {
                const long r = std::lround(static_cast<double>(v) * scale);
                table[v] = static_cast<std::uint8_t>(r > 255 ? 255 : r);
            }
            for (std::size_t y = 0; y < h; ++y) {
                const auto* src = reinterpret_cast<const std::uint16_t*>(rowAt(view, roi.y + static_cast<int>(y), roi.x));
                std::uint8_t* row = dst + y * w;
                for (std::size_t x = 0; x < w; ++x) row[x] = table[src[x]];
            }
            break;
        }
        case PixelFormat::Bgrx32:
        case PixelFormat::Bgr24:
        case PixelFormat::Rgb24: {
            const int step = bytesPerPixel(view.format);
            const bool rgb = view.format == PixelFormat::Rgb24;
            for (std::size_t y = 0; y < h; ++y) {
                const std::uint8_t* src = rowAt(view, roi.y + static_cast<int>(y), roi.x);
                for (std::size_t x = 0; x < w; ++x, src += step) {
                    dst[y * w + x] = rgb ? gray(src[0], src[1], src[2]) : gray(src[2], src[1], src[0]);
                }
            }
            break;
        }
    }
    countImageCopy(w * h);
    out.image.GenImage1Extern("byte", roi.width, roi.height, dst, nullptr);
    out.keepAlive = std::move(buffer);
    out.copied = true;
    return out;
}

ImageView viewOfHalconImage(const HalconCpp::HImage& image, double umPerPixel) {
    if (!image.IsInitialized() || image.CountChannels() != 1) return {};
    auto keep = std::make_shared<const HalconCpp::HImage>(image);  // 參照計數，不複製像素
    HalconCpp::HString type;
    Hlong width = 0;
    Hlong height = 0;
    const void* pixels = keep->GetImagePointer1(&type, &width, &height);
    if (std::strcmp(type.Text(), "byte") != 0 || pixels == nullptr || width <= 0 || height <= 0) return {};
    ImageView view;
    view.data = static_cast<const std::uint8_t*>(pixels);
    view.width = static_cast<int>(width);
    view.height = static_cast<int>(height);
    view.strideBytes = static_cast<int>(width);  // HALCON 的影像一列接一列
    view.format = PixelFormat::Gray8;
    view.owner = std::move(keep);
    view.umPerPixel = umPerPixel;
    return view;
}

}  // namespace aoi::infra
