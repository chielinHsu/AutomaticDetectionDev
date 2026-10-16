#pragma once

#include <opencv2/core.hpp>

#include "domain/match_region.h"

// infrastructure/backend_opencv：比對用的特徵影像（ADR-028）。範本建立器與搜尋器在**同一個時點**套用：
// 轉 8 bit 灰階 → 建金字塔 → 每一層各自算特徵 → 之後才旋轉與裁切（避免裁切的邊界產生假的邊）。
namespace aoi::infra {

// Intensity：原樣回傳（不複製）。Edges：Sobel 3×3 的梯度大小 × scale，飽和到 8 bit（CV_8U）。
cv::Mat featureImage(const cv::Mat& gray8, MatchMode mode, double scale);

}  // namespace aoi::infra
