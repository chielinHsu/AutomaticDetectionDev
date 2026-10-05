#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

// domain 層：只用 C++ 標準庫（docs/architecture.md）。
//
// ImageView 是跨過 port 傳遞影像的型別：它「借用」一塊像素，並用 owner 讓這塊像素在
// 所有 view 消失之前不會被釋放。任何函式庫的影像類別（QImage、cv::Mat、HImage）都不跨過 port。
//
// S02 先放最小版，因為畫布要零複製地顯示共享緩衝。所有權規則、格式轉換與複製計數
// 在 S03／S06 定案；那時可以擴充欄位，但這裡的欄位意義不要改。
namespace aoi {

enum class PixelFormat {
    Gray8,   // 每像素 1 byte
    Rgb24,   // R, G, B 各 1 byte
    Bgrx32,  // 每像素 4 byte，記憶體順序 B, G, R, 未使用（等同 QImage::Format_RGB32 在小端機器上的排列）
};

constexpr int bytesPerPixel(PixelFormat f) noexcept {
    switch (f) {
        case PixelFormat::Gray8: return 1;
        case PixelFormat::Rgb24: return 3;
        case PixelFormat::Bgrx32: return 4;
    }
    return 0;
}

struct ImageView {
    const std::uint8_t* data = nullptr;
    int width = 0;
    int height = 0;
    int strideBytes = 0;
    PixelFormat format = PixelFormat::Gray8;
    // 擁有者：只負責讓 data 活著，不描述型別。可以是空的（呼叫端保證 data 活得比 view 久）。
    std::shared_ptr<const void> owner;

    bool valid() const noexcept {
        return data != nullptr && width > 0 && height > 0 &&
               strideBytes >= width * bytesPerPixel(format);
    }
    std::size_t sizeBytes() const noexcept {
        return static_cast<std::size_t>(strideBytes) * static_cast<std::size_t>(height);
    }
};

}  // namespace aoi
