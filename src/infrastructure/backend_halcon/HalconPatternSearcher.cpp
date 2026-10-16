#include "infrastructure/backend_halcon/HalconPatternSearcher.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <new>
#include <string>
#include <utility>

#include "infrastructure/backend_halcon/HalconCancelWatch.h"
#include "infrastructure/backend_halcon/HalconErrors.h"
#include "infrastructure/backend_halcon/HalconSearchSteps.h"

namespace aoi::infra {
namespace {

using Clock = std::chrono::steady_clock;

double millisecondsSince(Clock::time_point start) { return std::chrono::duration<double, std::milli>(Clock::now() - start).count(); }

Error cancelledError() { return makeError(ErrorCode::Cancelled, "The HALCON search was cancelled."); }

}  // namespace

Result<std::vector<Match>> HalconPatternSearcher::find(const ImageView& image, const ITemplate& tmpl, const SearchRegion& region,
                                                       const SearchParams& params, const CancelToken& cancel) {
    return find(image, tmpl, region, params, cancel, nullptr);
}

Result<std::vector<Match>> HalconPatternSearcher::find(const ImageView& image, const ITemplate& tmpl, const SearchRegion& region,
                                                       const SearchParams& params, const CancelToken& cancel,
                                                       HalconSearchStats* statsOut) const {
    const Clock::time_point started = Clock::now();
    HalconSearchStats stats;
    try {
        const auto* t = dynamic_cast<const HalconTemplate*>(&tmpl);
        if (t == nullptr || tmpl.backendId() != kHalconBackendId) {
            return makeError(ErrorCode::BackendMismatch, "The template was not built by the HALCON back end.");
        }
        if (!image.valid()) return makeError(ErrorCode::InvalidArgument, "The image is not valid.");
        if (!(params.minScore >= 0.0 && params.minScore <= 1.0)) {
            return makeError(ErrorCode::InvalidArgument, "minScore must be between 0 and 1.");
        }
        if (!std::isfinite(params.angleMinDeg) || !std::isfinite(params.angleMaxDeg) || params.angleMinDeg > params.angleMaxDeg) {
            return makeError(ErrorCode::InvalidArgument, "The angle range is not valid.");
        }
        // 模型只建了它的角度範圍：要搜的範圍超出它，HALCON 會默默裁掉。契約要明確的錯誤（和 OpenCV 版相同）。
        if (!t->fullCircle() && (params.angleMinDeg < t->angleMinDeg() - 1e-6 || params.angleMaxDeg > t->angleMaxDeg() + 1e-6)) {
            return makeError(ErrorCode::InvalidArgument,
                             "The template was built for angles " + std::to_string(t->angleMinDeg()) + " to " +
                                 std::to_string(t->angleMaxDeg()) + " degrees; the search asks for " +
                                 std::to_string(params.angleMinDeg) + " to " + std::to_string(params.angleMaxDeg) + ".");
        }
        if (!t->hasModel()) return std::vector<Match>{};
        if (cancel.cancelled()) return cancelledError();
        stats.numLevels = t->numLevels();

        // ---- 1. 區域：每個匹配的中心要落在 accept 裡 ----
        const Rect imageRect{0.0, 0.0, static_cast<double>(image.width), static_cast<double>(image.height)};
        const bool windows = region.kind == SearchRegion::Kind::Windows;
        std::vector<Rect> accepts;
        if (!windows) {
            accepts.push_back(imageRect);
        } else {
            for (const Rect& w : region.windows) {
                const Rect accept = intersection(w, imageRect);
                if (!accept.empty()) accepts.push_back(accept);
            }
        }
        stats.areas = accepts.size();
        if (accepts.empty()) {
            if (statsOut != nullptr) *statsOut = stats;
            return std::vector<Match>{};
        }

        // ---- 2. 影像與搜尋區域 ----
        Clock::time_point t0 = Clock::now();
        // 範本決定的平滑與次像素（HalconTemplate.h 的 HalconTemplateTuning）：搜尋影像和模型要是同一種影像。
        const HalconTemplateTuning& tuning = t->tuning();
        // 比對區（ADR-028）：find_shape_model 的搜尋區域限制的是**參考點**（domain 的重心；說明檔 find_shape_model 一節），
        // 主體中心的視窗要換成參考點的視窗（平移、放大）。整框時參考點就是主體中心，視窗不變。
        std::vector<Rect> searchWindows = accepts;
        if (windows && t->originDistance() > 0.0) {
            searchWindows.clear();
            for (const Rect& w : referenceWindows(accepts, t->originOffset(), params.angleMinDeg, params.angleMaxDeg)) {
                const Rect clipped = intersection(w, imageRect);
                if (!clipped.empty()) searchWindows.push_back(clipped);
            }
            if (searchWindows.empty()) {
                if (statsOut != nullptr) *statsOut = stats;
                return std::vector<Match>{};
            }
        }
        const HalconSearchInput input = prepareHalconSearch(image, searchWindows, windows, t->referenceReach(),
                                                            halconWindowGrow(t->numLevels()), options_.nativeGray16, tuning.smoothRadius, options_.meanPasses);
        if (!input.gray.image.IsInitialized()) return makeError(ErrorCode::Internal, "Cannot convert the image for HALCON.");
        stats.copied = input.gray.copied;
        stats.smoothRadius = tuning.smoothRadius;
        stats.edgeWidthPx = tuning.edgeWidthPx;
        stats.convertMs = millisecondsSince(t0);
        if (cancel.cancelled()) return cancelledError();

        // ---- 3. find_shape_model（NumMatches ＝ 0：全部；NumLevels ＝ 0：模型的層數，邊很寬的範本只追蹤到 tuning.lowestLevel）----
        t0 = Clock::now();
        HalconCpp::HTuple rows, columns, angles, scores;
        {
            HalconCancelWatch watch(cancel);
            t->model().FindShapeModel(input.searchImage, degreesToRadians(params.angleMinDeg),
                                      degreesToRadians(params.angleMaxDeg - params.angleMinDeg), HalconCpp::HTuple(params.minScore), 0,
                                      options_.maxOverlap, HalconCpp::HTuple((tuning.subPixel.empty() ? options_.subPixel : tuning.subPixel).c_str()),
                                      tuning.lowestLevel > 1 ? HalconCpp::HTuple(static_cast<Hlong>(t->numLevels())).TupleConcat(HalconCpp::HTuple(static_cast<Hlong>(std::min(tuning.lowestLevel, t->numLevels())))) : HalconCpp::HTuple(0),
                                      options_.greediness, &rows, &columns, &angles, &scores);
        }
        stats.findMs = millisecondsSince(t0);
        if (cancel.cancelled()) return cancelledError();

        // ---- 4、5. 換算、過濾、NMS、排序 ----
        t0 = Clock::now();
        stats.rawMatches = static_cast<std::size_t>(rows.Length());
        std::vector<Match> out = halconMatches(rows, columns, angles, scores, {}, input.gray, accepts, params, t->matchSize(), options_.nmsMaxOverlap,
                                               t->originOffset());
        stats.nmsMs = millisecondsSince(t0);
        stats.matches = out.size();
        stats.totalMs = millisecondsSince(started);
        if (statsOut != nullptr) *statsOut = stats;
        return out;
    } catch (const HalconCpp::HException& e) {
        return fromHalcon(e, "searching the image");
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::BackendFailed, "Not enough memory to search the image.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while searching the image.");
    }
}

}  // namespace aoi::infra
