#pragma once

#include <opencv2/core.hpp>
#include <vector>

// infrastructure/backend_opencv：影像金字塔與座標換算（S07；範本建立器與搜尋器共用）。
//
// 座標用 ADR-010 的連續座標（domain/geometry.h）：像素 i 佔 [i, i + 1)，中心在 i + 0.5。
// cv::pyrDown 的第 l + 1 層像素 i，是以第 l 層像素 2i 為中心的加權平均，所以
//   x_l = 2·x_{l+1} − 0.5、x_{l+1} = (x_l + 0.5) ／ 2；往上 l 層：x_0 = 2^l·x_l − (2^l − 1) ／ 2。
// 這個換算由 tst_opencv_backend 用已知位置的合成元件鎖住（找到的中心誤差 < 0.25 像素）。
namespace aoi::infra {

// 第 0 層的連續座標 → 第 level 層。
double toPyramidLevel(double x0, int level) noexcept;
// 第 level 層的連續座標 → 第 0 層。
double fromPyramidLevel(double xl, int level) noexcept;

// levels[0] ＝ base（不複製），levels[l + 1] ＝ pyrDown(levels[l])；共 levelCount 層（至少 1）。
// 金字塔是演算法產生的新內容，不算影像複製（domain/image_copy.h）。
std::vector<cv::Mat> buildPyramid(const cv::Mat& base, int levelCount);

// 範本在第 level 層的邊長：side ／ 2^level 四捨五入，至少 1。
int sideAtPyramidLevel(int side, int level) noexcept;

}  // namespace aoi::infra
