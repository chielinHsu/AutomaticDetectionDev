#pragma once

#include <cstddef>
#include <cstdint>

// domain 層：只用 C++ 標準庫。
//
// 影像像素的複製計數（S06，ADR-014）。目的：讓「零複製」是可以斷言的事實，而不是註解裡的願望。
//
// 什麼算一次複製：為了換「容器、記憶體排列或像素格式」，把同一張影像的像素寫到新的緩衝。例如
//   QImage 的索引色轉成灰階、cv::Mat 的 BGR 轉成 BGRx、彩色轉灰階給函式庫用、16 bit 轉 8 bit。
// 什麼不算：解碼（檔案 → 像素，之前沒有像素緩衝）、演算法產生的新內容（模糊、金字塔、搜尋結果的分數圖）、
//   存檔時的序列化（像素 → 檔案）。
//
// 每個做複製的地方呼叫一次 countImageCopy（不論它在哪一層）；測試取前後的差，斷言零複製的路徑差是 0。
// 計數是整個程式共用的（atomic，任何執行緒都可以呼叫），所以量測時同一段時間內不要有別的影像工作。
namespace aoi {

struct ImageCopyCount {
    std::uint64_t copies = 0;  // 複製了幾次
    std::uint64_t bytes = 0;   // 寫進新緩衝的像素 byte 數（不含 stride 的填充）
};

// 記下一次複製。bytes 是寫進新緩衝的像素 byte 數。
void countImageCopy(std::size_t bytes) noexcept;

// 程式啟動以來的總數。
ImageCopyCount imageCopyCount() noexcept;

// 兩次讀數的差（after − before）。
constexpr ImageCopyCount operator-(ImageCopyCount after, ImageCopyCount before) noexcept {
    return {after.copies - before.copies, after.bytes - before.bytes};
}

}  // namespace aoi
