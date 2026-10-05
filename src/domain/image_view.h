#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

// domain 層：只用 C++ 標準庫（docs/architecture.md）。
//
// ImageView 是跨過 port 傳遞影像的型別：它「借用」一塊像素，並用 owner 讓這塊像素在
// 所有 view 消失之前不會被釋放。任何函式庫的影像類別（QImage、cv::Mat、HImage）都不跨過 port。
//
// S02 先放最小版，因為畫布要零複製地顯示共享緩衝。S03 加了校正值 umPerPixel（預設 0，既有欄位的意義沒變）。
// 所有權規則、格式轉換與複製計數在 S06 定案；那時可以再擴充欄位，但這裡的欄位意義不要改。
// 座標慣例（像素原點、軸向、角度）在 geometry.h。
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
    // 校正值：一個像素有多少 µm（S03 新增；ADR-006 §6）。0 ＝ 未校正，不可拿來換算。
    // 樣本庫存 µm，影像帶這個值，庫才能跨影像、跨相機重複使用。
    double umPerPixel = 0.0;

    bool valid() const noexcept {
        return data != nullptr && width > 0 && height > 0 &&
               strideBytes >= width * bytesPerPixel(format);
    }
    std::size_t sizeBytes() const noexcept {
        return static_cast<std::size_t>(strideBytes) * static_cast<std::size_t>(height);
    }
};

}  // namespace aoi
