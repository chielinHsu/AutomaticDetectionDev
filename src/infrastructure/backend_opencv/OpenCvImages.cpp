#include "infrastructure/backend_opencv/OpenCvImages.h"

#include <opencv2/imgproc.hpp>

#include "domain/image_copy.h"
#include "domain/image_depth.h"

namespace aoi::infra {
namespace {

bool formatOf(int type, PixelFormat* out) {
    switch (type) {
        case CV_8UC1: *out = PixelFormat::Gray8; return true;
        case CV_16UC1: *out = PixelFormat::Gray16; return true;
        case CV_8UC3: *out = PixelFormat::Bgr24; return true;
        case CV_8UC4: *out = PixelFormat::Bgrx32; return true;
        default: return false;
    }
}

int typeOf(PixelFormat f) {
    switch (f) {
        case PixelFormat::Gray8: return CV_8UC1;
        case PixelFormat::Gray16: return CV_16UC1;
        case PixelFormat::Rgb24:
        case PixelFormat::Bgr24: return CV_8UC3;
        case PixelFormat::Bgrx32: return CV_8UC4;
    }
    return -1;
}

}  // namespace

ImageView fromMat(const cv::Mat& mat, double umPerPixel) {
    ImageView view;
    PixelFormat format = PixelFormat::Gray8;
    if (mat.empty() || mat.dims != 2 || !formatOf(mat.type(), &format)) return view;
    if (mat.step[0] > static_cast<std::size_t>(0x7fffffff)) return view;
    // 包著外部記憶體的 Mat（例如 borrowAsMat 的結果）不擁有像素：複製這個 Mat 也留不住像素，做成 view 會懸空（違反規則 3）。
    if (mat.u == nullptr) return view;

    auto keepAlive = std::make_shared<const cv::Mat>(mat);  // 參照計數，不複製像素
    view.data = keepAlive->data;
    view.width = keepAlive->cols;
    view.height = keepAlive->rows;
    view.strideBytes = static_cast<int>(keepAlive->step[0]);
    view.format = format;
    view.owner = keepAlive;
    view.umPerPixel = umPerPixel;
    return view;
}

MatView borrowAsMat(const ImageView& view) {
    if (!view.valid()) return {};
    // cv::Mat 的建構子要 void*：只借來讀，不寫（見標頭）。
    cv::Mat mat(view.height, view.width, typeOf(view.format), const_cast<std::uint8_t*>(view.data),
                static_cast<std::size_t>(view.strideBytes));
    return {mat, view.owner};
}

GrayRoi toGray8(const ImageView& view, const cv::Rect& roi) {
    if (!view.valid() || roi.width <= 0 || roi.height <= 0) return {};
    if (roi.x < 0 || roi.y < 0 || roi.x + roi.width > view.width || roi.y + roi.height > view.height) return {};
    const MatView whole = borrowAsMat(view);
    const cv::Mat part = whole.mat(roi);
    GrayRoi out;
    switch (view.format) {
        case PixelFormat::Gray8:
            out.mat = part;
            out.owner = whole.owner;
            return out;
        case PixelFormat::Gray16:
            part.convertTo(out.mat, CV_8U, 255.0 / normalizedMax(effectiveBits(view)));
            break;
        case PixelFormat::Bgrx32: cv::cvtColor(part, out.mat, cv::COLOR_BGRA2GRAY); break;
        case PixelFormat::Bgr24: cv::cvtColor(part, out.mat, cv::COLOR_BGR2GRAY); break;
        case PixelFormat::Rgb24: cv::cvtColor(part, out.mat, cv::COLOR_RGB2GRAY); break;
    }
    out.copied = true;
    countImageCopy(static_cast<std::size_t>(out.mat.total()));
    return out;
}

}  // namespace aoi::infra
