#include "infrastructure/backend_halcon/HalconSearchSteps.h"

#include <algorithm>
#include <cmath>

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

}  // namespace

int halconWindowGrow(int numLevels) noexcept { return std::max(2, 1 << std::clamp(numLevels - 1, 0, 10)); }

HalconSearchInput prepareHalconSearch(const ImageView& image, const std::vector<Rect>& accepts, bool windows, double halfExtent,
                                      int grow, bool nativeGray16) {
    HalconSearchInput in;
    const bool zeroCopy = image.packed() && (image.format == PixelFormat::Gray8 || (nativeGray16 && image.format == PixelFormat::Gray16));
    PixelRect roi{0, 0, image.width, image.height};
    if (windows && !zeroCopy && !accepts.empty()) {
        // 只轉換用得到的那一塊：所有視窗的外接矩形，再加範本的一半（參考點在視窗邊上時，模型的像素在外面）。
        const double margin = halfExtent + grow + 2.0;
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

std::vector<Match> halconMatches(const HalconCpp::HTuple& rows, const HalconCpp::HTuple& columns, const HalconCpp::HTuple& angles,
                                 const HalconCpp::HTuple& scores, const std::vector<Hlong>& indices, const HalconGray& gray,
                                 const std::vector<Rect>& accepts, const SearchParams& params, Size2D size, double nmsMaxOverlap) {
    std::vector<Match> matches;
    const auto take = [&](Hlong i) {
        const double score = scores[i].D();
        if (!(score >= params.minScore)) return;
        Pose2D pose = poseFromHalcon(rows[i].D(), columns[i].D(), angles[i].D(), gray.originX, gray.originY);
        const Point2D center = pose.position();
        if (std::none_of(accepts.begin(), accepts.end(), [&](const Rect& a) { return a.contains(center); })) return;
        pose.angleDeg = angleIntoRange(pose.angleDeg, params);
        matches.push_back({pose, std::clamp(score, 0.0, 1.0)});
    };
    if (indices.empty()) {
        for (Hlong i = 0; i < rows.Length(); ++i) take(i);
    } else {
        for (const Hlong i : indices) take(i);
    }
    std::vector<ScoredBox> boxes;
    boxes.reserve(matches.size());
    for (const Match& m : matches) boxes.push_back({RectObj{m.pose, size.w, size.h}, m.score});
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
