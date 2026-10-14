#include "presentation/canvas/RoiGraphicsItem.h"

#include <QFontMetrics>
#include <QPainter>
#include <algorithm>
#include <cmath>

namespace aoi::ui {
namespace {

// 邊界的最小留白（場景單位）。描邊最寬約 5 px 螢幕寬，縮到最小 5% 時是 100 場景單位的一半，
// 取 50 讓逐一繪製的元件不必在每次縮放時重算邊界。
constexpr double kMinBoundsMargin = 50.0;
constexpr double kHandleReachPx = 40.0;  // 把手外推 12、半個點擊區 12、標籤高度
constexpr double kSelectionGapPx = 3.0;
constexpr double kCrossHalfPx = 6.0;

int kindOrder(BoxKind k) {
    // 窗 → Pin → 主體
    switch (k) {
        case BoxKind::Window: return 0;
        case BoxKind::Pin: return 1;
        case BoxKind::Body: return 2;
    }
    return 2;
}

QString sizeText(const QRectF& r) {
    return QStringLiteral("%1 × %2 px").arg(qRound(r.width())).arg(qRound(r.height()));
}

}  // namespace

RoiGraphicsItem::RoiGraphicsItem(OverlayStyle& style, QGraphicsItem* parent) : QGraphicsItem(parent), style_(style) {
    setAcceptedMouseButtons(Qt::NoButton);
}

void RoiGraphicsItem::setComponent(const ComponentView& component) {
    look_ = Look::Component;
    state_ = component.state;
    label_ = component.label();
    boxes_ = component.boxes;
    std::stable_sort(boxes_.begin(), boxes_.end(),
                     [](const BoxView& a, const BoxView& b) { return kindOrder(a.kind) < kindOrder(b.kind); });
    primary_ = component.outline;
    rebuildGeometry();
}

void RoiGraphicsItem::setDraft(const QRectF& rect, DraftMode mode) {
    look_ = mode == DraftMode::Sample ? Look::SampleDraft : Look::RegionDraft;  // 子框（Box）的畫法和區域相同
    state_ = OverlayState::Sample;
    label_.clear();
    boxes_.clear();
    boxes_.push_back({rect, BoxKind::Body});
    primary_ = rect;
    rebuildGeometry();
}

void RoiGraphicsItem::setDefinition(const QRectF& rect, BoxKind kind) {
    look_ = Look::Definition;
    definitionKind_ = kind;
    label_.clear();
    boxes_.clear();
    boxes_.push_back({rect, kind});
    primary_ = rect;
    rebuildGeometry();
}

void RoiGraphicsItem::setHighlighted(bool on) {
    if (on == highlighted_) return;
    highlighted_ = on;
    update();
}

void RoiGraphicsItem::setHandlesVisible(bool on) {
    if (on == handlesVisible_) return;
    handlesVisible_ = on;
    rebuildGeometry();
}

void RoiGraphicsItem::setViewScale(qreal viewScale) {
    if (viewScale == viewScale_ || viewScale <= 0) return;
    viewScale_ = viewScale;
    rebuildGeometry();
}

void RoiGraphicsItem::rebuildGeometry() {
    prepareGeometryChange();
    QRectF united = primary_;
    for (const BoxView& b : boxes_) united = united.united(b.rect);
    const bool needsRoom = handlesVisible_ || look_ != Look::Component || !label_.isEmpty();
    const double margin = needsRoom ? std::max(kMinBoundsMargin, kHandleReachPx / viewScale_) : kMinBoundsMargin;
    bounds_ = united.adjusted(-margin, -margin, margin, margin);
    update();
}

QRectF RoiGraphicsItem::boundingRect() const { return bounds_; }

std::vector<RoiGraphicsItem::HandleSpot> RoiGraphicsItem::handleSpots(qreal s) const {
    std::vector<HandleSpot> spots;
    if (primary_.width() <= 0 || primary_.height() <= 0) return spots;  // 退化的框沒有把手
    const SizeTokens& sz = style_.tokens().sizes;
    const qreal l = primary_.left(), r = primary_.right(), t = primary_.top(), b = primary_.bottom();
    const qreal cx = primary_.center().x(), cy = primary_.center().y();
    const bool small = std::min(primary_.width(), primary_.height()) * s < sz.handlesMoveOutsideBelow;
    if (small) {
        // 只放 4 個角，往外推：點擊區不互相重疊，框內部留給拖曳移動。
        const qreal push = 12.0 / s;
        spots = {{Handle::TopLeft, {l - push, t - push}},
                 {Handle::TopRight, {r + push, t - push}},
                 {Handle::BottomRight, {r + push, b + push}},
                 {Handle::BottomLeft, {l - push, b + push}}};
    } else {
        spots = {{Handle::TopLeft, {l, t}},     {Handle::TopRight, {r, t}},      {Handle::BottomRight, {r, b}},
                 {Handle::BottomLeft, {l, b}},  {Handle::Top, {cx, t}},          {Handle::Right, {r, cy}},
                 {Handle::Bottom, {cx, b}},     {Handle::Left, {l, cy}}};
    }
    return spots;
}

RoiGraphicsItem::Handle RoiGraphicsItem::handleAt(const QPointF& scenePos, qreal s) const {
    const qreal half = style_.tokens().sizes.handleHit / 2.0 / s;  // 點擊區 24 px 的一半，換成場景單位
    for (const HandleSpot& spot : handleSpots(s)) {  // 角在前：角與邊重疊時優先取角
        if (std::abs(scenePos.x() - spot.center.x()) <= half && std::abs(scenePos.y() - spot.center.y()) <= half)
            return spot.handle;
    }
    return Handle::None;
}

void RoiGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
    style_.setDevicePixelRatio(painter->device()->devicePixelRatioF());
    const qreal lod = painter->worldTransform().m11();
    painter->setRenderHint(QPainter::Antialiasing, false);

    if (look_ == Look::Component) paintComponent(painter, lod);
    else paintDraft(painter, lod);

    if (handlesVisible_) paintHandles(painter, lod);
}

void RoiGraphicsItem::paintComponent(QPainter* painter, qreal lod) {
    painter->setBrush(Qt::NoBrush);
    const qreal minSceneSide = kMinBoxScreenSide / lod;
    if (style_.solid(state_)) {
        // 實線：和 RoiLayerItem 同一種畫法（整數像素的填色細矩形）。boxes_ 已依「窗、Pin、主體」排序。
        const QTransform world = painter->worldTransform();
        const double sx = world.m11(), sy = world.m22(), tx = world.dx(), ty = world.dy();
        for (BoxFrames::Batch& batch : frames_) batch.clear();
        for (const BoxView& box : boxes_) {
            if (box.kind != BoxKind::Body && std::min(box.rect.width(), box.rect.height()) < minSceneSide) continue;
            const QRectF& r = box.rect;
            BoxFrames::append(frames_[static_cast<size_t>(kindIndex(box.kind))],
                              QRectF(r.x() * sx + tx, r.y() * sy + ty, r.width() * sx, r.height() * sy),
                              style_.frameThickness(state_, box.kind, highlighted_));
        }
        const StrokeColors& colors = style_.colors(state_);
        painter->save();
        painter->resetTransform();
        for (const BoxFrames::Batch& batch : frames_) BoxFrames::draw(*painter, batch, colors.casing, colors.stroke);
        painter->restore();
        painter->setBrush(Qt::NoBrush);
    } else {
        for (const BoxView& b : boxes_) {
            if (b.kind != BoxKind::Body && std::min(b.rect.width(), b.rect.height()) < minSceneSide) continue;
            const PenPair& pens = style_.box(state_, b.kind, highlighted_);
            painter->setPen(pens.casing);
            painter->drawRect(b.rect);
            painter->setPen(pens.stroke);
            painter->drawRect(b.rect);
        }
    }
    if (highlighted_) {
        const qreal gap = kSelectionGapPx / lod;
        const QRectF ring = primary_.adjusted(-gap, -gap, gap, gap);
        painter->setPen(style_.accentLine().casing);
        painter->drawRect(ring);
        painter->setPen(style_.accentLine().stroke);
        painter->drawRect(ring);
    }
    if (!label_.isEmpty() && primary_.width() * lod >= kLabelMinScreenWidth) {
        const StrokeColors& colors = style_.tokens().overlay.state[static_cast<size_t>(stateIndex(state_))];
        const QTransform world = painter->worldTransform();
        const QFontMetrics fm(painter->font());
        const QPointF origin = world.map(primary_.topLeft());
        const int w = fm.horizontalAdvance(label_) + 8;
        const int h = fm.height() + 2;
        const QRectF pill(origin.x(), origin.y() - h - 3, w, h);
        painter->save();
        painter->resetTransform();
        QColor bg = colors.casing;
        bg.setAlphaF(0.82f);
        painter->fillRect(pill, bg);
        painter->setPen(colors.stroke);
        painter->drawText(pill.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, label_);
        painter->restore();
    }
}

void RoiGraphicsItem::paintDraft(QPainter* painter, qreal lod) {
    const QRectF r = primary_;
    const bool degenerate = r.width() <= 0 || r.height() <= 0;
    const bool isRegion = look_ == Look::RegionDraft;
    const bool isDefinition = look_ == Look::Definition;

    if (degenerate) {
        // 按下的瞬間：一個十字記號，讓使用者知道框從哪裡開始。
        const PenPair& pens = isRegion ? style_.region() : style_.box(OverlayState::Sample, BoxKind::Body, false);
        const qreal arm = kCrossHalfPx / lod;
        const QPointF c = r.topLeft();
        for (const QPen* pen : {&pens.casing, &pens.stroke}) {
            painter->setPen(*pen);
            painter->drawLine(QPointF(c.x() - arm, c.y()), QPointF(c.x() + arm, c.y()));
            painter->drawLine(QPointF(c.x(), c.y() - arm), QPointF(c.x(), c.y() + arm));
        }
        return;
    }

    if (isRegion) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(style_.regionFill());
        painter->drawRect(r);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(style_.region().casing);
        painter->drawRect(r);
        painter->setPen(style_.region().stroke);
        painter->drawRect(r);
    } else if (isDefinition) {
        const PenPair& pens = style_.definition(definitionKind_);
        if (definitionKind_ == BoxKind::Pin) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(style_.definitionPinFill());
            painter->drawRect(r);
        }
        painter->setBrush(Qt::NoBrush);
        painter->setPen(pens.casing);
        painter->drawRect(r);
        painter->setPen(pens.stroke);
        painter->drawRect(r);
    } else {
        const PenPair& pens = style_.box(OverlayState::Sample, BoxKind::Body, false);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(pens.casing);
        painter->drawRect(r);
        painter->setPen(pens.stroke);
        painter->drawRect(r);
    }

    if (!isDefinition) {
        // 尺寸標籤，畫在框的左上角外側（裝置座標，字級不隨縮放變）。
        const QTransform world = painter->worldTransform();
        const QFontMetrics fm(painter->font());
        const QString text = sizeText(r);
        const QPointF origin = world.map(r.topLeft());
        const int w = fm.horizontalAdvance(text) + 8;
        const int h = fm.height() + 2;
        const QRectF pill(origin.x(), origin.y() - h - 3, w, h);
        const StrokeColors& colors = style_.tokens().overlay.state[static_cast<size_t>(stateIndex(OverlayState::Sample))];
        painter->save();
        painter->resetTransform();
        QColor bg = colors.casing;
        bg.setAlphaF(0.82f);
        painter->fillRect(pill, bg);
        painter->setPen(colors.stroke);
        painter->drawText(pill.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
        painter->restore();
    }
}

void RoiGraphicsItem::paintHandles(QPainter* painter, qreal lod) {
    const qreal half = style_.tokens().sizes.handleVisual / 2.0;
    const QTransform world = painter->worldTransform();
    painter->save();
    painter->resetTransform();
    painter->setBrush(style_.tokens().overlay.defStroke);
    painter->setPen(style_.accentLine().stroke);
    for (const HandleSpot& spot : handleSpots(lod)) {
        const QPointF c = world.map(spot.center);
        painter->drawRect(QRectF(c.x() - half, c.y() - half, half * 2, half * 2));
    }
    painter->restore();
}

}  // namespace aoi::ui
