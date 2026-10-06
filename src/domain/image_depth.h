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
// 配方或相機設定知道真正的位元數時，應該覆寫（S07：decideSignificantBits）。不是 Gray16、或無效的 view，回傳 0（＝ 格式的完整位元數）。
int detectSignificantBits(const ImageView& image) noexcept;

// 實際用的位元數：significantBits 有值就用它，否則是格式的完整位元數（Gray16 是 16，其他每通道 8）。
int effectiveBits(const ImageView& image) noexcept;

// 這個位元數的最大值（2^bits − 1）：亮度換算的分母。
constexpr int normalizedMax(int bits) noexcept { return bits <= 0 || bits > 16 ? 65535 : (1 << bits) - 1; }

// ===== 有效位元數的覆寫（S07）=====
// 推測只看最大值：12 bit 相機拍的暗影像（最大值 ≤ 1023）會被當成 10 bit（或 8 bit），畫面太亮、對比的換算放大 4 倍。
// 知道真正位元數的人（使用者、配方、相機設定）可以覆寫。規則：
//  - 0 ＝ 不覆寫，用推測的值。設定值只能是 0 或 8..16（kSignificantBitsChoices）。
//  - 只對 Gray16 有意義：其他格式每個通道就是 8 bit，覆寫不適用（照原樣）。
//  - 覆寫值比推測的小，表示影像裡有值超過這個位元數的最大值：設定和資料矛盾（多半是選錯了），不套用、維持推測的值。
//    （推測的值是「容得下最大值的最少位元數」，所以「覆寫 ≥ 推測」就保證不會有值被截掉。）
enum class SignificantBitsChoice {
    Detected,       // 沒有覆寫（0）：用推測的值
    Overridden,     // 套用覆寫
    NotApplicable,  // 不是 Gray16：覆寫不適用
    BelowData,      // 覆寫值容不下影像裡的值：不套用
    Invalid,        // 設定值不是 0、也不在 8..16：不套用
};

struct SignificantBitsDecision {
    int bits = 0;  // 要放進 ImageView::significantBits 的值
    SignificantBitsChoice choice = SignificantBitsChoice::Detected;
};

inline constexpr int kSignificantBitsChoices[] = {8, 10, 12, 14, 16};  // UI 提供的選項（另加 0 ＝ 自動）

constexpr bool validSignificantBitsSetting(int bits) noexcept { return bits == 0 || (bits >= 8 && bits <= 16); }

// format 與 detectedBits 是影像來源給的（ImageView::format、ImageView::significantBits）；requestedBits 是設定值。
SignificantBitsDecision decideSignificantBits(PixelFormat format, int detectedBits, int requestedBits) noexcept;

// 套用決定：只改 view 的 significantBits（零複製：像素與 owner 不變）。
inline ImageView withSignificantBits(ImageView view, const SignificantBitsDecision& decision) noexcept {
    view.significantBits = decision.bits;
    return view;
}

}  // namespace aoi
