#pragma once

#include <string_view>
#include <utility>

#include "application/ports/ITemplateBuilder.h"
#include "infrastructure/backend_halcon/HalconSearchOptions.h"
#include "infrastructure/backend_halcon/HalconTemplate.h"

// infrastructure/backend_halcon：ITemplateBuilder 的 HALCON 版（S08）。契約見 ITemplateBuilder.h。
//
// 從樣本建一個 shape model（create_shape_model）：
//  1. 樣本的矩形先裁到影像內、再四捨五入到整數像素（和 OpenCV 版相同；範本大小就是這個整數矩形的大小）。
//  2. 影像：Gray8 緊密排列時整張零複製；其他格式只轉換樣本外加 8 個像素的一塊（梯度要用到樣本外一圈的像素）。
//  3. 模型的區域（domain）就是樣本的矩形；HALCON 以區域的重心當參考點，也就是樣本框的中心（和 OpenCV 版相同）。
//  4. 參數照 HalconSearchOptions（預設全部 'auto'）；角度範圍來自 SearchParams（度 → 弧度）。
//  5. 樣本裡的邊緣點太少（H_ERR_NTPTS，例如整塊是平的）：回傳帶 sampleProblem ＝ LowContrast 的範本（不搜尋），不是錯誤。
// 取消：create_shape_model 不能中斷（get_operator_info(…, 'interrupt_mode') 實測是 'none'），只在呼叫前後檢查，
// 所以延遲的上限是一次建模型的時間（perf.md 的 T1）。無狀態，可以被多個 worker 同時呼叫。不丟例外。
namespace aoi::infra {

class HalconTemplateBuilder final : public ITemplateBuilder {
public:
    explicit HalconTemplateBuilder(HalconSearchOptions options = {}) : options_(std::move(options)) {}

    std::string_view backendId() const noexcept override { return kHalconBackendId; }
    Result<TemplatePtr> build(const ImageView& image, const Rect& sample, const SearchParams& params,
                              const CancelToken& cancel) override;
    // 比對區（ADR-028）：domain 改成比對區（矩形或遮罩的 runs），參考點是它的重心；範本記住「主體中心 − 參考點」。
    // 邊緣模式對 HALCON 沒有作用（shape model 本來就是邊緣，ADR-004 D7）。整框時和 build 逐位元相同。
    Result<TemplatePtr> buildFromSample(const ImageView& image, const SampleSpec& spec, const SearchParams& params,
                                        const CancelToken& cancel) override;

    const HalconSearchOptions& options() const noexcept { return options_; }

private:
    HalconSearchOptions options_;
};

}  // namespace aoi::infra
