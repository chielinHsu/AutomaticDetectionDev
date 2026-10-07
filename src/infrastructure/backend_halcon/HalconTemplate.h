#pragma once

#include <halconcpp/HalconCpp.h>

#include <optional>
#include <string_view>

#include "application/ports/ITemplateBuilder.h"

// infrastructure/backend_halcon：HALCON 的比對模型（S08）：一個 shape model。只有 HalconTemplateBuilder 建、
// HalconPatternSearcher 與 HalconLibraryMatcher 讀；建好之後不再改。
//
// 多執行緒：find_shape_model 的「parallelization」資訊（get_operator_info 實際查）見 docs/steps/S08-halcon/spec.md；
// 同一個模型可以被幾個 worker 同時用來搜尋（tst_halcon_backend 的 concurrentSearchesGiveTheSameResult 驗證）。
namespace aoi::infra {

inline constexpr std::string_view kHalconBackendId = "halcon";

class HalconTemplate final : public ITemplate {
public:
    // model 無效（沒有初始化）時必須帶 problem：建範本時就看得出來的樣本問題（例如邊緣點太少，HALCON 建不出模型）。
    HalconTemplate(Size2D size, double angleMinDeg, double angleMaxDeg, HalconCpp::HShapeModel model, int numLevels,
                   std::optional<NoMatchReason> problem = std::nullopt);

    Size2D size() const noexcept override { return size_; }
    std::string_view backendId() const noexcept override { return kHalconBackendId; }
    std::optional<NoMatchReason> sampleProblem() const noexcept override { return problem_; }

    double angleMinDeg() const noexcept { return angleMinDeg_; }
    double angleMaxDeg() const noexcept { return angleMaxDeg_; }
    bool fullCircle() const noexcept { return angleMaxDeg_ - angleMinDeg_ >= 360.0 - 1e-9; }
    bool hasModel() const noexcept { return !problem_.has_value(); }
    const HalconCpp::HShapeModel& model() const noexcept { return model_; }
    // create_shape_model 決定的金字塔層數（'auto' 的結果）。
    int numLevels() const noexcept { return numLevels_; }

private:
    Size2D size_;
    double angleMinDeg_;
    double angleMaxDeg_;
    HalconCpp::HShapeModel model_;
    int numLevels_;
    std::optional<NoMatchReason> problem_;
};

}  // namespace aoi::infra
