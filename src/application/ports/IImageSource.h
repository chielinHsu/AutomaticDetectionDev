#pragma once

#include <string_view>

#include "application/Result.h"
#include "domain/image_view.h"

// application 層：port。實作在 S06（OpenCV、HALCON、Qt 的後備）。
namespace aoi {

// 契約：同一個檔案，所有實作回傳相同語意的 ImageView（像素格式、座標慣例、校正值）。
// 檔案不存在回 ErrorCode::NotFound；格式無法解碼回 ErrorCode::FormatInvalid；其餘 I/O 失敗回 ErrorCode::IoFailed。不丟例外。
class IImageSource {
public:
    virtual ~IImageSource() = default;
    // path 是 UTF-8。
    virtual bool canRead(std::string_view path) const = 0;
    virtual Result<ImageView> load(std::string_view path) = 0;
};

}  // namespace aoi
