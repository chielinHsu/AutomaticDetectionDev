#pragma once

#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <Qt>
#include <optional>
#include <vector>

#include "presentation/canvas/ICanvasController.h"
#include "presentation/canvas/OverlayTypes.h"
#include "presentation/tools/ToolId.h"

namespace aoi::ui {

// 畫布把原始的 Qt 事件翻成這個值再交給工具。工具只看這個值就能決定怎麼做，
// 所以工具不認識 QGraphicsView，也不必自己轉座標。
struct ToolEvent {
    enum class Type { Press, Move, Release, DoubleClick, KeyPress };

    Type type = Type::Move;
    QPointF scenePos;          // 影像像素
    Qt::MouseButton button = Qt::NoButton;
    Qt::KeyboardModifiers modifiers;
    int key = 0;               // KeyPress 時的 Qt::Key
    double viewScale = 1.0;    // 螢幕像素／影像像素
    QRectF imageBounds;        // 影像矩形；沒有影像時是空的
    int hitComponent = -1;     // 只有 Press 與 DoubleClick 會算（點擊判定要走過所有元件）
    std::vector<int> selected; // 目前選取的元件編號

    bool insideImage() const { return imageBounds.contains(scenePos); }
};

// 工具能向畫布要的東西：外界的兩個 controller，以及顯示拖曳中的框。
class IToolHost {
public:
    virtual ~IToolHost() = default;

    virtual ICanvasController* canvasController() = 0;
    virtual ISelectionController* selectionController() = 0;
    virtual void showRegionDraft(std::optional<QRectF> rect, DraftMode mode) = 0;
};

// 一個工具一個類別（SOLID 的 S）；新增工具＝新增類別＋在 ToolRegistry 註冊，不改畫布（O）。
// 方法數 ≤ 5（I）。
class ITool {
public:
    virtual ~ITool() = default;

    virtual ToolId id() const = 0;
    virtual Qt::CursorShape cursor() const = 0;
    virtual void activate(IToolHost& host) = 0;
    virtual void deactivate(IToolHost& host) = 0;  // 放棄進行中的操作（例如拖到一半的框）
    // 回傳 true ＝ 這個事件被工具用掉了（畫布不再處理，例如 Esc 不會再清除選取）。
    virtual bool handle(const ToolEvent& event, IToolHost& host) = 0;
};

}  // namespace aoi::ui
