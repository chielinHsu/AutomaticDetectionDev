#include "infrastructure/backend_opencv/OpenCvSolderRegionDetector.h"

#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include "infrastructure/backend_opencv/OpenCvImages.h"

namespace aoi::infra {
Result<std::vector<Rect>> OpenCvSolderRegionDetector::detect(const ImageView& image, const Rect& region,
    const SolderDetectionOptions& options, const CancelToken& cancel) const {
    try {
        if (cancel.cancelled()) return makeError(ErrorCode::Cancelled, "Detection cancelled.");
        if (!image.valid() || !std::isfinite(region.x) || !std::isfinite(region.y) || !std::isfinite(region.w) ||
            !std::isfinite(region.h) || region.x < 0 || region.y < 0 || region.w < 2 || region.h < 2 ||
            region.x+region.w > image.width || region.y+region.h > image.height ||
            options.threshold < 0 || options.threshold > 255 || options.minArea < 1 ||
            options.maxArea < options.minArea || options.padding < 0 || options.padding > 100 ||
            options.maxBoxes < 1 || options.maxBoxes > 5000)
            return makeError(ErrorCode::InvalidArgument, "Invalid detection region or parameters.");
        // 只取完全落在選區內的像素；不因小數座標的取整把框擴到選區外。
        const int x = static_cast<int>(std::ceil(region.x)), y = static_cast<int>(std::ceil(region.y));
        const int right = static_cast<int>(std::floor(region.x+region.w));
        const int bottom = static_cast<int>(std::floor(region.y+region.h));
        if (right-x < 2 || bottom-y < 2) return std::vector<Rect>{};
        const cv::Rect roi(x, y, right-x, bottom-y);
        const GrayRoi gray = toGray8(image, roi);
        if (gray.mat.empty()) return makeError(ErrorCode::FormatInvalid, "Cannot convert this image to grayscale.");
        cv::Mat mask;
        int mode = options.darkForeground ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY;
        if (options.threshold == 0) mode |= cv::THRESH_OTSU;
        cv::threshold(gray.mat, mask, options.threshold, 255, mode);
        if (cancel.cancelled()) return makeError(ErrorCode::Cancelled, "Detection cancelled.");
        cv::Mat labels, stats, centroids;
        const int count = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8, CV_32S);
        std::vector<Rect> boxes;
        for (int i = 1; i < count; ++i) {
            if (cancel.cancelled()) return makeError(ErrorCode::Cancelled, "Detection cancelled.");
            const int area = stats.at<int>(i, cv::CC_STAT_AREA);
            const int w = stats.at<int>(i, cv::CC_STAT_WIDTH), h = stats.at<int>(i, cv::CC_STAT_HEIGHT);
            if (area < options.minArea || area > options.maxArea || w < 2 || h < 2) continue;
            const int left = stats.at<int>(i, cv::CC_STAT_LEFT), top = stats.at<int>(i, cv::CC_STAT_TOP);
            // 穿越圈選邊界的亮塊不是完整焊點，保守忽略；UI 提示使用者在焊點外留邊。
            if (left == 0 || top == 0 || left+w >= roi.width || top+h >= roi.height) continue;
            const int x0 = std::max(0, left-options.padding), y0 = std::max(0, top-options.padding);
            const int x1 = std::min(roi.width, left+w+options.padding), y1 = std::min(roi.height, top+h+options.padding);
            boxes.push_back(Rect{static_cast<double>(x+x0), static_cast<double>(y+y0),
                                 static_cast<double>(x1-x0), static_cast<double>(y1-y0)});
            if (boxes.size() > static_cast<std::size_t>(options.maxBoxes))
                return makeError(ErrorCode::InvalidArgument, "Too many regions. Narrow the selection or increase the minimum area.");
        }
        std::sort(boxes.begin(), boxes.end(), [](const Rect& a, const Rect& b) {
            return a.y == b.y ? a.x < b.x : a.y < b.y;
        });
        return boxes;
    } catch (const cv::Exception& e) {
        return makeError(ErrorCode::BackendFailed, e.what());
    } catch (const std::exception& e) {
        return makeError(ErrorCode::BackendFailed, e.what());
    } catch (...) {
        return makeError(ErrorCode::BackendFailed, "Unexpected detection failure.");
    }
}
}  // namespace aoi::infra
