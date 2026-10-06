#include "infrastructure/image_qt/QtImages.h"

#include <memory>
#include <utility>

#include "domain/image_copy.h"

namespace aoi::infra {
namespace {

// 零複製對得上的格式。
bool wrappable(QImage::Format f, PixelFormat* out) {
    switch (f) {
        case QImage::Format_Grayscale8: *out = PixelFormat::Gray8; return true;
        case QImage::Format_Grayscale16: *out = PixelFormat::Gray16; return true;
        case QImage::Format_RGB32:
        case QImage::Format_ARGB32: *out = PixelFormat::Bgrx32; return true;
        case QImage::Format_RGB888: *out = PixelFormat::Rgb24; return true;
        case QImage::Format_BGR888: *out = PixelFormat::Bgr24; return true;
        default: return false;
    }
}

bool indexedOrMono(QImage::Format f) {
    return f == QImage::Format_Indexed8 || f == QImage::Format_Mono || f == QImage::Format_MonoLSB;
}

// 轉成 target，記一次複製。記憶體不足時回傳空影像。
QImage convertCounted(const QImage& image, QImage::Format target) {
    QImage out = image.convertToFormat(target);
    if (!out.isNull()) {
        countImageCopy(static_cast<std::size_t>(out.width()) * static_cast<std::size_t>(out.height()) *
                       static_cast<std::size_t>(out.depth() / 8));
    }
    return out;
}

QImage::Format targetFor(const QImage& image) {
    return indexedOrMono(image.format()) && image.isGrayscale() ? QImage::Format_Grayscale8 : QImage::Format_RGB32;
}

QImage::Format toQtFormat(PixelFormat f) {
    switch (f) {
        case PixelFormat::Gray8: return QImage::Format_Grayscale8;
        case PixelFormat::Rgb24: return QImage::Format_RGB888;
        case PixelFormat::Bgrx32: return QImage::Format_RGB32;
        case PixelFormat::Bgr24: return QImage::Format_BGR888;
        case PixelFormat::Gray16: return QImage::Format_Grayscale16;
    }
    return QImage::Format_Invalid;
}

void releaseOwner(void* info) { delete static_cast<std::shared_ptr<const void>*>(info); }

}  // namespace

ImageView fromQImage(const QImage& source, double umPerPixel) {
    ImageView view;
    if (source.isNull()) return view;

    QImage image = source;  // 隱式共享：只加參照計數
    PixelFormat format = PixelFormat::Gray8;
    if (!wrappable(image.format(), &format)) {
        image = convertCounted(image, targetFor(image));
        if (image.isNull()) return view;
        format = image.format() == QImage::Format_Grayscale8 ? PixelFormat::Gray8 : PixelFormat::Bgrx32;
    }

    // const 的 QImage：constBits() 不會分離，指到的就是共享的那一塊像素。
    auto keepAlive = std::make_shared<const QImage>(std::move(image));
    view.data = keepAlive->constBits();
    view.width = keepAlive->width();
    view.height = keepAlive->height();
    view.strideBytes = static_cast<int>(keepAlive->bytesPerLine());
    view.format = format;
    view.owner = keepAlive;
    view.umPerPixel = umPerPixel;
    return view;
}

QImage wrapAsQImage(const ImageView& view) {
    if (!view.valid()) return {};
    auto* holder = new std::shared_ptr<const void>(view.owner);
    QImage image(view.data, view.width, view.height, static_cast<qsizetype>(view.strideBytes), toQtFormat(view.format),
                 &releaseOwner, holder);
    if (image.isNull()) delete holder;  // Qt 沒有接手清理函式
    return image;
}

QImage toCanonicalQImage(QImage image) {
    switch (image.format()) {
        case QImage::Format_Invalid:
        case QImage::Format_Grayscale8:
        case QImage::Format_Grayscale16:
        case QImage::Format_RGB32:
        case QImage::Format_ARGB32: return image;
        default: return convertCounted(image, targetFor(image));
    }
}

}  // namespace aoi::infra
