#pragma once

#include <QPointF>
#include <QRectF>
#include <optional>
#include <vector>

#include "presentation/tools/ITool.h"

namespace aoi::ui {

// V：點擊選取元件（Shift＋點擊加入或移除；點空白處清除）；雙擊開啟元件類型編輯器；
// 按住元件拖曳＝移動整個選取（S04）：拖曳中畫出「放開後會落在哪裡」的外框，放開時回報一次 onComponentsMoved
// （一次拖曳是一個 Command，ADR-002 規則 7）；拖曳中按 Esc 放棄（回報位移 0，Presenter 把畫面還原）。
// 按下的是還沒選取的元件時，先選取它再拖；按下的是已選取的元件時，整個選取一起走（不先縮成一個）。
// ADR-025（2026-10-14）：
//  - 按在單一選取元件的把手上拖曳＝改大小：拖曳中畫新的框，放開時回報一次 onComponentResized。
//  - 在空白處拖曳＝框選：放開時選取外框和框相交的元件（Shift＋框選＝加入）；沒拖動就是原本的點擊（清除選取）。
class SelectTool final : public ITool {
public:
    // 滑鼠移動超過這麼多螢幕像素才算拖曳；之內是點擊（手抖不會移動元件）。
    static constexpr double kDragStartScreenPx = 4.0;
    // 改大小時框的最小邊長（影像像素）：和最小樣本相同，Presenter 另外還會檢查。
    static constexpr double kMinResizeSidePx = 1.0;

    ToolId id() const override { return ToolId::Select; }
    Qt::CursorShape cursor() const override { return Qt::ArrowCursor; }
    void activate(IToolHost&) override {}
    void deactivate(IToolHost& host) override;
    bool handle(const ToolEvent& event, IToolHost& host) override;

    bool dragging() const { return drag_.has_value() || resize_.has_value() || marquee_.has_value(); }

private:
    struct Drag {
        std::vector<int> ids;
        QPointF start;
        QRectF outline;  // 開始拖曳時，這些元件的外框聯集
        bool moving = false;
    };
    struct Resize {
        int id = -1;
        int handle = 0;
        QPointF start;
        QRectF rect;   // 開始時的外框
        bool moving = false;
    };
    struct Marquee {
        QPointF start;
        std::vector<int> selectedAtPress;  // 按下時的選取（放開的事件不帶選取）
        bool additive = false;
        bool moving = false;
    };
    bool startDrag(const ToolEvent& event, IToolHost& host, const std::vector<int>& ids);
    void cancelDrag(IToolHost& host, bool report);
    static QRectF resizedRect(const Resize& resize, const QPointF& scenePos);

    std::optional<Drag> drag_;
    std::optional<Resize> resize_;
    std::optional<Marquee> marquee_;
};

// 橡皮筋框選的共用部分：按下 → 拖曳（即時顯示框）→ 放開時回報；拖曳中按 Esc 放棄。
// 不論框多小，放開都回報，由 Presenter 判斷「太小」；這樣工具不必知道最小尺寸規則。
class RubberBandTool : public ITool {
public:
    Qt::CursorShape cursor() const override { return Qt::CrossCursor; }
    void activate(IToolHost& host) override;
    void deactivate(IToolHost& host) override;
    bool handle(const ToolEvent& event, IToolHost& host) override;

    bool dragging() const { return anchor_.has_value(); }

protected:
    explicit RubberBandTool(DraftMode mode) : mode_(mode) {}

private:
    QRectF currentRect(const ToolEvent& event) const;
    void abort(IToolHost& host);

    DraftMode mode_;
    std::optional<QPointF> anchor_;  // 按下的點，已限制在影像內
    QRectF bounds_;
};

// S：建立樣本，框一個元件（2026-10-14 起放開只建立樣本，搜尋另外按；ADR-025）。
class SampleRoiTool final : public RubberBandTool {
public:
    SampleRoiTool() : RubberBandTool(DraftMode::Sample) {}
    ToolId id() const override { return ToolId::Sample; }
};

// R：區域掃描，框任意區域。
class RegionScanTool final : public RubberBandTool {
public:
    RegionScanTool() : RubberBandTool(DraftMode::Region) {}
    ToolId id() const override { return ToolId::Region; }
};

// B：在所選的元件上畫一個子框（放開時回報 onRegionDrawn，DraftMode::Box；Presenter 加到那個元件的類型上）。
class BoxTool final : public RubberBandTool {
public:
    BoxTool() : RubberBandTool(DraftMode::Box) {}
    ToolId id() const override { return ToolId::Box; }
};

// M：在所選的元件上設定比對區（ADR-026）：拖一個框 ＝ 矩形，點一下 ＝ 自動擷取（放開時回報 onRegionDrawn，DraftMode::Match；
// 點一下的框幾乎沒有大小，Presenter 用它的中心當點擊的位置）。
class MatchRegionTool final : public RubberBandTool {
public:
    MatchRegionTool() : RubberBandTool(DraftMode::Match) {}
    ToolId id() const override { return ToolId::Match; }
};

// A：在影像上點一下，補一個元件。
class AddComponentTool final : public ITool {
public:
    ToolId id() const override { return ToolId::Add; }
    Qt::CursorShape cursor() const override { return Qt::CrossCursor; }
    void activate(IToolHost&) override {}
    void deactivate(IToolHost&) override {}
    bool handle(const ToolEvent& event, IToolHost& host) override;
};

}  // namespace aoi::ui
