#include "infrastructure/backend_opencv/OpenCvLibraryMatcher.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <opencv2/imgproc.hpp>
#include <string>

#include "domain/lattice/non_max_suppression.h"
#include "infrastructure/backend_opencv/OpenCvImages.h"

namespace aoi::infra {
namespace {

// 參考影像換成板子影像的比例（8 bit 灰階，縮放過的是新配置的 Mat，view 擁有它）。比例差不到 1% 時照原樣用。
ImageView atBoardScale(const ImageView& reference, double boardUmPerPixel) {
    if (!(reference.umPerPixel > 0.0) || !(boardUmPerPixel > 0.0)) return reference;
    const double scale = reference.umPerPixel / boardUmPerPixel;
    if (std::abs(scale - 1.0) <= 0.01) return reference;
    const GrayRoi gray = toGray8(reference, cv::Rect(0, 0, reference.width, reference.height));
    if (gray.mat.empty()) return {};
    cv::Mat resized;
    cv::resize(gray.mat, resized, cv::Size(), scale, scale, scale < 1.0 ? cv::INTER_AREA : cv::INTER_LINEAR);
    if (resized.empty()) return {};
    return fromMat(resized, boardUmPerPixel);
}

struct Hit {
    Pose2D pose;
    double score = 0.0;
    std::size_t type = 0;
    Size2D size;
};

}  // namespace

Result<LibraryMatchResult> OpenCvLibraryMatcher::match(const ImageView& image, const Rect& region,
                                                       const std::vector<ComponentType>& candidates, const SearchParams& params,
                                                       const CancelToken& cancel, IProgressSink* progress) {
    try {
        if (!image.valid()) return makeError(ErrorCode::InvalidArgument, "The image is not valid.");
        if (!std::isfinite(region.x) || !std::isfinite(region.y) || !std::isfinite(region.w) || !std::isfinite(region.h)) {
            return makeError(ErrorCode::InvalidArgument, "The region is not finite.");
        }
        const Rect imageRect{0.0, 0.0, static_cast<double>(image.width), static_cast<double>(image.height)};
        const Rect area = intersection(region, imageRect);
        if (area.empty()) return makeError(ErrorCode::InvalidArgument, "The region is outside the image.");

        LibraryMatchResult result;
        std::vector<Hit> hits;
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            if (cancel.cancelled()) return makeError(ErrorCode::Cancelled, "The library match was cancelled.");
            const ImageView reference = atBoardScale(candidates[i].referenceImage, image.umPerPixel);
            const bool fits = reference.valid() && reference.width <= area.w && reference.height <= area.h;
            if (fits) {
                Result<TemplatePtr> built = builder_.build(reference, Rect{0.0, 0.0, static_cast<double>(reference.width),
                                                                          static_cast<double>(reference.height)},
                                                           params, cancel);
                if (!built) return built.error();
                ++result.samplesCompared;
                const TemplatePtr& tmpl = built.value();
                if (!tmpl->sampleProblem()) {
                    Result<std::vector<Match>> found = searcher_.find(image, *tmpl, SearchRegion::inWindows({area}), params, cancel);
                    if (!found) return found.error();
                    for (const Match& m : found.value()) hits.push_back({m.pose, m.score, i, tmpl->size()});
                }
            }
            if (progress != nullptr) progress->report({Stage::Match, static_cast<double>(i + 1) / static_cast<double>(candidates.size())});
        }

        // 分數由高到低（同分依 y、x、類型的順序），依序合併成元件。
        std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
            if (a.score != b.score) return a.score > b.score;
            if (a.pose.y != b.pose.y) return a.pose.y < b.pose.y;
            if (a.pose.x != b.pose.x) return a.pose.x < b.pose.x;
            return a.type < b.type;
        });
        std::vector<RectObj> boxes;
        for (const Hit& h : hits) {
            const RectObj box{h.pose, h.size.w, h.size.h};
            std::size_t owner = boxes.size();
            for (std::size_t j = 0; j < boxes.size(); ++j) {
                if (overlapRatio(box, boxes[j], OverlapMeasure::IntersectionOverSmaller) > 0.5) {
                    owner = j;
                    break;
                }
            }
            const std::string& typeId = candidates[h.type].id;
            if (owner == boxes.size()) {
                boxes.push_back(box);
                result.components.push_back({h.pose, {{typeId, h.score}}});
                continue;
            }
            std::vector<TypeScore>& scores = result.components[owner].typeScores;
            if (std::none_of(scores.begin(), scores.end(), [&](const TypeScore& s) { return s.typeId == typeId; })) {
                scores.push_back({typeId, h.score});  // 依分數順序加入，所以已經由高到低
            }
        }
        if (result.samplesCompared == 0) result.noMatch = NoMatchInfo{NoMatchReason::LibraryEmpty, 0.0, 0.0, 0};
        else if (result.components.empty()) result.noMatch = NoMatchInfo{NoMatchReason::NoLibraryMatch, 0.0, 0.0, 0};
        return result;
    } catch (const cv::Exception& e) {
        return makeError(ErrorCode::BackendFailed, std::string("OpenCV failed while matching the library: ") + e.what());
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::BackendFailed, "Not enough memory to match the library.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while matching the library.");
    }
}

}  // namespace aoi::infra
