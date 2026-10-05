#include "presentation/canvas/ImageItem.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <memory>

namespace aoi::ui {
namespace {

QImage::Format toQtFormat(aoi::PixelFormat f) {
    switch (f) {
        case aoi::PixelFormat::Gray8: return QImage::Format_Grayscale8;
        case aoi::PixelFormat::Rgb24: return QImage::Format_RGB888;
        case aoi::PixelFormat::Bgrx32: return QImage::Format_RGB32;
    }
    return QImage::Format_Invalid;
}

void releaseOwner(void* info) { delete static_cast<std::shared_ptr<const void>*>(info); }

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
