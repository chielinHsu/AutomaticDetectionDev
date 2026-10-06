#pragma once

#include <string_view>

#include "application/ports/IImageSource.h"

// infrastructure/backend_opencv：OpenCV。
namespace aoi::infra {

// IImageSource 的 OpenCV 版（S06）：cv::imdecode 解碼，交出正規格式（Gray8、Gray16、Bgrx32）的 view，契約見 IImageSource.h。
//  - 能讀 TIFF（8／16 bit）、16 bit PNG、BMP、JPEG、PNM 等（這個 OpenCV 4.9.0 建置有 libtiff、libpng、libjpeg）。
//  - 零複製：8 bit 灰階、16 bit 灰階、8 bit BGRA 直接交出 cv::Mat 的像素。
//    **8 bit 彩色要複製一次**（OpenCV 解出 B、G、R 三個 byte，正規格式是四個 byte 的 Bgrx32），16 bit 彩色轉成 8 bit 也算複製；
//    每次都呼叫 countImageCopy。所以組裝時 Qt 的來源排在前面（常見格式零複製），OpenCV 讀 Qt 讀不了的（ADR-014）。
//  - 自己用 UTF-8 路徑讀檔再交給 imdecode：cv::imread 在 Windows 用系統的 ANSI 字碼頁開檔，含中文的路徑會失敗。
//  - 不套用 EXIF 的旋轉（IMREAD_UNCHANGED），校正值是 0。不丟例外（cv::Exception 在這裡攔下）。
//  - 可重入：沒有成員狀態。
class OpenCvImageSource final : public IImageSource {
public:
    bool canRead(std::string_view path) const override;
    Result<ImageView> load(std::string_view path) override;
};

}  // namespace aoi::infra
