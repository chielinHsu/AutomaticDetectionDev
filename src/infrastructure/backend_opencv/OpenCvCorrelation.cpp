#include "infrastructure/backend_opencv/OpenCvCorrelation.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <opencv2/imgproc.hpp>

namespace aoi::infra {

PatchIntegrals integralsOf(const cv::Mat& patch) {
    PatchIntegrals out;
    cv::integral(patch, out.sum, out.squares, CV_32S, CV_64F);
    return out;
}

void correlate(const cv::Mat& patch, const PatchIntegrals& integrals, const cv::Mat& templ, double templSum,
               double templCenteredSquares, cv::Mat& scores) {
    const int w = templ.cols;
    const int h = templ.rows;
    const int rows = patch.rows - h + 1;
    const int cols = patch.cols - w + 1;
    scores.create(rows, cols, CV_32F);
    const double n = static_cast<double>(w) * static_cast<double>(h);
    for (int y = 0; y < rows; ++y) {
        float* out = scores.ptr<float>(y);
        const int* s0 = integrals.sum.ptr<int>(y);
        const int* s1 = integrals.sum.ptr<int>(y + h);
        const double* q0 = integrals.squares.ptr<double>(y);
        const double* q1 = integrals.squares.ptr<double>(y + h);
        for (int x = 0; x < cols; ++x) {
            std::int64_t dot = 0;
            for (int j = 0; j < h; ++j) {
                const std::uint8_t* ip = patch.ptr<std::uint8_t>(y + j) + x;
                const std::uint8_t* tp = templ.ptr<std::uint8_t>(j);
                std::int32_t row = 0;  // 一列最多 255 × 255 × w：w < 33 000 不會溢位
                for (int i = 0; i < w; ++i) row += static_cast<std::int32_t>(ip[i]) * static_cast<std::int32_t>(tp[i]);
                dot += row;
            }
            const double sumI = static_cast<double>(s1[x + w] - s1[x] - s0[x + w] + s0[x]);
            const double sumI2 = q1[x + w] - q1[x] - q0[x + w] + q0[x];
            const double varI = sumI2 - sumI * sumI / n;
            const double denominator = std::sqrt(std::max(0.0, varI) * templCenteredSquares);
            const double numerator = static_cast<double>(dot) - templSum * sumI / n;
            out[x] = denominator > 1e-9 ? static_cast<float>(std::clamp(numerator / denominator, -1.0, 1.0)) : 0.0f;
        }
    }
}

bool correlateDirectly(const cv::Size& patch, const cv::Size& templ) noexcept {
    const double positions = static_cast<double>(patch.width - templ.width + 1) * static_cast<double>(patch.height - templ.height + 1);
    return positions * static_cast<double>(templ.area()) <= 4.0e6;
}

}  // namespace aoi::infra
