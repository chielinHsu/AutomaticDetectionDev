#pragma once

#include <memory>
#include <opencv2/core.hpp>

#include "domain/image_view.h"

// infrastructure/backend_opencv：OpenCV（S06 起：影像轉換與影像來源；S07 加範本建立器與搜尋器）。
// 不用 Qt、不用 HALCON、不 include 別的 adapter（docs/architecture.md）。
// cv::Mat 與 ImageView 之間的轉換（S06，ADR-014）。
namespace aoi::infra {

// cv::Mat → ImageView，零複製（view 的 owner 持有一份 cv::Mat，只加參照計數）：
//   8UC1 → Gray8、16UC1 → Gray16、8UC3 → Bgr24（OpenCV 的通道順序是 B、G、R）、8UC4 → Bgrx32。
// 其他型別（浮點、有號、2 通道、多維）回傳無效的 view：要不要轉、怎麼轉（並計數），由呼叫端決定。
// Mat 是別人的子矩陣（ROI）也可以：stride 用 Mat 的 step。
// 包著外部記憶體的 Mat（自己不擁有像素，例如 borrowAsMat 的結果）回傳無效的 view：它留不住像素，做成 view 會懸空。
// 要把 OpenCV 的結果交出去，結果必須是 OpenCV 自己配置的 Mat。
ImageView fromMat(const cv::Mat& mat, double umPerPixel = 0.0);

// ImageView → cv::Mat，零複製：Mat 指到 view 的像素，但不擁有它。
// 拿著 MatView 的時候像素一定在（owner 跟著）；把 mat 複製出去另外保存，生命週期就要自己負責。**不可以寫**：
// ImageView 的像素交出去之後不再改（ImageView 規則 1、5），cv::Mat 沒有唯讀的型別，只能靠約定。
// Gray8 → 8UC1、Gray16 → 16UC1、Bgr24 → 8UC3、Bgrx32 → 8UC4、Rgb24 → 8UC3（通道順序是 R、G、B，
// 轉灰階要用 COLOR_RGB2GRAY，不是 COLOR_BGR2GRAY）。無效的 view 回傳空的 Mat。
struct MatView {
    cv::Mat mat;
    std::shared_ptr<const void> owner;
};
MatView borrowAsMat(const ImageView& view);

// ImageView 的一塊（roi：影像像素的整數矩形，要整個在影像內）→ 8 bit 灰階的 cv::Mat，給 matchTemplate 用（S07）。
//  - Gray8：零複製（mat 指到 view 的像素，owner 跟著；不可以寫）。
//  - Gray16：v × 255 ／（2^位元數 − 1），四捨五入、超過 255 的飽和。位元數是 effectiveBits(view)：
//    看最大值的推測，或配方／使用者的覆寫（view.significantBits，S07）。一次複製，計數（countImageCopy）。
//  - 彩色（Bgrx32、Bgr24、Rgb24）：轉灰階（0.299 R ＋ 0.587 G ＋ 0.114 B）；Bgrx32 的第 4 個 byte 不讀。一次複製，計數。
// view 無效、roi 是空的或超出影像時回傳空的 mat。
struct GrayRoi {
    cv::Mat mat;
    std::shared_ptr<const void> owner;  // 零複製時讓像素活著；複製時是空的（mat 自己擁有像素）
    bool copied = false;
};
GrayRoi toGray8(const ImageView& view, const cv::Rect& roi);

}  // namespace aoi::infra
