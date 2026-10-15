#include "infrastructure/backend_halcon/HalconSearchSteps.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "domain/lattice/non_max_suppression.h"
#include "infrastructure/backend_halcon/HalconPose.h"

namespace aoi::infra {
namespace {

// 把角度放回 [min, max]（整圈的模型會給出差 360° 的同一個角度；HALCON 也說最後的角度可能稍微超出範圍）；放不回去就夾在邊上。
double angleIntoRange(double a, const SearchParams& params) {
    for (int turn = -2; turn <= 2; ++turn) {
        const double shifted = a + 360.0 * turn;
        if (shifted >= params.angleMinDeg && shifted <= params.angleMaxDeg) return shifted;
    }
    return std::clamp(a, params.angleMinDeg, params.angleMaxDeg);
}

// 一塊 'byte' 或 'uint2' 的 HALCON 影像讀成 double（一列接一列）。不支援的型別回傳空的。
std::vector<double> pixelsOf(const HalconCpp::HImage& image, Hlong* width, Hlong* height) {
    HalconCpp::HString type;
    const void* pixels = image.GetImagePointer1(&type, width, height);
    std::vector<double> out;
    if (pixels == nullptr || *width <= 0 || *height <= 0) return out;
    const std::size_t n = static_cast<std::size_t>(*width) * static_cast<std::size_t>(*height);
    out.resize(n);
    if (std::strcmp(type.Text(), "byte") == 0) {
        const auto* p = static_cast<const std::uint8_t*>(pixels);
        for (std::size_t i = 0; i < n; ++i) out[i] = p[i];
    } else if (std::strcmp(type.Text(), "uint2") == 0) {
        const auto* p = static_cast<const std::uint16_t*>(pixels);
        for (std::size_t i = 0; i < n; ++i) out[i] = p[i];
    } else {
        out.clear();
    }
    return out;
}

}  // namespace

int halconWindowGrow(int numLevels) noexcept { return std::max(2, 1 << std::clamp(numLevels - 1, 0, 10)); }

HalconCpp::HImage smoothHalconImage(const HalconCpp::HImage& image, int radius, int passes) {
    HalconCpp::HImage out = image;
    const Hlong size = 2 * static_cast<Hlong>(std::max(0, radius)) + 1;
    for (int pass = 0; pass < passes && radius > 0; ++pass) out = out.MeanImage(size, size);
    return out;
}

HalconSampleStatistics estimateSampleStatistics(const HalconCpp::HImage& gray, const HalconRect1& rect) {
    HalconSampleStatistics stats;
    if (!gray.IsInitialized()) return stats;
    Hlong imageWidth = 0;
    Hlong imageHeight = 0;
    gray.GetImageSize(&imageWidth, &imageHeight);
    // 往外一個像素（梯度與 3×3 平均要用到），裁到影像內。
    const Hlong r1 = std::clamp(static_cast<Hlong>(std::lround(rect.row1)) - 1, Hlong{0}, imageHeight - 1);
    const Hlong c1 = std::clamp(static_cast<Hlong>(std::lround(rect.column1)) - 1, Hlong{0}, imageWidth - 1);
    const Hlong r2 = std::clamp(static_cast<Hlong>(std::lround(rect.row2)) + 1, Hlong{0}, imageHeight - 1);
    const Hlong c2 = std::clamp(static_cast<Hlong>(std::lround(rect.column2)) + 1, Hlong{0}, imageWidth - 1);
    if (r2 - r1 < 2 || c2 - c1 < 2) return stats;
    Hlong w = 0;
    Hlong h = 0;
    const std::vector<double> px = pixelsOf(gray.CropRectangle1(r1, c1, r2, c2), &w, &h);
    if (px.empty()) return stats;
    const auto at = [&](Hlong x, Hlong y) { return px[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)]; };
    std::vector<double> values;
    std::vector<double> grads;
    std::vector<double> residuals;
    values.reserve(static_cast<std::size_t>((w - 2) * (h - 2)));
    grads.reserve(values.capacity());
    residuals.reserve(values.capacity());
    for (Hlong y = 1; y + 1 < h; ++y) {
        for (Hlong x = 1; x + 1 < w; ++x) {
            values.push_back(at(x, y));
            const double gx = (at(x + 1, y - 1) + 2.0 * at(x + 1, y) + at(x + 1, y + 1)) - (at(x - 1, y - 1) + 2.0 * at(x - 1, y) + at(x - 1, y + 1));
            const double gy = (at(x - 1, y + 1) + 2.0 * at(x, y + 1) + at(x + 1, y + 1)) - (at(x - 1, y - 1) + 2.0 * at(x, y - 1) + at(x + 1, y - 1));
            grads.push_back(std::hypot(gx, gy) / 8.0);
            double sum = 0.0;
            for (Hlong dy = -1; dy <= 1; ++dy) {
                for (Hlong dx = -1; dx <= 1; ++dx) sum += at(x + dx, y + dy);
            }
            residuals.push_back(std::abs(at(x, y) - sum / 9.0));
        }
    }
    if (values.empty()) return stats;
    std::sort(values.begin(), values.end());
    std::sort(grads.begin(), grads.end());
    std::sort(residuals.begin(), residuals.end());
    const double p5 = values[values.size() / 20];
    const double p95 = values[values.size() * 19 / 20];
    const double g99 = grads[grads.size() * 99 / 100];
    const double range = p95 - p5;
    if (!(range > 0.0)) return stats;
    if (g99 > 0.0) stats.edgeWidthPx = range / g99;
    stats.noiseRatio = 1.4826 * residuals[residuals.size() / 2] / range;
    return stats;
}

HalconSmoothing chooseHalconSmoothing(const HalconSampleStatistics& stats, const HalconSearchOptions& options) {
    HalconSmoothing s;
    s.subPixel = options.subPixel;
    if (!options.adaptiveSmoothing) return s;
    if (stats.edgeWidthPx > 0.0 && stats.edgeWidthPx >= options.wideEdgeWidth) {
        s.smoothRadius = std::max(0, options.wideRadius);
        s.subPixel = "least_squares";
        s.lowestLevel = std::max(1, options.wideEdgeLowestLevel);
    } else if (stats.noiseRatio >= options.noisyRatio) {
        s.smoothRadius = std::max(0, options.noisyRadius);
        s.subPixel = options.noisySubPixel;
    }
    return s;
}

HalconSearchInput prepareHalconSearch(const ImageView& image, const std::vector<Rect>& accepts, bool windows, double halfExtent,
                                      int grow, bool nativeGray16, int smoothRadius, int smoothPasses) {
    HalconSearchInput in;
    const bool zeroCopy = image.packed() && (image.format == PixelFormat::Gray8 || (nativeGray16 && image.format == PixelFormat::Gray16));
    PixelRect roi{0, 0, image.width, image.height};
    if (windows && (!zeroCopy || smoothRadius > 0) && !accepts.empty()) {
        // 只轉換（平滑）用得到的那一塊：所有視窗的外接矩形，再加範本的一半（參考點在視窗邊上時，模型的像素在外面）與濾波器的範圍。
        const double margin = halfExtent + grow + 2.0 + smoothRadius * smoothPasses;
        double left = accepts.front().x;
        double top = accepts.front().y;
        double right = accepts.front().right();
        double bottom = accepts.front().bottom();
        for (const Rect& a : accepts) {
            left = std::min(left, a.x);
            top = std::min(top, a.y);
            right = std::max(right, a.right());
            bottom = std::max(bottom, a.bottom());
        }
        const int rx0 = std::max(0, static_cast<int>(std::floor(left - margin)));
        const int ry0 = std::max(0, static_cast<int>(std::floor(top - margin)));
        const int rx1 = std::min(image.width, static_cast<int>(std::ceil(right + margin)));
        const int ry1 = std::min(image.height, static_cast<int>(std::ceil(bottom + margin)));
        roi = {rx0, ry0, rx1 - rx0, ry1 - ry0};
    }
    in.gray = toHalconGray(image, roi, nativeGray16);
    if (!in.gray.image.IsInitialized()) return in;
    if (smoothRadius > 0) {
        // HALCON 配置新的影像（像素不再是 view 的），keepAlive 留著沒有害處。
        in.gray.image = smoothHalconImage(in.gray.image, smoothRadius, smoothPasses);
    }
    if (!windows) {
        in.searchImage = in.gray.image;
        return in;
    }
    // 參考點的像素中心在視窗裡：像素 i 的中心是 i ＋ 0.5，所以像素的範圍是 [ceil(x − 0.5), ceil(right − 0.5) − 1]，再往外 grow。
    HalconCpp::HTuple r1, c1, r2, c2;
    for (const Rect& a : accepts) {
        r1.Append(static_cast<double>(static_cast<int>(std::ceil(a.y - 0.5)) - grow - in.gray.originY));
        c1.Append(static_cast<double>(static_cast<int>(std::ceil(a.x - 0.5)) - grow - in.gray.originX));
        r2.Append(static_cast<double>(static_cast<int>(std::ceil(a.bottom() - 0.5)) - 1 + grow - in.gray.originY));
        c2.Append(static_cast<double>(static_cast<int>(std::ceil(a.right() - 0.5)) - 1 + grow - in.gray.originX));
    }
    HalconCpp::HRegion rectangles;
    rectangles.GenRectangle1(r1, c1, r2, c2);
    in.searchImage = in.gray.image.ReduceDomain(rectangles.Union1());
    return in;
}

std::vector<Rect> referenceWindows(const std::vector<Rect>& accepts, Point2D originOffset, double angleMinDeg, double angleMaxDeg) {
    const double length = std::hypot(originOffset.x, originOffset.y);
    if (length == 0.0) return accepts;
    const double half = 0.5 * (angleMaxDeg - angleMinDeg);
    Point2D shift{0.0, 0.0};
    double grow = length;
    if (half < 90.0) {
        const Point2D r = rotate(originOffset, angleMinDeg + half);
        shift = {-r.x, -r.y};
        grow = length * 2.0 * std::sin(degreesToRadians(half) * 0.5) + 1.0;
    }
    std::vector<Rect> out;
    out.reserve(accepts.size());
    for (const Rect& a : accepts) out.push_back({a.x + shift.x - grow, a.y + shift.y - grow, a.w + 2.0 * grow, a.h + 2.0 * grow});
    return out;
}

std::vector<Match> halconMatches(const HalconCpp::HTuple& rows, const HalconCpp::HTuple& columns, const HalconCpp::HTuple& angles,
                                 const HalconCpp::HTuple& scores, const std::vector<Hlong>& indices, const HalconGray& gray,
                                 const std::vector<Rect>& accepts, const SearchParams& params, Size2D size, double nmsMaxOverlap,
                                 Point2D originOffset) {
    std::vector<Match> matches;
    std::vector<ScoredBox> boxes;
    const bool shifted = originOffset.x != 0.0 || originOffset.y != 0.0;
    const auto take = [&](Hlong i) {
        const double score = scores[i].D();
        if (!(score >= params.minScore)) return;
        Pose2D pose = poseFromHalcon(rows[i].D(), columns[i].D(), angles[i].D(), gray.originX, gray.originY);
        const Pose2D reference = pose;
        if (shifted) {
            const Point2D r = rotate(originOffset, pose.angleDeg);
            pose.x += r.x;
            pose.y += r.y;
        }
        const Point2D center = pose.position();
        if (std::none_of(accepts.begin(), accepts.end(), [&](const Rect& a) { return a.contains(center); })) return;
        pose.angleDeg = angleIntoRange(pose.angleDeg, params);
        matches.push_back({pose, std::clamp(score, 0.0, 1.0)});
        boxes.push_back({RectObj{Pose2D{reference.x, reference.y, pose.angleDeg}, size.w, size.h}, matches.back().score});
    };
    if (indices.empty()) {
        for (Hlong i = 0; i < rows.Length(); ++i) take(i);
    } else {
        for (const Hlong i : indices) take(i);
    }
    const std::vector<std::size_t> kept = NonMaxSuppression(NmsOptions{nmsMaxOverlap, OverlapMeasure::IntersectionOverUnion}).keep(boxes);
    std::vector<Match> out;
    out.reserve(std::min(kept.size(), params.maxMatches));
    for (const std::size_t k : kept) {
        if (out.size() >= params.maxMatches) break;
        out.push_back(matches[k]);
    }
    return out;
}

}  // namespace aoi::infra
