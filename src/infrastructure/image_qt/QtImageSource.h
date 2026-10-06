#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "application/ports/IImageSource.h"

// infrastructure/image_qt：Qt Core ＋ Qt Gui。
namespace aoi::infra {

// IImageSource 的 Qt 版（S06）：QImageReader 解碼，交出正規格式（Gray8、Gray16、Bgrx32）的 view，契約見 IImageSource.h。
//  - 零複製：解碼出來的 QImage 已經是 Grayscale8、Grayscale16、RGB32 或 ARGB32 時（PNG、JPEG、BMP 的常見情況），
//    view 直接指到它的像素；其他格式轉一次並計數（QtImages.h）。
//  - 能讀的格式看這個 Qt 安裝有哪些外掛：這台機器的 Qt 6.8.0 實測是 bmp cur gif ico jfif jpeg jpg pbm pgm png ppm svg svgz
//    xbm xpm（tst_image_qt 的 log 會印出來），**沒有 TIFF**（qtiff 外掛不在 plugins/imageformats）。TIFF 由 OpenCV 的來源讀。
//  - 格式由檔案內容決定（副檔名錯了也讀得到），不套用 EXIF 的旋轉，校正值是 0。
//  - 可重入：每次 load 用自己的 QImageReader。建構時載入外掛清單，所以請在主執行緒建構。
class QtImageSource final : public IImageSource {
public:
    QtImageSource();

    bool canRead(std::string_view path) const override;
    Result<ImageView> load(std::string_view path) override;

    // 認得的副檔名（小寫、不含點），依字母排序。
    const std::vector<std::string>& suffixes() const noexcept { return suffixes_; }

private:
    std::vector<std::string> suffixes_;
};

}  // namespace aoi::infra
