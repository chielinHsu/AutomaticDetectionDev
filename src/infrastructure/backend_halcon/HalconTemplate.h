#pragma once

#include <halconcpp/HalconCpp.h>

#include <cmath>
#include <optional>
#include <string>
#include <string_view>

#include "application/ports/ITemplateBuilder.h"

// infrastructure/backend_halcon：HALCON 的比對模型（S08）：一個 shape model。只有 HalconTemplateBuilder 建、
// HalconPatternSearcher 與 HalconLibraryMatcher 讀；建好之後不再改。
//
// 多執行緒：find_shape_model 的「parallelization」資訊（get_operator_info 實際查）見 docs/steps/S08-halcon/spec.md；
// 同一個模型可以被幾個 worker 同時用來搜尋（tst_halcon_backend 的 concurrentSearchesGiveTheSameResult 驗證）。
namespace aoi::infra {

inline constexpr std::string_view kHalconBackendId = "halcon";

// 建範本時依樣本決定、搜尋時要跟著用的設定（2026-10-13，HalconSearchOptions.h 的「預先平滑」）：
// 搜尋影像要用和模型同一個半徑平滑，次像素模式也在這裡決定（邊很寬時 'least_squares'）。
struct HalconTemplateTuning {
    double edgeWidthPx = 0.0;  // 樣本的邊寬估計（量測用）
    double noiseRatio = 0.0;   // 樣本的相對雜訊估計（量測用）
    int smoothRadius = 0;      // mean_image 的半徑（0 ＝ 不平滑；做 HalconSearchOptions::meanPasses 次）
    std::string subPixel;      // find_shape_model 的 SubPixel；空 ＝ 用 HalconSearchOptions 的
    int lowestLevel = 1;       // find_shape_model 最低追蹤到哪一層（NumLevels 的第二個值；1 ＝ 到底）
};

// 比對區（ADR-028）：模型的參考點是 domain（比對區）的重心（HALCON 的預設，不呼叫 set_shape_model_origin），
// 搜尋器自己把參考點換成主體中心（和 OpenCV 版同一個做法；poseFromHalcon 的推導不變）。
struct HalconMatchGeometry {
    Size2D matchSize;          // 比對區的外接矩形；0 ＝ 整框（等於主體框）
    Point2D originOffset;      // 主體中心 − 參考點（ADR-010 的像素、未旋轉）；角度 θ 時是 rotate(originOffset, θ)
    double referenceReach = 0.0;  // 參考點到比對區最遠的角落（轉換影像的範圍）；0 ＝ 主體框的半對角線
};

class HalconTemplate final : public ITemplate {
public:
    // model 無效（沒有初始化）時必須帶 problem：建範本時就看得出來的樣本問題（例如邊緣點太少，HALCON 建不出模型）。
    HalconTemplate(Size2D size, double angleMinDeg, double angleMaxDeg, HalconCpp::HShapeModel model, int numLevels,
                   std::optional<NoMatchReason> problem = std::nullopt, HalconTemplateTuning tuning = {}, HalconMatchGeometry geometry = {});

    // 主體框的大小（use case 的 partSize）。
    Size2D size() const noexcept override { return size_; }
    Size2D matchSize() const noexcept { return geometry_.matchSize; }
    Point2D originOffset() const noexcept { return geometry_.originOffset; }
    double originDistance() const noexcept { return std::hypot(geometry_.originOffset.x, geometry_.originOffset.y); }
    double referenceReach() const noexcept { return geometry_.referenceReach; }
    std::string_view backendId() const noexcept override { return kHalconBackendId; }
    std::optional<NoMatchReason> sampleProblem() const noexcept override { return problem_; }

    double angleMinDeg() const noexcept { return angleMinDeg_; }
    double angleMaxDeg() const noexcept { return angleMaxDeg_; }
    bool fullCircle() const noexcept { return angleMaxDeg_ - angleMinDeg_ >= 360.0 - 1e-9; }
    bool hasModel() const noexcept { return !problem_.has_value(); }
    const HalconCpp::HShapeModel& model() const noexcept { return model_; }
    // create_shape_model 決定的金字塔層數（'auto' 的結果）。
    int numLevels() const noexcept { return numLevels_; }
    const HalconTemplateTuning& tuning() const noexcept { return tuning_; }

private:
    Size2D size_;
    double angleMinDeg_;
    double angleMaxDeg_;
    HalconCpp::HShapeModel model_;
    int numLevels_;
    std::optional<NoMatchReason> problem_;
    HalconTemplateTuning tuning_;
    HalconMatchGeometry geometry_;
};

}  // namespace aoi::infra
