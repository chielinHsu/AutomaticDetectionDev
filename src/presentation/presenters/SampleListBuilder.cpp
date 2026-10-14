#include "presentation/presenters/SampleListBuilder.h"

#include <QColor>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <cmath>

#include "domain/box_expander.h"
#include "presentation/canvas/ImageItem.h"

namespace aoi::ui {

namespace {

// 縮圖裡的框：主體用藍、子框用橙（固定色；縮圖只是預覽，不走畫布的主題）。
const QColor kBodyColor(0x3C, 0x8B, 0xE0);
const QColor kPinColor(0xF2, 0xA6, 0x2E);

// 主體框外加 25%（至少 6 px）的邊界，夾在影像內。
Rect thumbnailSource(const Rect& body, int imageWidth, int imageHeight) {
    const double mx = std::max(6.0, body.w * 0.25);
    const double my = std::max(6.0, body.h * 0.25);
    const double x0 = std::clamp(std::floor(body.x - mx), 0.0, static_cast<double>(imageWidth));
    const double y0 = std::clamp(std::floor(body.y - my), 0.0, static_cast<double>(imageHeight));
    const double x1 = std::clamp(std::ceil(body.right() + mx), 0.0, static_cast<double>(imageWidth));
    const double y1 = std::clamp(std::ceil(body.bottom() + my), 0.0, static_cast<double>(imageHeight));
    return Rect{x0, y0, x1 - x0, y1 - y0};
}

}  // namespace

SampleListPtr buildSampleList(const InspectionSession& session, const ImageView& image, int thumbnailWidth) {
    auto list = std::make_shared<SampleList>();
    const InspectionDocument& document = session.document();
    const Board& board = document.board();
    list->umPerPixel = board.umPerPixel;
    for (const SampleOnBoard& s : document.samples()) {
        const ComponentInstance* c = document.find(s.componentId);
        const ComponentType* type = document.findType(s.typeId);
        if (c == nullptr || type == nullptr) continue;
        SampleEntry e;
        e.sceneId = c->id;
        e.name = QString::fromStdString(c->name);
        e.typeLabel = QString::fromStdString(type->partNumber.empty() ? type->id : type->partNumber);
        e.width = s.rect.w;
        e.height = s.rect.h;
        e.boxCount = countBoxes(*type);

        if (image.valid() && thumbnailWidth > 0) {
            const Rect src = thumbnailSource(s.rect, image.width, image.height);
            const int x = static_cast<int>(src.x);
            const int y = static_cast<int>(src.y);
            const int w = static_cast<int>(src.w);
            const int h = static_cast<int>(src.h);
            const ImageView sub = subView(image, x, y, w, h);
            if (sub.valid()) {
                // 裁下來的那一小塊才複製（wrap 對 12 bit 的影像會換到 8 bit；之後縮圖與畫框都在這一小塊上）。
                const QImage crop = ImageItem::wrap(sub).convertToFormat(QImage::Format_RGB32);
                QImage thumb = crop.scaledToWidth(thumbnailWidth, Qt::SmoothTransformation);
                const double scale = static_cast<double>(thumb.width()) / static_cast<double>(w);
                QPainter painter(&thumb);
                painter.setRenderHint(QPainter::Antialiasing, false);
                for (const ExpandedBox& b : expandBoxes(*type, c->pose, board.umPerPixel)) {
                    const Rect r = boundingRect(b.rect);
                    painter.setPen(QPen(b.kind == BoxGroupKind::Body ? kBodyColor : kPinColor, 1.0));
                    painter.drawRect(QRectF((r.x - x) * scale, (r.y - y) * scale, r.w * scale, r.h * scale));
                }
                painter.end();
                e.thumbnail = std::move(thumb);
            }
        }
        list->entries.push_back(std::move(e));
    }
    return list;
}

SampleListPtr sampleListFor(SampleListCache& cache, const InspectionSession& session, const ImageView& image,
                            int thumbnailWidth) {
    const std::uint64_t revision = session.document().revision();
    if (cache.valid && cache.list && cache.revision == revision && cache.imageData == image.data &&
        cache.significantBits == image.significantBits) {
        return cache.list;
    }
    cache.list = buildSampleList(session, image, thumbnailWidth);
    cache.valid = true;
    cache.revision = revision;
    cache.imageData = image.data;
    cache.significantBits = image.significantBits;
    return cache.list;
}

}  // namespace aoi::ui
