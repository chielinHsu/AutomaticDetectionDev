#include "presentation/compare/CompareOverlayItem.h"

#include <QElapsedTimer>
#include <QPaintDevice>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QStyleOptionGraphicsItem>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <utility>

namespace aoi::ui {

namespace {

constexpr double kStrokePx = 1.5;  // 描邊（裝置無關的螢幕像素）
constexpr double kCasingPx = 3.5;  // 外框：描邊底下較寬的深色線，讓標記在任何影像上都看得到

QPen cosmeticPen(const QColor& color, double widthPx, qreal dpr) {
    QPen pen(color, widthPx * dpr);  // cosmetic 筆的寬度單位是裝置像素（OverlayStyle 的實測）
    pen.setCosmetic(true);
    pen.setJoinStyle(Qt::MiterJoin);
    return pen;
}

// 元件的框：以中心、大小與角度（ADR-010：螢幕上逆時針為正，y 向下）算四個角。
QPolygonF partOutline(const CompareMark& m, const QSizeF& size) {
    const double rad = m.angleDeg * 3.14159265358979323846 / 180.0;
    const double c = std::cos(rad);
    const double s = std::sin(rad);
    const double hw = size.width() * 0.5;
    const double hh = size.height() * 0.5;
    QPolygonF poly;
    for (const auto& [dx, dy] : {std::pair{-hw, -hh}, std::pair{hw, -hh}, std::pair{hw, hh}, std::pair{-hw, hh}}) {
        // geometry.h 的 rotate：x' = x·cos + y·sin，y' = −x·sin + y·cos
        poly << QPointF(m.center.x() + dx * c + dy * s, m.center.y() - dx * s + dy * c);
    }
    return poly;
}

// 小圖的邊長（裝置無關的像素）：標記加上外框的寬度。
constexpr double kSpritePx = CompareOverlayItem::kMarkerScreenPx + 6.0;

void drawMarker(QPainter* painter, aoi::CompareKind kind, const QPointF& c, double half) {
    switch (kind) {
        case aoi::CompareKind::Both:
            painter->drawEllipse(c, half, half);
            break;
        case aoi::CompareKind::OnlyA:
            painter->drawRect(QRectF(c.x() - half, c.y() - half, 2.0 * half, 2.0 * half));
            break;
        case aoi::CompareKind::OnlyB: {
            const QPointF tri[3] = {QPointF(c.x(), c.y() - half * 1.15), QPointF(c.x() + half * 1.1, c.y() + half * 0.85),
                                    QPointF(c.x() - half * 1.1, c.y() + half * 0.85)};
            painter->drawPolygon(tri, 3);
            break;
        }
    }
}

}  // namespace

CompareOverlayItem::CompareOverlayItem(const ThemeTokens& tokens) {
    colors_[index(aoi::CompareKind::Both)] = tokens.overlay.state[static_cast<size_t>(stateIndex(OverlayState::Found))].stroke;
    colors_[index(aoi::CompareKind::OnlyA)] = tokens.overlay.state[static_cast<size_t>(stateIndex(OverlayState::Sample))].stroke;
    colors_[index(aoi::CompareKind::OnlyB)] = tokens.overlay.state[static_cast<size_t>(stateIndex(OverlayState::Inferred))].stroke;
    casing_ = tokens.overlay.state[static_cast<size_t>(stateIndex(OverlayState::Found))].casing;
    accent_ = tokens.light.surface.accent;
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);  // exposedRect：只畫露出的部分
}

void CompareOverlayItem::rebuildSprites(qreal dpr) {
    spriteDpr_ = dpr;
    const int side = static_cast<int>(std::ceil(kSpritePx * dpr));
    for (const aoi::CompareKind kind : {aoi::CompareKind::Both, aoi::CompareKind::OnlyA, aoi::CompareKind::OnlyB}) {
        QPixmap sprite(side, side);
        sprite.setDevicePixelRatio(dpr);
        sprite.fill(Qt::transparent);
        QPainter p(&sprite);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setBrush(Qt::NoBrush);
        const QPointF center(kSpritePx * 0.5, kSpritePx * 0.5);
        const double half = 0.5 * kMarkerScreenPx;
        QPen casing(casing_, kCasingPx);
        casing.setJoinStyle(Qt::MiterJoin);
        p.setPen(casing);
        drawMarker(&p, kind, center, half);
        QPen stroke(colors_[index(kind)], kind == aoi::CompareKind::Both ? kStrokePx : kStrokePx + 0.5);
        stroke.setJoinStyle(Qt::MiterJoin);
        p.setPen(stroke);
        drawMarker(&p, kind, center, half);
        sprites_[index(kind)] = sprite;
    }
}

void CompareOverlayItem::setBounds(const QRectF& bounds) {
    prepareGeometryChange();
    bounds_ = bounds;
}

void CompareOverlayItem::setOverlay(CompareOverlayPtr overlay) {
    overlay_ = std::move(overlay);
    highlight_ = -1;
    update();
}

void CompareOverlayItem::setKindVisible(aoi::CompareKind kind, bool visible) {
    if (visible_[index(kind)] == visible) return;
    visible_[index(kind)] = visible;
    update();
}

void CompareOverlayItem::setHighlight(int markIndex) {
    if (highlight_ == markIndex) return;
    highlight_ = markIndex;
    update();
}

void CompareOverlayItem::setAccent(const QColor& accent) {
    accent_ = accent;
    update();
}

void CompareOverlayItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*) {
    QElapsedTimer timer;
    timer.start();
    stats_ = {};
    if (!overlay_ || overlay_->marks.empty()) return;

    const qreal lod = QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform());
    const qreal dpr = painter->device() != nullptr ? painter->device()->devicePixelRatioF() : 1.0;
    const double half = 0.5 * kMarkerScreenPx / lod;  // 場景單位
    const QSizeF part = overlay_->partSize;
    const bool outlines = std::min(part.width(), part.height()) * lod >= kOutlineMinScreenPx;
    const double reach = std::max(half * 1.2, 0.75 * std::hypot(part.width(), part.height()));
    const QRectF cull = option->exposedRect.adjusted(-reach, -reach, reach, reach);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);
    const QPen casing = cosmeticPen(casing_, kCasingPx, dpr);
    // 元件的框（放大到看得清楚時才有；這時露出的元件不多，逐一描邊）。
    if (outlines) {
        for (const aoi::CompareKind kind : {aoi::CompareKind::Both, aoi::CompareKind::OnlyA, aoi::CompareKind::OnlyB}) {
            if (!visible_[index(kind)]) continue;
            const QPen stroke = cosmeticPen(colors_[index(kind)], kind == aoi::CompareKind::Both ? kStrokePx : kStrokePx + 0.5, dpr);
            for (const QPen* pen : {&casing, &stroke}) {
                painter->setPen(*pen);
                for (const CompareMark& m : overlay_->marks) {
                    if (m.kind != kind || !cull.contains(m.center)) continue;
                    painter->drawPolygon(partOutline(m, part));
                    if (pen == &stroke) ++stats_.outlinesDrawn;
                }
            }
        }
    }
    // 標記：每一類的小圖貼在每個位置（螢幕座標，大小固定）。兩邊都找到的先貼，差異貼在上面。
    if (!qFuzzyCompare(spriteDpr_, dpr) || sprites_[0].isNull()) rebuildSprites(dpr);
    const QTransform world = painter->worldTransform();
    painter->setWorldTransform(QTransform());
    const QPointF spriteOffset(kSpritePx * 0.5, kSpritePx * 0.5);
    for (const aoi::CompareKind kind : {aoi::CompareKind::Both, aoi::CompareKind::OnlyA, aoi::CompareKind::OnlyB}) {
        if (!visible_[index(kind)]) continue;
        const QPixmap& sprite = sprites_[index(kind)];
        for (const CompareMark& m : overlay_->marks) {
            if (m.kind != kind || !cull.contains(m.center)) continue;
            painter->drawPixmap(world.map(m.center) - spriteOffset, sprite);
            ++stats_.marksDrawn;
        }
    }
    painter->setWorldTransform(world);
    if (highlight_ >= 0 && highlight_ < static_cast<int>(overlay_->marks.size())) {
        const CompareMark& m = overlay_->marks[static_cast<std::size_t>(highlight_)];
        const double ring = half * 2.2;
        painter->setPen(casing);
        painter->drawEllipse(m.center, ring, ring);
        painter->setPen(cosmeticPen(accent_, kStrokePx + 0.5, dpr));
        painter->drawEllipse(m.center, ring, ring);
    }
    painter->restore();
    stats_.paintMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
}

}  // namespace aoi::ui
