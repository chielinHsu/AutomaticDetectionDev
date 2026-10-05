#pragma once

#include <QGraphicsItem>
#include <QImage>

#include "domain/image_view.h"

namespace aoi::ui {

// 板子影像。場景座標就是影像像素：影像的左上角在 (0, 0)。
class ImageItem : public QGraphicsItem {
public:
    ImageItem();

    // QImage 本身是隱式共享，這裡只存一份參照，不複製像素。
    void setImage(const QImage& image);

    // 零複製：把 ImageView 的像素包成 QImage（共享擁有者放進清理函式，QImage 活著緩衝就活著）。
    // ImageView 無效時回傳空的 QImage。
    static QImage wrap(const aoi::ImageView& view);

    const QImage& image() const { return image_; }
    double lastPaintMs() const { return lastPaintMs_; }  // 可觀察性：最近一次 paint 的耗時

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QImage image_;
    double lastPaintMs_ = 0;
};

}  // namespace aoi::ui
