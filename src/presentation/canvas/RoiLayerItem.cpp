#include "presentation/canvas/RoiLayerItem.h"

#include <QElapsedTimer>
#include <QFontMetrics>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <algorithm>
#include <array>

namespace aoi::ui {
namespace {

// 畫的順序：狀態由「拒絕」到「樣本」（樣本在最上面）；同狀態內由大到小（窗 → Pin → 主體），小框不被大框的外框蓋住。
constexpr OverlayState kStateOrder[] = {OverlayState::Rejected, OverlayState::Inferred, OverlayState::Found,
                                        OverlayState::Sample};
constexpr BoxKind kKindOrder[] = {BoxKind::Window, BoxKind::Pin, BoxKind::Body};

// 外框＋描邊的總半寬不會超過這個值（樣本 3 ＋ 選取 1 ＋ 外框 2，除以 2，再留餘裕）；單位：螢幕像素。
constexpr double kStrokeReachPx = 4.0;
// 選取外框離元件外框的距離，螢幕像素。
constexpr double kSelectionGapPx = 3.0;

}  // namespace

RoiLayerItem::RoiLayerItem(OverlayStyle& style) : style_(style) {
    setFlag(ItemUsesExtendedStyleOption);
    setAcceptedMouseButtons(Qt::NoButton);
}

void RoiLayerItem::setBounds(const QRectF& bounds) {
    if (bounds == bounds_) return;
    prepareGeometryChange();
    bounds_ = bounds;
}

void RoiLayerItem::setSnapshot(SnapshotPtr snapshot) {
    snapshot_ = std::move(snapshot);
    for (auto& bucket : buckets_) bucket.clear();
    idToIndex_.clear();
    selectedIndex_.clear();
    if (snapshot_) {
        const auto& comps = snapshot_->components;
        idToIndex_.reserve(static_cast<qsizetype>(comps.size()));
        for (std::size_t i = 0; i < comps.size(); ++i) {
            const ComponentView& c = comps[i];
            idToIndex_.insert(c.id, static_cast<int>(i));
            for (const BoxView& b : c.boxes) buckets_[static_cast<size_t>(bucketIndex(c.state, b.kind))].push_back(b.rect);
        }
        for (int id : selectedIds_) {
            const auto it = idToIndex_.constFind(id);
            if (it != idToIndex_.constEnd()) selectedIndex_.push_back(it.value());
        }
    }
    update();
}

void RoiLayerItem::setSelection(const std::vector<int>& componentIds) {
    selectedIds_ = componentIds;
    selectedIndex_.clear();
    for (int id : selectedIds_) {
        const auto it = idToIndex_.constFind(id);
        if (it != idToIndex_.constEnd()) selectedIndex_.push_back(it.value());
    }
    update();
}

int RoiLayerItem::componentAt(const QPointF& scenePos) const {
    if (!snapshot_) return -1;
    int best = -1;
    qreal bestArea = 0;
    for (const ComponentView& c : snapshot_->components) {
        if (!c.outline.contains(scenePos)) continue;
        const qreal area = c.outline.width() * c.outline.height();
        if (best < 0 || area <= bestArea) {
            best = c.id;
            bestArea = area;
        }
    }
    return best;
}

QRectF RoiLayerItem::outlineOf(const std::vector<int>& componentIds) const {
    QRectF united;
    if (!snapshot_) return united;
    for (int id : componentIds) {
        const auto it = idToIndex_.constFind(id);
        if (it == idToIndex_.constEnd()) continue;
        const QRectF& r = snapshot_->components[static_cast<size_t>(it.value())].outline;
        united = united.isNull() ? r : united.united(r);
    }
    return united;
}

void RoiLayerItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*) {
    stats_ = {};
    if (!snapshot_ || snapshot_->components.empty()) return;
    QElapsedTimer timer;
    timer.start();

    style_.setDevicePixelRatio(painter->device()->devicePixelRatioF());
    const qreal lod = option->levelOfDetailFromTransform(painter->worldTransform());
    const qreal margin = kStrokeReachPx / lod;
    const QRectF cull = option->exposedRect.adjusted(-margin, -margin, margin, margin);
    const qreal minSceneSide = kMinBoxScreenSide / lod;

    // 這個 Item 不依賴 View 還原畫筆狀態（View 有 DontSavePainterState），所以用到的狀態每次都明確設定。
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setBrush(Qt::NoBrush);

    // 場景矩形 → 螢幕矩形：這個轉換只有縮放與平移（畫布沒有旋轉）。
    const QTransform world = painter->worldTransform();
    const double sx = world.m11(), sy = world.m22(), tx = world.dx(), ty = world.dy();

    for (OverlayState state : kStateOrder) {
        // 整數像素的填色細矩形（BoxFrames）：實線快約 5 倍，點線快約 17 倍（cosmetic 的圓端點點線每個框要 68 µs）。
        // 只有花樣不是「畫、空」一組的狀態才退回 cosmetic 描邊（目前沒有）。
        const bool solid = style_.frameable(state);
        std::array<bool, kBoxKindCount> hasFrames{};
        for (BoxKind kind : kKindOrder) {
            const auto& bucket = buckets_[static_cast<size_t>(bucketIndex(state, kind))];
            if (bucket.empty()) continue;
            stats_.boxesConsidered += static_cast<int>(bucket.size());

            visible_.clear();
            const bool isBody = kind == BoxKind::Body;
            for (const QRectF& r : bucket) {
                if (!cull.intersects(r)) continue;
                if (!isBody && std::min(r.width(), r.height()) < minSceneSide) {
                    ++stats_.boxesSkippedByLod;
                    continue;
                }
                visible_.push_back(r);
            }
            if (visible_.empty()) continue;
            stats_.boxesDrawn += static_cast<int>(visible_.size());
            ++stats_.bucketsDrawn;

            if (solid) {
                BoxFrames::Batch& batch = frames_[static_cast<size_t>(kindIndex(kind))];
                batch.clear();
                const int t = style_.frameThickness(state, kind, false);
                const BoxFrames::DashSpec dash = style_.dashSpec(state, kind, false);
                for (const QRectF& r : visible_) {
                    const QRectF screen(r.x() * sx + tx, r.y() * sy + ty, r.width() * sx, r.height() * sy);
                    if (dash.valid()) BoxFrames::appendDashed(batch, screen, t, dash);
                    else BoxFrames::append(batch, screen, t);
                }
                hasFrames[static_cast<size_t>(kindIndex(kind))] = true;
            } else {
                const PenPair& pens = style_.box(state, kind, false);
                painter->setPen(pens.casing);
                painter->drawRects(visible_.data(), static_cast<int>(visible_.size()));
                painter->setPen(pens.stroke);
                painter->drawRects(visible_.data(), static_cast<int>(visible_.size()));
            }
        }
        if (solid && std::any_of(hasFrames.begin(), hasFrames.end(), [](bool b) { return b; })) {
            const StrokeColors& colors = style_.colors(state);
            painter->save();
            painter->resetTransform();  // 填色矩形的座標是螢幕像素
            for (BoxKind kind : kKindOrder) {
                if (hasFrames[static_cast<size_t>(kindIndex(kind))])
                    BoxFrames::draw(*painter, frames_[static_cast<size_t>(kindIndex(kind))], colors.casing, colors.stroke);
            }
            painter->restore();
        }
    }

    paintSelection(painter, lod);
    paintLabels(painter, cull, lod);
    stats_.paintMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
}

void RoiLayerItem::paintSelection(QPainter* painter, qreal lod) {
    if (selectedIndex_.empty()) return;
    const qreal minSceneSide = kMinBoxScreenSide / lod;
    const qreal gap = kSelectionGapPx / lod;
    for (int index : selectedIndex_) {
        const ComponentView& c = snapshot_->components[static_cast<size_t>(index)];
        for (const BoxView& b : c.boxes) {
            if (b.kind != BoxKind::Body && std::min(b.rect.width(), b.rect.height()) < minSceneSide) continue;
            const PenPair& pens = style_.box(c.state, b.kind, true);
            painter->setPen(pens.casing);
            painter->drawRect(b.rect);
            painter->setPen(pens.stroke);
            painter->drawRect(b.rect);
        }
        const QRectF ring = c.outline.adjusted(-gap, -gap, gap, gap);
        painter->setPen(style_.accentLine().casing);
        painter->drawRect(ring);
        painter->setPen(style_.accentLine().stroke);
        painter->drawRect(ring);
    }
}

void RoiLayerItem::paintLabels(QPainter* painter, const QRectF& cull, qreal lod) {
    // 標籤用裝置座標畫（字級不隨縮放變），先收集再一次畫，避免每個標籤 save／restore。
    const QTransform world = painter->worldTransform();
    bool started = false;
    const QFontMetrics fm(painter->font());
    for (const ComponentView& c : snapshot_->components) {
        if (c.label.isEmpty() || c.outline.width() * lod < kLabelMinScreenWidth) continue;
        if (!cull.intersects(c.outline)) continue;
        if (!started) {
            painter->save();
            painter->resetTransform();
            started = true;
        }
        const QPointF origin = world.map(c.outline.topLeft());
        const int w = fm.horizontalAdvance(c.label) + 8;
        const int h = fm.height() + 2;
        const QRectF pill(origin.x(), origin.y() - h - 3, w, h);
        const StrokeColors& colors = style_.tokens().overlay.state[static_cast<size_t>(stateIndex(c.state))];
        QColor bg = colors.casing;
        bg.setAlphaF(0.82f);
        painter->fillRect(pill, bg);
        painter->setPen(colors.stroke);
        painter->drawText(pill.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, c.label);
        ++stats_.labelsDrawn;
    }
    if (started) painter->restore();
}

}  // namespace aoi::ui
