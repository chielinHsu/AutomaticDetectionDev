#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "domain/image_view.h"

// domain 層：只用 C++ 標準庫。
namespace aoi {

// 自己擁有像素的影像緩衝（S06，ADR-014）：給「產生像素」的人用（合成影像、從檔案還原的參考影像、轉換的結果）。
//
//  - 只在交出 view() 之前寫（ImageView 規則 1）。交出之後緩衝仍可以再交出同一塊像素的 view，但不要再寫。
//  - 列是緊密排列的：strideBytes ＝ 寬 × 每像素 byte 數，沒有填充。QImage 與 cv::Mat 接受任何 stride，
//    HALCON 的外部記憶體要緊密排列（S08 驗證），所以緊密排列讓三個函式庫都有機會零複製接手。
//  - 起點對齊 64 byte（SIMD 與快取列）；配置時清成 0。
//  - 尺寸不合法（寬或高 ≤ 0、單邊超過 kMaxSide、總量超過 kMaxBytes）或記憶體不足時，得到無效的緩衝（valid() ＝ false），不丟例外。
//  - 複製 ImageBuffer 共用同一塊像素（和 ImageView 一樣是共享擁有）。
class ImageBuffer {
public:
    static constexpr int kMaxSide = 65535;
    static constexpr std::size_t kMaxBytes = std::size_t{1} << 31;  // 2 GiB
    static constexpr std::size_t kAlignment = 64;

    ImageBuffer() = default;
    ImageBuffer(int width, int height, PixelFormat format);

    bool valid() const noexcept { return pixels_ != nullptr; }
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    int strideBytes() const noexcept { return strideBytes_; }
    PixelFormat format() const noexcept { return format_; }
    std::size_t sizeBytes() const noexcept {
        return static_cast<std::size_t>(strideBytes_) * static_cast<std::size_t>(height_);
    }

    std::uint8_t* data() noexcept { return pixels_.get(); }
    const std::uint8_t* data() const noexcept { return pixels_.get(); }
    std::uint8_t* row(int y) noexcept {
        return pixels_.get() + static_cast<std::size_t>(y) * static_cast<std::size_t>(strideBytes_);
    }
    const std::uint8_t* row(int y) const noexcept {
        return pixels_.get() + static_cast<std::size_t>(y) * static_cast<std::size_t>(strideBytes_);
    }

    // 共用擁有權的 view（owner 是這塊像素）。無效的緩衝回傳無效的 view。
    ImageView view(double umPerPixel = 0.0) const;

private:
    std::shared_ptr<std::uint8_t> pixels_;
    int width_ = 0;
    int height_ = 0;
    int strideBytes_ = 0;
    PixelFormat format_ = PixelFormat::Gray8;
};

}  // namespace aoi
