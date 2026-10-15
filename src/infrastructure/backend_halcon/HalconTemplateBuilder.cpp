#include "infrastructure/backend_halcon/HalconTemplateBuilder.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#include "infrastructure/backend_halcon/HalconErrors.h"
#include "infrastructure/backend_halcon/HalconImages.h"
#include "infrastructure/backend_halcon/HalconPose.h"
#include "infrastructure/backend_halcon/HalconSearchSteps.h"

namespace aoi::infra {
namespace {

Error cancelled() { return makeError(ErrorCode::Cancelled, "The HALCON template build was cancelled."); }

// 樣本外多轉換幾個像素：邊緣（梯度）在樣本的邊上也要用到外面一圈的像素，整張影像與一小塊才會給出同一個模型。
// 平滑（mean_image 半徑最大 2、3 次 ＝ 6 個像素）時再多濾波器的範圍，邊界碰不到樣本。
constexpr int kContextPx = 8 + 6;

HalconCpp::HTuple autoOr(int value) { return value > 0 ? HalconCpp::HTuple(static_cast<Hlong>(value)) : HalconCpp::HTuple("auto"); }

}  // namespace

Result<TemplatePtr> HalconTemplateBuilder::build(const ImageView& image, const Rect& sample, const SearchParams& params,
                                                 const CancelToken& cancel) {
    return buildFromSample(image, SampleSpec{sample, {}}, params, cancel);
}

Result<TemplatePtr> HalconTemplateBuilder::buildFromSample(const ImageView& image, const SampleSpec& spec, const SearchParams& params,
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

        // 整數像素的樣本（四捨五入到最近的像素邊界，再裁到影像內；和 OpenCV 版相同）。
        const int x0 = std::clamp(static_cast<int>(std::lround(clamped.x)), 0, image.width - 1);
        const int y0 = std::clamp(static_cast<int>(std::lround(clamped.y)), 0, image.height - 1);
        const int x1 = std::clamp(static_cast<int>(std::lround(clamped.right())), x0 + 1, image.width);
        const int y1 = std::clamp(static_cast<int>(std::lround(clamped.bottom())), y0 + 1, image.height);
        const int width = x1 - x0;
        const int height = y1 - y0;
        const Size2D size{static_cast<double>(width), static_cast<double>(height)};

        // 比對區（ADR-026）：沒有設定 ＝ 主體框。矩形用和主體框一樣的取整；遮罩已經在整數像素上，裁到影像內，
        // 存成每列的 runs（影像座標，[begin, end)）。
        int mx0 = x0;
        int my0 = y0;
        int mw = width;
        int mh = height;
        struct Run {
            int row;
            int begin;
            int end;
        };
        std::vector<Run> runs;
        const bool regionGiven = match.rect.has_value();
        if (regionGiven) {
            const Rect& r = *match.rect;
            if (!std::isfinite(r.x) || !std::isfinite(r.y) || !std::isfinite(r.w) || !std::isfinite(r.h)) {
                return makeError(ErrorCode::InvalidArgument, "The match region is not finite.");
            }
            if (!match.mask.empty()) {
                const int bx0 = std::max(0, match.mask.x);
                const int by0 = std::max(0, match.mask.y);
                const int bx1 = std::min(image.width, match.mask.x + match.mask.width);
                const int by1 = std::min(image.height, match.mask.y + match.mask.height);
                int minX = bx1;
                int minY = by1;
                int maxX = -1;
                int maxY = -1;
                for (int y = by0; y < by1; ++y) {
                    int x = bx0;
                    while (x < bx1) {
                        while (x < bx1 && !match.mask.at(x, y)) ++x;
                        if (x >= bx1) break;
                        const int begin = x;
                        while (x < bx1 && match.mask.at(x, y)) ++x;
                        runs.push_back({y, begin, x});
                        minX = std::min(minX, begin);
                        maxX = std::max(maxX, x - 1);
                        minY = std::min(minY, y);
                        maxY = std::max(maxY, y);
                    }
                }
                if (runs.empty()) return makeError(ErrorCode::InvalidArgument, "The match mask is outside the image or empty.");
                mx0 = minX;
                my0 = minY;
                mw = maxX - minX + 1;
                mh = maxY - minY + 1;
            } else {
                const Rect c = intersection(r, imageRect);
                if (c.empty()) return makeError(ErrorCode::InvalidArgument, "The match region is outside the image.");
                mx0 = std::clamp(static_cast<int>(std::lround(c.x)), 0, image.width - 1);
                my0 = std::clamp(static_cast<int>(std::lround(c.y)), 0, image.height - 1);
                mw = std::clamp(static_cast<int>(std::lround(c.right())), mx0 + 1, image.width) - mx0;
                mh = std::clamp(static_cast<int>(std::lround(c.bottom())), my0 + 1, image.height) - my0;
            }
        }
        if (cancel.cancelled()) return cancelled();

        const bool zeroCopy = image.packed() && (image.format == PixelFormat::Gray8 ||
                                                 (options_.nativeGray16 && image.format == PixelFormat::Gray16));
        const PixelRect context{std::max(0, mx0 - kContextPx), std::max(0, my0 - kContextPx),
                                std::min(image.width, mx0 + mw + kContextPx) - std::max(0, mx0 - kContextPx),
                                std::min(image.height, my0 + mh + kContextPx) - std::max(0, my0 - kContextPx)};
        const HalconGray gray = toHalconGray(image, zeroCopy ? PixelRect{0, 0, image.width, image.height} : context, options_.nativeGray16);
        if (!gray.image.IsInitialized()) return makeError(ErrorCode::Internal, "Cannot convert the sample for HALCON.");

        // 樣本的邊有多寬、雜訊多大 → 要不要先平滑、次像素用哪一種、追蹤到哪一層（HalconSearchOptions.h 的「預先平滑」；搜尋影像跟著範本做）。
        // 比對區時量比對區（模型只用它的邊）。
        HalconTemplateTuning tuning;
        const HalconSampleStatistics stats = estimateSampleStatistics(gray.image, halconRect1(mx0 - gray.originX, my0 - gray.originY, mw, mh));
        tuning.edgeWidthPx = stats.edgeWidthPx;
        tuning.noiseRatio = stats.noiseRatio;
        const HalconSmoothing smoothing = chooseHalconSmoothing(stats, options_);
        tuning.smoothRadius = smoothing.smoothRadius;
        tuning.subPixel = smoothing.subPixel;
        tuning.lowestLevel = smoothing.lowestLevel;

        // 建模型用的影像：平滑時只平滑樣本外加一圈的那一塊（零複製的整張先裁下來；HALCON 自己配置新影像）。
        HalconCpp::HImage modelImage = gray.image;
        int originX = gray.originX;
        int originY = gray.originY;
        if (tuning.smoothRadius > 0) {
            if (zeroCopy) {
                modelImage = gray.image.CropRectangle1(static_cast<Hlong>(context.y), static_cast<Hlong>(context.x),
                                                       static_cast<Hlong>(context.y + context.height - 1),
                                                       static_cast<Hlong>(context.x + context.width - 1));
                originX = context.x;
                originY = context.y;
            }
            modelImage = smoothHalconImage(modelImage, tuning.smoothRadius, options_.meanPasses);
        }
        HalconCpp::HRegion domain;
        if (runs.empty()) {
            const HalconRect1 r = halconRect1(mx0 - originX, my0 - originY, mw, mh);
            domain.GenRectangle1(r.row1, r.column1, r.row2, r.column2);
        } else {
            // 遮罩：每一段一個一列高的矩形（gen_rectangle1 的兩端都包含，和 halconRect1 相同），再聯集。
            HalconCpp::HTuple r1, c1, r2, c2;
            for (const Run& run : runs) {
                r1.Append(static_cast<double>(run.row - originY));
                c1.Append(static_cast<double>(run.begin - originX));
                r2.Append(static_cast<double>(run.row - originY));
                c2.Append(static_cast<double>(run.end - 1 - originX));
            }
            HalconCpp::HRegion pieces;
            pieces.GenRectangle1(r1, c1, r2, c2);
            domain = pieces.Union1();
        }
        const HalconCpp::HImage reduced = modelImage.ReduceDomain(domain);

        // 參考點（ADR-026）：模型座標的 (0, 0) 是 domain 的重心（HALCON 的預設）。整框時就是主體中心，不必算（和之前逐位元相同）。
        HalconMatchGeometry geometry;
        if (regionGiven) {
            double row = 0.0;
            double column = 0.0;
            domain.AreaCenter(&row, &column);
            const Point2D reference{column + 0.5 + originX, row + 0.5 + originY};  // HALCON 的像素中心在整數 → ADR-010
            geometry.matchSize = Size2D{static_cast<double>(mw), static_cast<double>(mh)};
            geometry.originOffset = Point2D{x0 + width * 0.5 - reference.x, y0 + height * 0.5 - reference.y};
            for (const double cx : {static_cast<double>(mx0), static_cast<double>(mx0 + mw)}) {
                for (const double cy : {static_cast<double>(my0), static_cast<double>(my0 + mh)}) {
                    geometry.referenceReach = std::max(geometry.referenceReach, std::hypot(cx - reference.x, cy - reference.y));
                }
            }
        }

        // 角度步進與模型的角度範圍（HalconSearchOptions.h 的說明與 S08 的實測）。
        double stepDeg = options_.angleStepDeg;
        if (!(stepDeg > 0.0) && options_.cornerShiftPx > 0.0) {
            stepDeg = radiansToDegrees(std::atan2(options_.cornerShiftPx, 0.5 * std::hypot(mw, mh)));
        }
        const bool fullCircle = params.angleMaxDeg - params.angleMinDeg >= 360.0 - 1e-9;
        const double margin = fullCircle || !(stepDeg > 0.0) ? 0.0 : options_.angleMarginSteps * stepDeg;
        const double modelMin = params.angleMinDeg - margin;
        const double modelExtent = std::min(360.0, params.angleMaxDeg - params.angleMinDeg + 2.0 * margin);
        const double angleStart = degreesToRadians(modelMin);
        const double angleExtent = degreesToRadians(modelExtent);
        const HalconCpp::HTuple angleStep = stepDeg > 0.0 ? HalconCpp::HTuple(degreesToRadians(stepDeg)) : HalconCpp::HTuple("auto");
        HalconCpp::HShapeModel model;
        try {
            // create_shape_model 不能中斷（get_operator_info 的 interrupt_mode 是 'none'），取消只在前後檢查。
            model.CreateShapeModel(reduced, autoOr(options_.numLevels), angleStart, angleExtent, angleStep,
                                   HalconCpp::HTuple(options_.optimization.c_str()), options_.metric.c_str(),
                                   autoOr(options_.contrast), autoOr(options_.minContrast));
        } catch (const HalconCpp::HException& e) {
            if (static_cast<long>(e.ErrorCode()) != kHalconTooFewModelPoints) throw;
            // 邊緣點太少：樣本是平的（或對比太低）。和 OpenCV 版一樣，回傳帶問題的範本，搜尋會回空的結果。
            return TemplatePtr(std::make_shared<const HalconTemplate>(size, params.angleMinDeg, params.angleMaxDeg,
                                                                      HalconCpp::HShapeModel(), 0, NoMatchReason::LowContrast, tuning, geometry));
        }
        if (cancel.cancelled()) return cancelled();

        double ignoredStart = 0.0;
        double ignoredExtent = 0.0;
        double ignoredStep = 0.0;
        HalconCpp::HTuple scaleMin;
        HalconCpp::HTuple scaleMax;
        HalconCpp::HTuple scaleStep;
        HalconCpp::HString metric;
        Hlong minContrast = 0;
        const HalconCpp::HTuple levels =
            model.GetShapeModelParams(&ignoredStart, &ignoredExtent, &ignoredStep, &scaleMin, &scaleMax, &scaleStep, &metric, &minContrast);
        const int numLevels = levels.Length() > 0 ? static_cast<int>(levels[0].L()) : 0;
        return TemplatePtr(std::make_shared<const HalconTemplate>(size, params.angleMinDeg, params.angleMaxDeg, std::move(model), numLevels,
                                                                  std::nullopt, tuning, geometry));
    } catch (const HalconCpp::HException& e) {
        return fromHalcon(e, "building the template");
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::BackendFailed, "Not enough memory to build the template.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while building the template.");
    }
}

}  // namespace aoi::infra
