#include "infrastructure/backend_opencv/OpenCvFeatures.h"

#include <opencv2/imgproc.hpp>

namespace aoi::infra {

cv::Mat featureImage(const cv::Mat& gray8, MatchMode mode, double scale) {
    if (mode == MatchMode::Intensity || gray8.empty()) return gray8;
    cv::Mat gx;
    cv::Mat gy;
    cv::Sobel(gray8, gx, CV_32F, 1, 0, 3, 1.0, 0.0, cv::BORDER_REPLICATE);
    cv::Sobel(gray8, gy, CV_32F, 0, 1, 3, 1.0, 0.0, cv::BORDER_REPLICATE);
    cv::Mat magnitude;
    cv::magnitude(gx, gy, magnitude);
    cv::Mat out;
    magnitude.convertTo(out, CV_8U, scale);  // saturate_cast：超過 255 的夾到 255
    return out;
}

}  // namespace aoi::infra
