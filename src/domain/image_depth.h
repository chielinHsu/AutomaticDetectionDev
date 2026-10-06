#pragma once

#include "domain/image_view.h"

// domain 層：只用標準庫。影像的位元深度（S06：使用者說影像也有 12 bit 的）。
//
// 12 bit（以及 10、14 bit）的相機影像存成檔案有兩種樣子：
//   - 16 bit 的容器、值只到 2^n − 1（低位對齊）：相機軟體常見。這種要靠 ImageView::significantBits 才能正確顯示與換算。
//   - 值放大到整個 16 bit 的範圍（高位對齊）：例如 OpenCV 4.9.0 讀 BitsPerSample ＝ 12 的 TIFF，會把 0x123 給成 0x1230
//     （tst_opencv_image 鎖住這個事實）。這種當成 16 bit 處理就是對的，最低的幾位一定是 0。
// 兩種情況，「值 ÷（2^位元數 − 1）」都是同一個亮度，使用端一律用 normalizedMax 換算。
namespace aoi {

// 看最大值推測 Gray16 的有效位元數：最大值 ≤ 255 → 8、≤ 1023 → 10、≤ 4095 → 12、≤ 16383 → 14，否則 16。
// 這是推測：一張很暗的 16 bit 影像會被當成位元數較少（只會讓顯示比較亮、對比的換算比較寬鬆）；
// 配方或相機設定知道真正的位元數時，應該覆寫。不是 Gray16、或無效的 view，回傳 0（＝ 格式的完整位元數）。
int detectSignificantBits(const ImageView& image) noexcept;

// 實際用的位元數：significantBits 有值就用它，否則是格式的完整位元數（Gray16 是 16，其他每通道 8）。
int effectiveBits(const ImageView& image) noexcept;

// 這個位元數的最大值（2^bits − 1）：亮度換算的分母。
constexpr int normalizedMax(int bits) noexcept { return bits <= 0 || bits > 16 ? 65535 : (1 << bits) - 1; }

}  // namespace aoi
