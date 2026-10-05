#include "presentation/tools/Tools.h"

#include <algorithm>

namespace aoi::ui {

// ---- SelectTool ------------------------------------------------------------------------------------------

bool SelectTool::handle(const ToolEvent& event, IToolHost& host) {
    using Type = ToolEvent::Type;
    if (event.button != Qt::LeftButton) return false;

    if (event.type == Type::Press) {
        ISelectionController* selection = host.selectionController();
        if (!selection) return true;
        const int hit = event.hitComponent;
        if (hit < 0) {
            // 點空白處：清除選取；Shift＋點空白處不動，避免手滑丟掉多選。
            if (!(event.modifiers & Qt::ShiftModifier) && !event.selected.empty()) selection->select({});
            return true;
        }
        std::vector<int> next;
        if (event.modifiers & Qt::ShiftModifier) {
            next = event.selected;
            const auto it = std::find(next.begin(), next.end(), hit);
            if (it == next.end()) next.push_back(hit);
            else next.erase(it);
        } else {
            next = {hit};
        }
        selection->select(next);
        return true;
    }
    if (event.type == Type::DoubleClick && event.hitComponent >= 0) {
        if (ICanvasController* controller = host.canvasController())
            controller->onOpenTypeEditor(event.hitComponent, event.scenePos);
        return true;
    }
    return false;
}

// ---- RubberBandTool --------------------------------------------------------------------------------------

void RubberBandTool::activate(IToolHost&) { anchor_.reset(); }

void RubberBandTool::deactivate(IToolHost& host) {
    if (anchor_) abort(host);
}

QRectF RubberBandTool::currentRect(const ToolEvent& event) const {
    // 拖出影像外時，框停在影像邊緣：影像外沒有像素可以比對。
    const QPointF p(std::clamp(event.scenePos.x(), bounds_.left(), bounds_.right()),
                    std::clamp(event.scenePos.y(), bounds_.top(), bounds_.bottom()));
    return QRectF(*anchor_, p).normalized();
}

void RubberBandTool::abort(IToolHost& host) {
    anchor_.reset();
    host.showRegionDraft(std::nullopt, mode_);
    if (ICanvasController* controller = host.canvasController()) controller->onRegionAborted();
}

bool RubberBandTool::handle(const ToolEvent& event, IToolHost& host) {
    using Type = ToolEvent::Type;
    switch (event.type) {
        case Type::Press:
            if (event.button != Qt::LeftButton) return false;
            if (!event.insideImage()) return true;  // 影像外的按下不開始（但也不讓畫布當成別的操作）
            anchor_ = event.scenePos;
            bounds_ = event.imageBounds;
            host.showRegionDraft(QRectF(*anchor_, QSizeF(0, 0)), mode_);
            return true;
        case Type::Move:
            if (!anchor_) return false;
            host.showRegionDraft(currentRect(event), mode_);
            return true;
        case Type::Release: {
            if (!anchor_ || event.button != Qt::LeftButton) return false;
            const QRectF rect = currentRect(event);
            anchor_.reset();
            host.showRegionDraft(rect, mode_);  // 框留著：搜尋中與預覽時都要讓使用者看到掃了哪裡
            if (ICanvasController* controller = host.canvasController()) controller->onRegionDrawn(rect, mode_);
            return true;
        }
        case Type::KeyPress:
            if (event.key == Qt::Key_Escape && anchor_) {
                abort(host);
                return true;
            }
            return false;
        case Type::DoubleClick:
            return false;
    }
    return false;
}

// ---- AddComponentTool ------------------------------------------------------------------------------------

bool AddComponentTool::handle(const ToolEvent& event, IToolHost& host) {
    if (event.type != ToolEvent::Type::Press || event.button != Qt::LeftButton) return false;
    if (!event.insideImage()) return true;
    if (ICanvasController* controller = host.canvasController()) controller->onAddRequested(event.scenePos);
    return true;
}

}  // namespace aoi::ui
