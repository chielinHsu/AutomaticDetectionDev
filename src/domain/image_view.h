#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

// domain 層：只用 C++ 標準庫（docs/architecture.md）。
//
// ImageView 是跨過 port 傳遞影像的型別：它「借用」一塊像素，並用 owner 讓這塊像素在
// 所有 view 消失之前不會被釋放。任何函式庫的影像類別（QImage、cv::Mat、HImage）都不跨過 port。
// 座標慣例（像素原點、軸向、角度）在 geometry.h。
//
// ===== 所有權與生命週期（S06 定案，ADR-014）=====
//  1. 像素交出去之後不再改：誰拿到 view，誰就可以在任何執行緒讀，不必加鎖（UI 畫、worker 搜尋同時讀同一塊）。
//     要改像素就配一塊新的（ImageBuffer），寫完再交出它的 view。
//  2. 複製 ImageView 只複製指標與 owner 的參照計數，不複製像素。
//  3. owner 讓像素活著。跨執行緒、跨 port、或活得比這次呼叫久的 view 一定要有 owner；
//     owner 是空的 view 只能在同一次呼叫裡借用（呼叫端保證像素活得比它久，例如測試的區域變數）。
//  4. 子影像（subView）與原影像共用 owner。
//  5. 函式庫的影像物件包住 view 的像素時（QImage 的清理函式、cv::Mat 的外部資料），要讓 owner 活著，而且不可以寫。
//  6. 做不到零複製的轉換，就配新的緩衝並呼叫 countImageCopy（image_copy.h）：複製要看得見、數得出來。
//
// S02 先放最小版；S03 加了校正值 umPerPixel；S06 加了 Bgr24、Gray16 兩種格式與 subView。既有欄位的意義沒有改。
namespace aoi {

enum class PixelFormat {
    Gray8,   // 每像素 1 byte
    Rgb24,   // R, G, B 各 1 byte
    // 每像素 4 byte，記憶體順序 B, G, R, x（等同 QImage::Format_RGB32 在小端機器上的排列）。
    // 第 4 個 byte 沒有定義（可能是 alpha，也可能是 255）：使用端不可以讀它。
    Bgrx32,
    Bgr24,   // B, G, R 各 1 byte（S06：OpenCV 彩色影像的排列，cv::Mat 8UC3 可以零複製）
    Gray16,  // 每像素 2 byte：無號整數、本機的位元組順序（x86／x64 是小端），0..65535（S06：16 bit 灰階，例如 TIFF）
};

constexpr int bytesPerPixel(PixelFormat f) noexcept {
    switch (f) {
        case PixelFormat::Gray8: return 1;
        case PixelFormat::Rgb24: return 3;
        case PixelFormat::Bgrx32: return 4;
        case PixelFormat::Bgr24: return 3;
        case PixelFormat::Gray16: return 2;
    }
    return 0;
}

// 給 log、測試訊息與檔案格式用的名稱（storage 的 JSON 存這個字串）。
constexpr std::string_view toString(PixelFormat f) noexcept {
    switch (f) {
        case PixelFormat::Gray8: return "Gray8";
        case PixelFormat::Rgb24: return "Rgb24";
        case PixelFormat::Bgrx32: return "Bgrx32";
        case PixelFormat::Bgr24: return "Bgr24";
        case PixelFormat::Gray16: return "Gray16";
    }
    return "Unknown";
}

struct ImageView {
    const std::uint8_t* data = nullptr;
    int width = 0;
    int height = 0;
    int strideBytes = 0;
    PixelFormat format = PixelFormat::Gray8;
    // 擁有者：只負責讓 data 活著，不描述型別。可以是空的（規則 3：只能在同一次呼叫裡借用）。
    std::shared_ptr<const void> owner;
    // 校正值：一個像素有多少 µm（S03 新增；ADR-006 §6）。0 ＝ 未校正，不可拿來換算。
    // 樣本庫存 µm，影像帶這個值，庫才能跨影像、跨相機重複使用。
    double umPerPixel = 0.0;
    // 有效位元數（S06，給 Gray16）：值在 0..2^n − 1、低位對齊。12 bit 相機的影像常存成 16 bit 的檔案、值只到 4095，
    // 沒有這個欄位，畫面會幾乎全黑、對比會被低估 16 倍。0 ＝ 這個格式的完整位元數（Gray16 是 16、其他每通道 8）。
    // 影像來源看最大值推測（image_depth.h）；S07 起使用者或配方可以覆寫（Board::significantBits、decideSignificantBits）。
    int significantBits = 0;

    // 一列像素實際用到的 byte 數（不含 stride 的填充）。
    std::int64_t rowBytes() const noexcept {
        return static_cast<std::int64_t>(width) * bytesPerPixel(format);
    }
    bool valid() const noexcept {
        return data != nullptr && width > 0 && height > 0 && strideBytes >= rowBytes();
    }
    std::size_t sizeBytes() const noexcept {
        return static_cast<std::size_t>(strideBytes) * static_cast<std::size_t>(height);
    }
    // 列與列之間沒有填充（HALCON 的外部記憶體要這種排列，S08 驗證）。
    bool packed() const noexcept { return valid() && strideBytes == rowBytes(); }
};

// 零複製的子影像：與 view 共用像素與 owner（規則 4），stride 不變，校正值照抄。
// 矩形要整個落在影像內、而且寬高 > 0；否則回傳無效的 view（不裁切：裁切會讓座標悄悄偏移）。
inline ImageView subView(const ImageView& view, int x, int y, int width, int height) noexcept {
    if (!view.valid() || width <= 0 || height <= 0 || x < 0 || y < 0) return {};
    if (static_cast<std::int64_t>(x) + width > view.width || static_cast<std::int64_t>(y) + height > view.height) return {};
    ImageView sub = view;
    sub.data = view.data + static_cast<std::size_t>(y) * static_cast<std::size_t>(view.strideBytes) +
               static_cast<std::size_t>(x) * static_cast<std::size_t>(bytesPerPixel(view.format));
    sub.width = width;
    sub.height = height;
    return sub;
}

}  // namespace aoi
