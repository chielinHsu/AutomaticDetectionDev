#pragma once

#include <QGraphicsItem>
#include <QPointF>
#include <QRectF>
#include <array>
#include <vector>

#include "presentation/canvas/BoxFrames.h"
#include "presentation/canvas/ComponentSnapshot.h"
#include "presentation/canvas/OverlayStyle.h"

namespace aoi::ui {

// 一個「元件（含它的框）」或一個「使用者正在拖的框」，依狀態套用 S01 的樣式。
// 用在少量、需要個別互動的地方：拖曳中的樣本／區域框、類型編輯器的框、（效能比較用的）逐一繪製。
// 大量的板上元件走 RoiLayerItem。
//
// 把手（類型編輯器用）：看起來 8 px、點擊區 24 px；框的螢幕邊長 < 72 px 時只放 4 個角並往外推 12 px，
// 這樣小框的把手不會互相重疊、也不會蓋住要拿來拖曳移動的框內部（docs/ux/ux-spec.md 第 8 節）。
class RoiGraphicsItem : public QGraphicsItem {
public:
    // Component ＝ 元件與它的框（狀態樣式）；SampleDraft／RegionDraft ＝ 拖曳中的框；Definition ＝ 類型編輯器的定義框。
    enum class Look { Component, SampleDraft, RegionDraft, Definition };
    enum class Handle { None, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };
    struct HandleSpot {
        Handle handle = Handle::None;
        QPointF center;  // 場景座標
    };

    explicit RoiGraphicsItem(OverlayStyle& style, QGraphicsItem* parent = nullptr);

    // Look::Component：框的座標是場景座標。
    void setComponent(const ComponentView& component);
    // 拖曳中的框：Sample ＝ 狀態樣式的樣本框；Region ＝ 白色細線＋淡填色。附尺寸標籤。
    // 寬或高為 0 的退化矩形只畫一個十字記號（按下滑鼠的瞬間就要有回饋）。
    void setDraft(const QRectF& rect, DraftMode mode);
    // 類型編輯器的定義框（中性白色）。
    void setDefinition(const QRectF& rect, BoxKind kind);

    void setHighlighted(bool on);
    void setHandlesVisible(bool on);
    // 畫布縮放時通知：影響邊界（標籤、把手要留空間）與把手位置。
    void setViewScale(qreal viewScale);

    QRectF rect() const { return primary_; }
    Look look() const { return look_; }

    std::vector<HandleSpot> handleSpots(qreal viewScale) const;
    Handle handleAt(const QPointF& scenePos, qreal viewScale) const;

    // 層級簡化與標籤的門檻，與 RoiLayerItem 相同。
    static constexpr double kMinBoxScreenSide = 3.0;
    static constexpr double kLabelMinScreenWidth = 48.0;

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    void rebuildGeometry();
    void paintComponent(QPainter* painter, qreal lod);
    void paintDraft(QPainter* painter, qreal lod);
    void paintHandles(QPainter* painter, qreal lod);

    OverlayStyle& style_;
    Look look_ = Look::Component;
    OverlayState state_ = OverlayState::Found;
    BoxKind definitionKind_ = BoxKind::Body;
    std::vector<BoxView> boxes_;  // 依種類排序：窗、Pin、主體（大的在下）
    QString label_;
    QRectF primary_;  // 主要的矩形：草稿框，或元件的外框；把手與標籤依它定位
    bool highlighted_ = false;
    bool handlesVisible_ = false;
    qreal viewScale_ = 1.0;
    QRectF bounds_;
    std::array<BoxFrames::Batch, kBoxKindCount> frames_;  // 每次繪製的暫存
};

}  // namespace aoi::ui
