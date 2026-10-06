#pragma once

#include <cstddef>
#include <opencv2/core.hpp>
#include <string_view>
#include <vector>

#include "application/ports/ITemplateBuilder.h"

// infrastructure/backend_opencv：OpenCV 的比對模型（S07）。只有 OpenCvTemplateBuilder 建、OpenCvPatternSearcher 讀；
// 建好之後不再改（可以被多個 worker 同時讀）。
namespace aoi::infra {

inline constexpr std::string_view kOpenCvBackendId = "opencv";

// 金字塔的一層：這一層的範本大小，與一組旋轉過的範本（8 bit 灰階）。
struct OpenCvTemplateLevel {
    int width = 0;
    int height = 0;
    double angleStepDeg = 0.0;           // 相鄰兩個角度的間隔；只有一個角度時是 0
    std::vector<double> anglesDeg;       // 由小到大
    std::vector<cv::Mat> rotated;        // 與 anglesDeg 一一對應：樣本在螢幕上逆時針轉了那個角度的樣子（ADR-010）
    // 每個旋轉範本的像素總和 ΣT 與 ΣT² − (ΣT)²／n（n ＝ 像素數）：小區域的 NCC 自己算時用（OpenCvCorrelation.h）。
    std::vector<double> sums;
    std::vector<double> centeredSquares;
};

class OpenCvTemplate final : public ITemplate {
public:
    // levels[0] 是原尺寸，levels.back() 是頂層（搜尋從那裡開始）。
    // problem：建範本時就看得出來的樣本問題（例如樣本整塊是平的，NCC 沒有定義）。
    OpenCvTemplate(Size2D size, double angleMinDeg, double angleMaxDeg, std::vector<OpenCvTemplateLevel> levels,
                   std::optional<NoMatchReason> problem = std::nullopt);

    Size2D size() const noexcept override { return size_; }
    std::string_view backendId() const noexcept override { return kOpenCvBackendId; }
    std::optional<NoMatchReason> sampleProblem() const noexcept override { return problem_; }

    double angleMinDeg() const noexcept { return angleMinDeg_; }
    double angleMaxDeg() const noexcept { return angleMaxDeg_; }
    // 角度範圍是整圈（例如 −180..180）：角度在頭尾相接。
    bool fullCircle() const noexcept { return fullCircle_; }
    const std::vector<OpenCvTemplateLevel>& levels() const noexcept { return levels_; }
    int topLevel() const noexcept { return static_cast<int>(levels_.size()) - 1; }
    std::size_t bytes() const noexcept;

    // 兩個角度的差（整圈時取最短的那一邊）。
    double angleDifference(double a, double b) const noexcept;

private:
    Size2D size_;
    double angleMinDeg_;
    double angleMaxDeg_;
    bool fullCircle_;
    std::vector<OpenCvTemplateLevel> levels_;
    std::optional<NoMatchReason> problem_;
};

}  // namespace aoi::infra
