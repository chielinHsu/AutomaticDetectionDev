#include "infrastructure/backend_opencv/OpenCvPyramid.h"

#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>

namespace aoi::infra {

double toPyramidLevel(double x0, int level) noexcept {
    const double scale = std::ldexp(1.0, level);
    return (x0 + 0.5 * (scale - 1.0)) / scale;
}

double fromPyramidLevel(double xl, int level) noexcept {
    const double scale = std::ldexp(1.0, level);
    return scale * xl - 0.5 * (scale - 1.0);
}

std::vector<cv::Mat> buildPyramid(const cv::Mat& base, int levelCount) {
    std::vector<cv::Mat> levels;
    levels.reserve(static_cast<std::size_t>(std::max(1, levelCount)));
    levels.push_back(base);
    for (int l = 1; l < levelCount; ++l) {
        cv::Mat next;
        cv::pyrDown(levels.back(), next);
        levels.push_back(next);
    }
    return levels;
}

int sideAtPyramidLevel(int side, int level) noexcept {
    return std::max(1, static_cast<int>(std::lround(std::ldexp(static_cast<double>(side), -level))));
}

}  // namespace aoi::infra
