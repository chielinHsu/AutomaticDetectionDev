#include "infrastructure/backend_halcon/HalconTemplateBuilder.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <utility>

#include "infrastructure/backend_halcon/HalconErrors.h"
#include "infrastructure/backend_halcon/HalconImages.h"
#include "infrastructure/backend_halcon/HalconPose.h"

namespace aoi::infra {
namespace {

Error cancelled() { return makeError(ErrorCode::Cancelled, "The HALCON template build was cancelled."); }

// 樣本外多轉換幾個像素：邊緣（梯度）在樣本的邊上也要用到外面一圈的像素，整張影像與一小塊才會給出同一個模型。
constexpr int kContextPx = 8;

HalconCpp::HTuple autoOr(int value) { return value > 0 ? HalconCpp::HTuple(static_cast<Hlong>(value)) : HalconCpp::HTuple("auto"); }

}  // namespace

Result<TemplatePtr> HalconTemplateBuilder::build(const ImageView& image, const Rect& sample, const SearchParams& params,
                                                 const CancelToken& cancel) {
    try {
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
        if (cancel.cancelled()) return cancelled();

        const bool zeroCopy = image.packed() && (image.format == PixelFormat::Gray8 ||
                                                 (options_.nativeGray16 && image.format == PixelFormat::Gray16));
        PixelRect roi{0, 0, image.width, image.height};
        if (!zeroCopy) {
            const int rx0 = std::max(0, x0 - kContextPx);
            const int ry0 = std::max(0, y0 - kContextPx);
            const int rx1 = std::min(image.width, x1 + kContextPx);
            const int ry1 = std::min(image.height, y1 + kContextPx);
            roi = {rx0, ry0, rx1 - rx0, ry1 - ry0};
        }
        const HalconGray gray = toHalconGray(image, roi, options_.nativeGray16);
        if (!gray.image.IsInitialized()) return makeError(ErrorCode::Internal, "Cannot convert the sample for HALCON.");

        const HalconRect1 r = halconRect1(x0 - gray.originX, y0 - gray.originY, width, height);
        HalconCpp::HRegion domain;
        domain.GenRectangle1(r.row1, r.column1, r.row2, r.column2);
        const HalconCpp::HImage reduced = gray.image.ReduceDomain(domain);

        // 角度步進與模型的角度範圍（HalconSearchOptions.h 的說明與 S08 的實測）。
        double stepDeg = options_.angleStepDeg;
        if (!(stepDeg > 0.0) && options_.cornerShiftPx > 0.0) {
            stepDeg = radiansToDegrees(std::atan2(options_.cornerShiftPx, 0.5 * std::hypot(width, height)));
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
                                                                      HalconCpp::HShapeModel(), 0, NoMatchReason::LowContrast));
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
        return TemplatePtr(std::make_shared<const HalconTemplate>(size, params.angleMinDeg, params.angleMaxDeg, std::move(model), numLevels));
    } catch (const HalconCpp::HException& e) {
        return fromHalcon(e, "building the template");
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::BackendFailed, "Not enough memory to build the template.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while building the template.");
    }
}

}  // namespace aoi::infra
