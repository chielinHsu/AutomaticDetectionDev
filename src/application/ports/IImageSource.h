#pragma once

#include <string_view>

#include "application/Result.h"
#include "domain/image_view.h"

// application 層：port。實作：infrastructure/image_qt（Qt，S06）、infrastructure/backend_opencv（OpenCV，S06）；
// HALCON 的在 S08。幾個來源依優先順序組合：application/image/ImageSourceChain。
namespace aoi {

// 契約（S03 定下前兩條，S06 補完；ADR-014）：
//  - 同一個檔案，所有實作回傳相同語意的 ImageView（像素格式、座標慣例、校正值）：
//      像素格式只有三種「正規格式」：8 bit 灰階 → Gray8；16 bit 灰階 → Gray16；其他（彩色、有 alpha、索引色、
//      16 bit 彩色）→ Bgrx32（8 bit；16 bit 彩色換成 8 bit ＝ 四捨五入 v / 257，有損）。Bgrx32 的 x 沒有定義。
//      座標慣例見 geometry.h：第一列是檔案的第一列，不套用 EXIF 的旋轉。
//      校正值 umPerPixel 一律是 0：檔案裡的 DPI 不可信（常是 72、96 的預設值）；校正值由配方或使用者給。
//  - 檔案不存在回 ErrorCode::NotFound；格式無法解碼回 ErrorCode::FormatInvalid；其餘 I/O 失敗（例如路徑是資料夾）
//    回 ErrorCode::IoFailed。不丟例外。
//  - 回傳的 view 一定有 owner（可以交給別的執行緒、活得比 load 久），像素之後不再改（ImageView 規則 1）。
//  - canRead 只看檔名（副檔名，不分大小寫），不碰檔案；可以誤判成 true（load 再回 FormatInvalid）。
//  - load 可重入：可以同時被多個執行緒呼叫。
class IImageSource {
public:
    virtual ~IImageSource() = default;
    // path 是 UTF-8（含中文的路徑要能讀）。
    virtual bool canRead(std::string_view path) const = 0;
    virtual Result<ImageView> load(std::string_view path) = 0;
};

}  // namespace aoi
