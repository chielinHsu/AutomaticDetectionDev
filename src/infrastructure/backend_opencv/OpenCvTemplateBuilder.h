#pragma once

#include <string_view>

#include "application/ports/ITemplateBuilder.h"
#include "infrastructure/backend_opencv/OpenCvSearchOptions.h"
#include "infrastructure/backend_opencv/OpenCvTemplate.h"

// infrastructure/backend_opencv：ITemplateBuilder 的 OpenCV 版（S07）。契約見 ITemplateBuilder.h。
//
// 從影像裁出樣本，建出「每一層、每一個角度」的範本（OpenCvTemplate）：
//  1. 金字塔的層數：每層縮小一半，直到範本的短邊剩 options.minTopSidePx（64×32 的樣本是 3 層：64×32、32×16、16×8）。
//  2. 每一層的角度步進：範本轉一步，角落移動 options.cornerShiftPx 個像素（大的範本步進細）；角度範圍來自 SearchParams。
//  3. 旋轉的範本不是把裁好的矩形轉一轉（那樣四個角會是空的）：先裁一塊夠大的方形（含樣本周圍的影像），
//     縮小、轉過之後再從中間切出範本大小。樣本貼著影像邊緣時，影像外面用邊緣的像素補（BORDER_REPLICATE）。
//  4. 樣本整塊是平的（標準差 < 1 灰階）：NCC 沒有定義，回傳的範本帶 sampleProblem ＝ LowContrast（不搜尋）。
// 樣本的矩形先裁到影像內、再四捨五入到整數像素；範本大小就是這個整數矩形的大小（use case 的主體框大小）。
// 比對區（ADR-026，buildFromSample）：範本改用比對區的像素（矩形，或遮罩的外接矩形＋遮罩），範本記住
// 「主體中心 − 比對區中心」；邊緣模式在每一層的梯度大小上建範本（OpenCvFeatures.h）。整框時和 build 逐行相同。
// 遮罩縮小或轉過之後剩不到 options.minMaskPixels 個像素的層不用；第 0 層就不夠 → sampleProblem ＝ SampleTooSmall。
// 無狀態（options 建立後不改），可以被多個 worker 同時呼叫。不丟例外（cv::Exception 在這裡攔下）。
namespace aoi::infra {

class OpenCvTemplateBuilder final : public ITemplateBuilder {
public:
    explicit OpenCvTemplateBuilder(OpenCvSearchOptions options = {}) : options_(options) {}

    std::string_view backendId() const noexcept override { return kOpenCvBackendId; }
    Result<TemplatePtr> build(const ImageView& image, const Rect& sample, const SearchParams& params,
                              const CancelToken& cancel) override;
    Result<TemplatePtr> buildFromSample(const ImageView& image, const SampleSpec& spec, const SearchParams& params,
                                        const CancelToken& cancel) override;

    const OpenCvSearchOptions& options() const noexcept { return options_; }

private:
    OpenCvSearchOptions options_;
};

}  // namespace aoi::infra
