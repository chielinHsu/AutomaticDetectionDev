#pragma once

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

#include "application/ports/IImageSource.h"

// application 層：只用標準庫。
namespace aoi {

// 幾個 IImageSource 依優先順序組成一個（S06）：例如 OpenCV（能讀 TIFF、16 bit）在前，Qt 在後當後備。
// 對外的契約與 IImageSource 相同（同一個檔案的語意不因由誰解碼而改變，這由各來源的契約保證）。
//
// 規則：
//  - 依加入的順序，只問 canRead(path) 為 true 的來源；第一個成功的結果就回傳。
//  - FormatInvalid（這個來源解不開這個檔案）→ 換下一個來源。
//  - NotFound、IoFailed 或其他錯誤 → 立刻回傳：換一個解碼器也讀不到檔案。
//  - 沒有任何來源認得這個檔名時，交給最後一個來源（後備）試一次：Qt 的 QImageReader 會看檔案內容判斷格式，
//    檔案不存在時也由它回 NotFound。
//  - 全部都是 FormatInvalid 時，回 FormatInvalid，訊息依序列出每個來源的原因。
//  - 擁有加入的來源。add 只在組裝時呼叫；load 與 canRead 可重入（各來源本身要可重入）。
class ImageSourceChain final : public IImageSource {
public:
    void add(std::unique_ptr<IImageSource> source);
    std::size_t size() const noexcept { return sources_.size(); }

    bool canRead(std::string_view path) const override;
    Result<ImageView> load(std::string_view path) override;

private:
    std::vector<std::unique_ptr<IImageSource>> sources_;
};

}  // namespace aoi
