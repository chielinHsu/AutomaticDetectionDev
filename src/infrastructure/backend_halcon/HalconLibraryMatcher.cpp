#include "infrastructure/backend_halcon/HalconLibraryMatcher.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <string>

#include "domain/lattice/non_max_suppression.h"
#include "infrastructure/backend_halcon/HalconCancelWatch.h"
#include "infrastructure/backend_halcon/HalconErrors.h"
#include "infrastructure/backend_halcon/HalconImages.h"
#include "infrastructure/backend_halcon/HalconSearchSteps.h"

namespace aoi::infra {
namespace {

Error cancelledError() { return makeError(ErrorCode::Cancelled, "The library match was cancelled."); }

// 參考影像換成板子影像的比例（8 bit 灰階；縮放過的是 HALCON 新配置的影像，view 擁有它）。比例差不到 1% 時照原樣用。
ImageView atBoardScale(const ImageView& reference, double boardUmPerPixel) {
    if (!(reference.umPerPixel > 0.0) || !(boardUmPerPixel > 0.0)) return reference;
    const double scale = reference.umPerPixel / boardUmPerPixel;
    if (std::abs(scale - 1.0) <= 0.01) return reference;
    const HalconGray gray = toHalconGray(reference, PixelRect{0, 0, reference.width, reference.height}, false);
    if (!gray.image.IsInitialized()) return {};
    // 'constant'：縮小時取平均（和 OpenCV 版的 INTER_AREA 相近），放大時是雙線性。
    return viewOfHalconImage(gray.image.ZoomImageFactor(scale, scale, "constant"), boardUmPerPixel);
}

struct Hit {
    Pose2D pose;
    double score = 0.0;
    std::size_t type = 0;
    Size2D size;
};

// 一個建好的候選：candidates 的第 index 個。
struct Built {
    std::size_t index = 0;
    std::shared_ptr<const HalconTemplate> tmpl;
};

}  // namespace

Result<LibraryMatchResult> HalconLibraryMatcher::match(const ImageView& image, const Rect& region,
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

        const auto report = [&](std::size_t done) {
            if (progress != nullptr && !candidates.empty()) {
                progress->report({Stage::Match, static_cast<double>(done) / static_cast<double>(candidates.size())});
            }
        };

        LibraryMatchResult result;
        std::vector<Hit> hits;
        std::vector<Built> built;
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            if (cancel.cancelled()) return cancelledError();
            const ImageView reference = atBoardScale(candidates[i].referenceImage, image.umPerPixel);
            const bool fits = reference.valid() && reference.width <= area.w && reference.height <= area.h;
            if (fits) {
                Result<TemplatePtr> b = builder_.build(reference, Rect{0.0, 0.0, static_cast<double>(reference.width),
                                                                       static_cast<double>(reference.height)},
                                                       params, cancel);
                if (!b) return b.error();
                ++result.samplesCompared;
                auto tmpl = std::dynamic_pointer_cast<const HalconTemplate>(b.value());
                if (tmpl && tmpl->hasModel()) {
                    if (options_.libraryOneCall) {
                        built.push_back({i, tmpl});
                    } else {
                        Result<std::vector<Match>> found = searcher_.find(image, *tmpl, SearchRegion::inWindows({area}), params, cancel);
                        if (!found) return found.error();
                        for (const Match& m : found.value()) hits.push_back({m.pose, m.score, i, tmpl->size()});
                    }
                }
            }
            // 一次搜尋的模式：最後一個類型的回報等搜尋完成（fraction ＝ 1 表示真的做完了）。
            if (!(options_.libraryOneCall && i + 1 == candidates.size())) report(i + 1);
        }

        if (options_.libraryOneCall && !built.empty()) {
            // 所有模型一次 find_shape_models。MaxOverlap、NumMatches 每個模型一個值：只有同一個模型的結果互相抑制。
            std::vector<HalconCpp::HShapeModel> models;
            double halfExtent = 0.0;
            int levels = 0;
            for (const Built& b : built) {
                models.push_back(b.tmpl->model());
                halfExtent = std::max(halfExtent, 0.5 * std::hypot(b.tmpl->size().w, b.tmpl->size().h));
                levels = std::max(levels, b.tmpl->numLevels());
            }
            const std::vector<Rect> accepts{area};
            const HalconSearchInput input = prepareHalconSearch(image, accepts, true, halfExtent, halconWindowGrow(levels), options_.nativeGray16);
            if (!input.gray.image.IsInitialized()) return makeError(ErrorCode::Internal, "Cannot convert the image for HALCON.");
            const Hlong n = static_cast<Hlong>(models.size());
            HalconCpp::HTuple maxOverlap;
            HalconCpp::HTuple numMatches;
            for (Hlong k = 0; k < n; ++k) {
                maxOverlap.Append(options_.maxOverlap);
                numMatches.Append(Hlong{0});
            }
            HalconCpp::HTuple rows, columns, angles, scores, which;
            if (cancel.cancelled()) return cancelledError();
            {
                HalconCancelWatch watch(cancel);
                HalconCpp::HShapeModel::FindShapeModels(input.searchImage, HalconCpp::HShapeModelArray(models.data(), n),
                                                        degreesToRadians(params.angleMinDeg),
                                                        degreesToRadians(params.angleMaxDeg - params.angleMinDeg),
                                                        HalconCpp::HTuple(params.minScore), numMatches, maxOverlap,
                                                        HalconCpp::HTuple(options_.subPixel.c_str()), HalconCpp::HTuple(0),
                                                        HalconCpp::HTuple(options_.greediness), &rows, &columns, &angles, &scores, &which);
            }
            if (cancel.cancelled()) return cancelledError();
            for (std::size_t m = 0; m < built.size(); ++m) {
                std::vector<Hlong> indices;
                for (Hlong k = 0; k < which.Length(); ++k) {
                    if (which[k].L() == static_cast<Hlong>(m)) indices.push_back(k);
                }
                if (indices.empty()) continue;
                const Size2D size = built[m].tmpl->size();
                for (const Match& match : halconMatches(rows, columns, angles, scores, indices, input.gray, accepts, params, size,
                                                        options_.nmsMaxOverlap)) {
                    hits.push_back({match.pose, match.score, built[m].index, size});
                }
            }
        }
        if (options_.libraryOneCall && !candidates.empty()) report(candidates.size());

        // 分數由高到低（同分依 y、x、類型的順序），依序合併成元件（和 OpenCV 版相同的規則）。
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
    } catch (const HalconCpp::HException& e) {
        return fromHalcon(e, "matching the library");
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::BackendFailed, "Not enough memory to match the library.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while matching the library.");
    }
}

}  // namespace aoi::infra
