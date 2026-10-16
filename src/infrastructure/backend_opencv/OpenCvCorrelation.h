#pragma once

#include <opencv2/core.hpp>

// infrastructure/backend_opencv：正規化相關係數（S07）。和 cv::matchTemplate 的 TM_CCOEFF_NORMED 是同一個定義：
//   R(x, y) ＝ Σ T'·I' ／ √(Σ T'² · Σ I'²)，T' ＝ T − 範本平均，I' ＝ I − 這個位置的視窗平均。
//
// 為什麼自己算：精修時每個候選只比幾十個位置（± 2 像素），cv::matchTemplate 對這麼小的分數圖仍走 DFT 的路，
// 固定成本遠大於計算本身（S07 實測：5 MP 的種子搜尋 224 ms 裡幾乎都是精修，perf.md）。小的分數圖直接算點積：
//   Σ T'·I' ＝ Σ T·I − ΣT·ΣI ／ n（Σ T·I 是 8 bit × 8 bit 的整數點積），ΣI、ΣI² 用積分影像，每個位置 O(1)。
// 大的分數圖（頂層的整張影像）仍用 cv::matchTemplate（DFT 比直接算快）。
namespace aoi::infra {

// 一塊影像的積分影像（ΣI 與 ΣI²），同一塊比好幾個角度時共用。
struct PatchIntegrals {
    cv::Mat sum;     // CV_32S，(rows + 1) × (cols + 1)
    cv::Mat squares; // CV_64F
};
PatchIntegrals integralsOf(const cv::Mat& patch);

// 分數圖（CV_32F，(patch.rows − h + 1) × (patch.cols − w + 1)）。patch 與 templ 都是 8 bit 單通道，patch 不比範本小。
// templSum ＝ ΣT，templCenteredSquares ＝ ΣT² − (ΣT)²／n。視窗是平的（變異數 0）或範本是平的：分數 0（和 OpenCV 相同）。
void correlate(const cv::Mat& patch, const PatchIntegrals& integrals, const cv::Mat& templ, double templSum,
               double templCenteredSquares, cv::Mat& scores);

// 直接算比 cv::matchTemplate 划算嗎？（分數圖的位置數 × 範本像素數不超過門檻）
bool correlateDirectly(const cv::Size& patch, const cv::Size& templ) noexcept;

// ---- 遮罩（ADR-028）----
// 只看遮罩 M 內的像素的 NCC：R ＝ Σ T″·I ／ √(Σ T″² · (Σ M·I² − (Σ M·I)²／n))，T″ ＝ M·(T − 遮罩內的範本平均)，n ＝ Σ M。
// 和 cv::matchTemplate(TM_CCOEFF_NORMED, mask) 是同一個定義（tst_opencv_backend 對照）。
// centered ＝ T″（CV_32F，遮罩外是 0），mask 是 CV_8U（非 0 ＝ 參與），maskCount ＝ n，centeredSquares ＝ Σ T″²。
// 視窗在遮罩內是平的或範本是平的：分數 0。
void correlateMasked(const cv::Mat& patch, const cv::Mat& centered, const cv::Mat& mask, double maskCount, double centeredSquares,
                     cv::Mat& scores);

// cv::matchTemplate 帶遮罩時，分母是 0 的位置會給 inf 或 NaN（OpenCV 不帶遮罩時給 0）。不是有限值、或 |s| 明顯超過 1 的
// 位置設成 0（不能夾成 1：那會變成假的峰），其餘夾到 [−1, 1]。
void sanitizeScores(cv::Mat& scores);

}  // namespace aoi::infra
