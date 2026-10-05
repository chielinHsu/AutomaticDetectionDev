#include "presentation/shell/ImageViewAdapter.h"

#include <memory>

namespace aoi::ui {

aoi::ImageView toImageView(const QImage& source, double umPerPixel) {
    aoi::ImageView view;
    if (source.isNull()) return view;

    QImage image = source;
    aoi::PixelFormat format = aoi::PixelFormat::Bgrx32;
    switch (image.format()) {
        case QImage::Format_Grayscale8: format = aoi::PixelFormat::Gray8; break;
        case QImage::Format_RGB32:
        case QImage::Format_ARGB32:
        case QImage::Format_ARGB32_Premultiplied: format = aoi::PixelFormat::Bgrx32; break;
        case QImage::Format_RGB888: format = aoi::PixelFormat::Rgb24; break;
        default:
            image = image.convertToFormat(QImage::Format_RGB32);
            format = aoi::PixelFormat::Bgrx32;
            break;
    }

    auto keepAlive = std::make_shared<QImage>(image);  // 隱式共享：只加參照計數
    view.data = keepAlive->constBits();
    view.width = keepAlive->width();
    view.height = keepAlive->height();
    view.strideBytes = static_cast<int>(keepAlive->bytesPerLine());
    view.format = format;
    view.owner = keepAlive;
    view.umPerPixel = umPerPixel;
    return view;
}

}  // namespace aoi::ui
