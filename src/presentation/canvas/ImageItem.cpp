#include "presentation/canvas/ImageItem.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>

#include "domain/image_copy.h"
#include "domain/image_depth.h"

namespace aoi::ui {
namespace {

QImage::Format toQtFormat(aoi::PixelFormat f) {
    switch (f) {
        case aoi::PixelFormat::Gray8: return QImage::Format_Grayscale8;
        case aoi::PixelFormat::Rgb24: return QImage::Format_RGB888;
        case aoi::PixelFormat::Bgrx32: return QImage::Format_RGB32;
        case aoi::PixelFormat::Bgr24: return QImage::Format_BGR888;
        case aoi::PixelFormat::Gray16: return QImage::Format_Grayscale16;
    }
    return QImage::Format_Invalid;
}

void releaseOwner(void* info) { delete static_cast<std::shared_ptr<const void>*>(info); }

// 有效位元數不到 16 的 Gray16（例如 12 bit 的相機影像，值只到 4095）：照原樣畫會幾乎全黑，
// 所以顯示前轉成 8 bit（右移 位元數 − 8），算一次複製。完整 16 bit 的照樣零複製。
QImage scaledGray8(const aoi::ImageView& view) {
    QImage out(view.width, view.height, QImage::Format_Grayscale8);
    if (out.isNull()) return out;
    const int shift = std::max(0, aoi::effectiveBits(view) - 8);
    for (int y = 0; y < view.height; ++y) {
        const std::uint8_t* src = view.data + static_cast<std::size_t>(y) * static_cast<std::size_t>(view.strideBytes);
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < view.width; ++x) {
            std::uint16_t v = 0;
            std::memcpy(&v, src + 2 * static_cast<std::size_t>(x), sizeof v);
            dst[x] = static_cast<uchar>(std::min(255, v >> shift));
        }
    }
    aoi::countImageCopy(static_cast<std::size_t>(view.width) * static_cast<std::size_t>(view.height));
    return out;
}

}  // namespace

ImageItem::ImageItem() {
    // exposedRect 與 levelOfDetail 只有在這個旗標下才有值；沒有它，paint 每次都得畫整張影像。
    setFlag(ItemUsesExtendedStyleOption);
    setAcceptedMouseButtons(Qt::NoButton);
}

void ImageItem::setImage(const QImage& image) {
    prepareGeometryChange();
    image_ = image;
    update();
}

QImage ImageItem::wrap(const aoi::ImageView& view) {
    if (!view.valid()) return {};
    if (view.format == aoi::PixelFormat::Gray16 && aoi::effectiveBits(view) < 16) return scaledGray8(view);
    auto* holder = new std::shared_ptr<const void>(view.owner);
    QImage image(view.data, view.width, view.height, static_cast<qsizetype>(view.strideBytes),
                 toQtFormat(view.format), &releaseOwner, holder);
    if (image.isNull()) delete holder;  // Qt 沒有接手清理函式
    return image;
}

QRectF ImageItem::boundingRect() const { return QRectF(QPointF(0, 0), QSizeF(image_.size())); }

void ImageItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*) {
    if (image_.isNull()) return;
    QElapsedTimer timer;
    timer.start();
    // 只畫露出的部分：放大到 3200% 時，一次重繪最多碰到幾百個影像像素。
    const QRectF source = option->exposedRect.intersected(boundingRect());
    if (source.isEmpty()) return;
    const qreal lod = option->levelOfDetailFromTransform(painter->worldTransform());
    painter->setRenderHint(QPainter::SmoothPixmapTransform, lod < 1.0);
    painter->drawImage(source, image_, source);
    lastPaintMs_ = static_cast<double>(timer.nsecsElapsed()) / 1e6;
}

}  // namespace aoi::ui
