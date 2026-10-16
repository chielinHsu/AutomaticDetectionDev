#pragma once

#include <cmath>
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
    // 有遮罩時是遮罩內的：ΣM·T 與 Σ T″²（T″ ＝ M·(T − 遮罩內平均)）。
    std::vector<double> sums;
    std::vector<double> centeredSquares;
    // 遮罩（ADR-028；沒有遮罩時是空的）：與 rotated 一一對應。masks 是 CV_8U（255 ＝ 參與），centered 是 T″（CV_32F），
    // maskCounts 是遮罩內的像素數。
    std::vector<cv::Mat> masks;
    std::vector<cv::Mat> centered;
    std::vector<double> maskCounts;
    bool masked() const noexcept { return !masks.empty(); }
};

// 比對區（ADR-028）：範本是比對區的像素，搜尋器回報的是主體中心。
struct OpenCvMatchGeometry {
    Size2D matchSize;    // 比對區（範本）的大小；整框時等於主體框
    Point2D originOffset;  // 主體中心 − 比對區中心（第 0 層、未旋轉的像素）；角度 θ 時是 rotate(originOffset, θ)
    MatchMode mode = MatchMode::Intensity;
    double featureScale = 0.0;  // 邊緣模式建範本時用的 edgeScale（搜尋要用同一個）
};

class OpenCvTemplate final : public ITemplate {
public:
    // levels[0] 是原尺寸，levels.back() 是頂層（搜尋從那裡開始）。
    // problem：建範本時就看得出來的樣本問題（例如樣本整塊是平的，NCC 沒有定義）。
    // geometry 沒給（matchSize 是 0）＝ 整框：比對區就是主體框。
    OpenCvTemplate(Size2D size, double angleMinDeg, double angleMaxDeg, std::vector<OpenCvTemplateLevel> levels,
                   std::optional<NoMatchReason> problem = std::nullopt, OpenCvMatchGeometry geometry = {});

    // 主體框的大小（use case 的 partSize）。
    Size2D size() const noexcept override { return size_; }
    // 範本（比對區）的大小、原點位移、特徵：搜尋器內部用。
    Size2D matchSize() const noexcept { return geometry_.matchSize; }
    Point2D originOffset() const noexcept { return geometry_.originOffset; }
    double originDistance() const noexcept { return std::hypot(geometry_.originOffset.x, geometry_.originOffset.y); }
    MatchMode mode() const noexcept { return geometry_.mode; }
    double featureScale() const noexcept { return geometry_.featureScale; }
    bool masked() const noexcept { return !levels_.empty() && levels_.front().masked(); }
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
    OpenCvMatchGeometry geometry_;
};

}  // namespace aoi::infra
