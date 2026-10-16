#include "infrastructure/backend_opencv/OpenCvTemplateBuilder.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <opencv2/imgproc.hpp>
#include <string>
#include <utility>
#include <vector>

#include "infrastructure/backend_opencv/OpenCvFeatures.h"
#include "infrastructure/backend_opencv/OpenCvImages.h"
#include "infrastructure/backend_opencv/OpenCvPyramid.h"

namespace aoi::infra {
namespace {

Error cancelled() { return makeError(ErrorCode::Cancelled, "The OpenCV template build was cancelled."); }

// 一層的角度：從 min 到 max、間隔不超過 step。整圈時頭尾相接，不重複 −180 與 180。
std::vector<double> angleGrid(double minDeg, double maxDeg, double stepDeg, int maxCount, bool fullCircle) {
    const double range = maxDeg - minDeg;
    if (range <= 0.0 || stepDeg <= 0.0) return {minDeg};
    std::vector<double> angles;
    if (fullCircle) {
        const int n = std::clamp(static_cast<int>(std::ceil(360.0 / stepDeg - 1e-9)), 1, maxCount);
        for (int k = 0; k < n; ++k) angles.push_back(minDeg + 360.0 * k / n);
    } else {
        const int n = std::clamp(static_cast<int>(std::ceil(range / stepDeg - 1e-9)) + 1, 2, std::max(2, maxCount));
        for (int k = 0; k < n; ++k) angles.push_back(minDeg + range * k / (n - 1));
    }
    return angles;
}

// 把 source（某一層的方形影像）繞 center（這一層的連續座標）轉 angleDeg 度（螢幕上逆時針為正，ADR-010），
// 再從中間切出 width × height。OpenCV 的像素中心在整數座標，所以中心減 0.5。
cv::Mat rotatedCrop(const cv::Mat& source, cv::Point2d center, double angleDeg, int width, int height, int border = cv::BORDER_REPLICATE) {
    const cv::Point2f pivot(static_cast<float>(center.x - 0.5), static_cast<float>(center.y - 0.5));
    cv::Mat m = cv::getRotationMatrix2D(pivot, angleDeg, 1.0);
    m.at<double>(0, 2) += (width - 1) * 0.5 - pivot.x;
    m.at<double>(1, 2) += (height - 1) * 0.5 - pivot.y;
    cv::Mat out;
    cv::warpAffine(source, out, m, cv::Size(width, height), cv::INTER_LINEAR, border, cv::Scalar(0));
    return out;
}

double standardDeviation(const cv::Mat& m, const cv::Mat& mask = cv::Mat()) {
    cv::Scalar mean;
    cv::Scalar sd;
    cv::meanStdDev(m, mean, sd, mask);
    return sd[0];
}

// 遮罩的金字塔：和影像同一個 pyrDown（幾何一致），每層再二值化（≥ 一半算在裡面）。
std::vector<cv::Mat> maskPyramid(const cv::Mat& mask, int levelCount) {
    std::vector<cv::Mat> levels = buildPyramid(mask, levelCount);
    for (std::size_t l = 1; l < levels.size(); ++l) cv::threshold(levels[l], levels[l], 127, 255, cv::THRESH_BINARY);
    return levels;
}

}  // namespace

Result<TemplatePtr> OpenCvTemplateBuilder::build(const ImageView& image, const Rect& sample, const SearchParams& params,
                                                 const CancelToken& cancel) {
    return buildFromSample(image, SampleSpec{sample, {}}, params, cancel);
}

Result<TemplatePtr> OpenCvTemplateBuilder::buildFromSample(const ImageView& image, const SampleSpec& spec, const SearchParams& params,
                                                           const CancelToken& cancel) {
    try {
        const Rect& sample = spec.body;
        const MatchSpec& match = spec.match;
        if (!image.valid()) return makeError(ErrorCode::InvalidArgument, "The image is not valid.");
        if (!std::isfinite(sample.x) || !std::isfinite(sample.y) || !std::isfinite(sample.w) || !std::isfinite(sample.h)) {
            return makeError(ErrorCode::InvalidArgument, "The sample rectangle is not finite.");
        }
        if (!std::isfinite(params.angleMinDeg) || !std::isfinite(params.angleMaxDeg) || params.angleMinDeg > params.angleMaxDeg) {
            return makeError(ErrorCode::InvalidArgument, "The angle range is not valid.");
        }
        const Rect imageRect{0.0, 0.0, static_cast<double>(image.width), static_cast<double>(image.height)};
        const Rect clamped = intersection(sample, imageRect);
        if (clamped.empty()) return makeError(ErrorCode::InvalidArgument, "The sample is outside the image.");

        // 整數像素的樣本（四捨五入到最近的像素邊界，再裁到影像內）。
        const int x0 = std::clamp(static_cast<int>(std::lround(clamped.x)), 0, image.width - 1);
        const int y0 = std::clamp(static_cast<int>(std::lround(clamped.y)), 0, image.height - 1);
        const int x1 = std::clamp(static_cast<int>(std::lround(clamped.right())), x0 + 1, image.width);
        const int y1 = std::clamp(static_cast<int>(std::lround(clamped.bottom())), y0 + 1, image.height);
        const int width = x1 - x0;
        const int height = y1 - y0;

        // 比對區（ADR-028）：沒有設定 ＝ 主體框。矩形用和主體框一樣的取整；遮罩已經在整數像素上，裁到影像內。
        int mx0 = x0;
        int my0 = y0;
        int mw = width;
        int mh = height;
        cv::Mat regionMask;  // mw × mh，255 ＝ 參與；空 ＝ 整個矩形
        if (match.rect) {
            const Rect& r = *match.rect;
            if (!std::isfinite(r.x) || !std::isfinite(r.y) || !std::isfinite(r.w) || !std::isfinite(r.h)) {
                return makeError(ErrorCode::InvalidArgument, "The match region is not finite.");
            }
            if (!match.mask.empty()) {
                const cv::Rect bounds(match.mask.x, match.mask.y, match.mask.width, match.mask.height);
                const cv::Rect inside = bounds & cv::Rect(0, 0, image.width, image.height);
                if (inside.empty()) return makeError(ErrorCode::InvalidArgument, "The match mask is outside the image.");
                regionMask = cv::Mat::zeros(inside.height, inside.width, CV_8U);
                for (int y = 0; y < inside.height; ++y) {
                    std::uint8_t* row = regionMask.ptr<std::uint8_t>(y);
                    for (int x = 0; x < inside.width; ++x) row[x] = match.mask.at(inside.x + x, inside.y + y) ? 255 : 0;
                }
                const int count = cv::countNonZero(regionMask);
                if (count == 0) return makeError(ErrorCode::InvalidArgument, "The match mask is empty inside the image.");
                mx0 = inside.x;
                my0 = inside.y;
                mw = inside.width;
                mh = inside.height;
                if (static_cast<double>(count) >= options_.maskAsRectFill * static_cast<double>(regionMask.total())) regionMask.release();
            } else {
                const Rect c = intersection(r, imageRect);
                if (c.empty()) return makeError(ErrorCode::InvalidArgument, "The match region is outside the image.");
                mx0 = std::clamp(static_cast<int>(std::lround(c.x)), 0, image.width - 1);
                my0 = std::clamp(static_cast<int>(std::lround(c.y)), 0, image.height - 1);
                mw = std::clamp(static_cast<int>(std::lround(c.right())), mx0 + 1, image.width) - mx0;
                mh = std::clamp(static_cast<int>(std::lround(c.bottom())), my0 + 1, image.height) - my0;
            }
        }
        const bool masked = !regionMask.empty();
        OpenCvMatchGeometry geometry;
        geometry.matchSize = Size2D{static_cast<double>(mw), static_cast<double>(mh)};
        geometry.originOffset = Point2D{(x0 + width * 0.5) - (mx0 + mw * 0.5), (y0 + height * 0.5) - (my0 + mh * 0.5)};
        geometry.mode = match.mode;
        geometry.featureScale = options_.edgeScale;
        const Size2D bodySize{static_cast<double>(width), static_cast<double>(height)};
        if (cancel.cancelled()) return cancelled();

        int levels = 0;
        while (levels < options_.maxLevels &&
               std::min(sideAtPyramidLevel(mw, levels + 1), sideAtPyramidLevel(mh, levels + 1)) >= options_.minTopSidePx) {
            ++levels;
        }
        const int levelCount = levels + 1;

        // 比對區周圍的方形：半徑 ＝ 半對角線 ＋ 頂層 2 個像素的餘量，轉任何角度都不會切到空白。
        const double cx = mx0 + mw * 0.5;
        const double cy = my0 + mh * 0.5;
        const double radius = 0.5 * std::hypot(mw, mh) + std::ldexp(2.0, levels) + 2.0;
        const cv::Rect patch(static_cast<int>(std::floor(cx - radius)), static_cast<int>(std::floor(cy - radius)),
                             static_cast<int>(std::ceil(2.0 * radius)) + 1, static_cast<int>(std::ceil(2.0 * radius)) + 1);
        const cv::Rect inside = patch & cv::Rect(0, 0, image.width, image.height);
        const GrayRoi gray = toGray8(image, inside);
        if (gray.mat.empty()) return makeError(ErrorCode::Internal, "Cannot convert the sample to gray.");
        cv::Mat context;
        cv::copyMakeBorder(gray.mat, context, inside.y - patch.y, patch.br().y - inside.br().y, inside.x - patch.x,
                           patch.br().x - inside.br().x, cv::BORDER_REPLICATE);
        std::vector<cv::Mat> pyramid = buildPyramid(context, levelCount);
        if (match.mode != MatchMode::Intensity) {
            for (cv::Mat& level : pyramid) level = featureImage(level, match.mode, options_.edgeScale);
        }
        std::vector<cv::Mat> maskLevels;
        if (masked) {
            cv::Mat maskContext = cv::Mat::zeros(context.size(), CV_8U);
            regionMask.copyTo(maskContext(cv::Rect(mx0 - patch.x, my0 - patch.y, mw, mh)));
            maskLevels = maskPyramid(maskContext, levelCount);
        }

        const double angleMin = params.angleMinDeg;
        const double angleMax = params.angleMaxDeg;
        const bool fullCircle = angleMax - angleMin >= 360.0 - 1e-9;
        const cv::Point2d center0(cx - patch.x, cy - patch.y);  // 比對區中心在方形裡的連續座標（第 0 層）
        const auto withProblem = [&](OpenCvTemplateLevel level, NoMatchReason reason) {
            return TemplatePtr(std::make_shared<const OpenCvTemplate>(bodySize, angleMin, angleMax,
                                                                      std::vector<OpenCvTemplateLevel>{std::move(level)}, reason, geometry));
        };

        std::vector<OpenCvTemplateLevel> built;
        built.reserve(static_cast<std::size_t>(levelCount));
        for (int l = 0; l < levelCount; ++l) {
            if (cancel.cancelled()) return cancelled();
            OpenCvTemplateLevel level;
            level.width = sideAtPyramidLevel(mw, l);
            level.height = sideAtPyramidLevel(mh, l);
            const double halfDiagonal = 0.5 * std::hypot(level.width, level.height);
            const double step = radiansToDegrees(std::atan2(options_.cornerShiftPx, halfDiagonal));
            const std::size_t area = static_cast<std::size_t>(level.width) * static_cast<std::size_t>(level.height);
            const std::size_t byBytes = options_.maxTemplateBytes / static_cast<std::size_t>(levelCount) / std::max<std::size_t>(1, area);
            const int maxCount = static_cast<int>(std::clamp<std::size_t>(byBytes, 1, static_cast<std::size_t>(options_.maxAnglesPerLevel)));
            level.anglesDeg = angleGrid(angleMin, angleMax, step, maxCount, fullCircle);
            level.angleStepDeg = level.anglesDeg.size() < 2 ? 0.0
                                 : fullCircle ? 360.0 / static_cast<double>(level.anglesDeg.size())
                                              : (angleMax - angleMin) / static_cast<double>(level.anglesDeg.size() - 1);
            const cv::Point2d centerL(toPyramidLevel(center0.x, l), toPyramidLevel(center0.y, l));
            level.rotated.reserve(level.anglesDeg.size());
            bool maskTooSmall = false;
            for (const double a : level.anglesDeg) {
                cv::Mat r = rotatedCrop(pyramid[static_cast<std::size_t>(l)], centerL, a, level.width, level.height);
                if (!masked) {
                    const double sum = cv::sum(r)[0];
                    level.sums.push_back(sum);
                    level.centeredSquares.push_back(r.dot(r) - sum * sum / static_cast<double>(r.total()));
                } else {
                    cv::Mat m = rotatedCrop(maskLevels[static_cast<std::size_t>(l)], centerL, a, level.width, level.height, cv::BORDER_CONSTANT);
                    cv::threshold(m, m, 127, 255, cv::THRESH_BINARY);
                    const int n = cv::countNonZero(m);
                    if (n < options_.minMaskPixels) maskTooSmall = true;
                    const double mean = n > 0 ? cv::mean(r, m)[0] : 0.0;
                    cv::Mat centered;
                    r.convertTo(centered, CV_32F, 1.0, -mean);
                    centered.setTo(0.0f, m == 0);
                    level.sums.push_back(mean * n);
                    level.centeredSquares.push_back(centered.dot(centered));
                    level.masks.push_back(std::move(m));
                    level.centered.push_back(std::move(centered));
                    level.maskCounts.push_back(static_cast<double>(n));
                }
                level.rotated.push_back(std::move(r));
            }

            // 縮小後整塊變平的層，NCC 沒有定義：這一層（與更上面的）不用。第 0 層就是平的 → 樣本對比太低。
            // 遮罩縮到太小的層也一樣；第 0 層就太小 → 比對區太小。
            if (maskTooSmall) {
                if (l == 0) return withProblem(std::move(level), NoMatchReason::SampleTooSmall);
                break;
            }
            const std::size_t mid = level.rotated.size() / 2;
            const double sd = masked ? standardDeviation(level.rotated[mid], level.masks[mid]) : standardDeviation(level.rotated[mid]);
            if (sd < 1.0) {
                if (l == 0) return withProblem(std::move(level), NoMatchReason::LowContrast);
                break;
            }
            built.push_back(std::move(level));
        }
        return TemplatePtr(std::make_shared<const OpenCvTemplate>(bodySize, angleMin, angleMax, std::move(built), std::nullopt, geometry));
    } catch (const cv::Exception& e) {
        return makeError(ErrorCode::BackendFailed, std::string("OpenCV failed while building the template: ") + e.what());
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::BackendFailed, "Not enough memory to build the template.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while building the template.");
    }
}

}  // namespace aoi::infra
