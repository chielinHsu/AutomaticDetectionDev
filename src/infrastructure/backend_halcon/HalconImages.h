#pragma once

#include <halconcpp/HalconCpp.h>

#include <memory>

#include "domain/image_view.h"

// infrastructure/backend_halcon：ImageView → HALCON 的影像（S08，ADR-014 的規則 5、6）。
// 不用 Qt、不用 OpenCV、不 include 別的 adapter（docs/architecture.md）。
namespace aoi::infra {

// 影像像素的整數矩形（左上角＋寬高）。
struct PixelRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// ImageView 的一塊 → HALCON 的單通道影像（'byte'；nativeGray16 時 Gray16 是 'uint2'）。
// image 的像素 (0, 0) 是 view 的像素 (originX, originY)。
//  - Gray8，view 緊密排列（stride ＝ 寬）而且 roi 是整張：零複製（gen_image1_extern 指到 view 的像素）。
//    HALCON 的外部影像要一列接一列（說明："stored line-sequentially"），所以子影像與一部分要複製。
//  - Gray8 的其他情況：複製那一塊。
//  - Gray16：依有效位元數換成 8 bit，v × 255 ／（2^位元數 − 1），四捨五入、超過 255 飽和。位元數是 effectiveBits(view)
//    （看最大值的推測，或配方／使用者的覆寫，S07）。nativeGray16：緊密排列且整張時零複製（uint2），否則複製那一塊。
//  - 彩色（Bgrx32、Bgr24、Rgb24）：轉灰階，和 OpenCV 的 cvtColor 同一組定點係數（R 4899、G 9617、B 1868，右移 14），
//    兩個 back end 拿到同樣的灰階值（A/B 比較公平）。Bgrx32 的第 4 個 byte 不讀。
// 每次複製呼叫一次 countImageCopy（寫進新緩衝的 byte 數）。
//
// 生命週期：gen_image1_extern 沒有給清理函式，HALCON 不擁有那塊記憶體；keepAlive（view 的 owner，或複製時自己的緩衝）讓它活著。
// 所以 image 與所有從它衍生的物件（reduce_domain 的結果）都不可以活得比這個 HalconGray 久。成員的順序就是為了這一點：
// keepAlive 先建、最後解構。
struct HalconGray {
    std::shared_ptr<const void> keepAlive;
    HalconCpp::HImage image;
    int originX = 0;
    int originY = 0;
    bool copied = false;
};

// roi 要整個在影像內。view 無效、格式不支援或 roi 不合法時，回傳的 image 沒有初始化（image.IsInitialized() ＝ false）。
HalconGray toHalconGray(const ImageView& view, const PixelRect& roi, bool nativeGray16);

// HALCON 的 'byte' 影像 → ImageView（Gray8），零複製：view 指到 HALCON 的像素，owner 持有一份 HImage（參照計數）。
// 給演算法產生的新影像用（例如流程 B 縮放過的參考影像）。不是 'byte'、或不是單通道時回傳無效的 view。
ImageView viewOfHalconImage(const HalconCpp::HImage& image, double umPerPixel = 0.0);

// HALCON 的 (Row, Column) 是像素中心在整數；ADR-010 的連續座標是像素中心在 +0.5。這兩個函式只做這個位移。
// （find_shape_model 的結果還要另外換算，見 HalconPose.h。）
constexpr double halconToContinuous(double halconCoordinate) noexcept { return halconCoordinate + 0.5; }
constexpr double continuousToHalcon(double continuous) noexcept { return continuous - 0.5; }

}  // namespace aoi::infra
