#include "presentation/tools/Tools.h"

#include <algorithm>
#include <cmath>

namespace aoi::ui {

// ---- SelectTool ------------------------------------------------------------------------------------------

void SelectTool::deactivate(IToolHost& host) {
    if (drag_) cancelDrag(host, true);
}

// 放棄拖曳。report ＝ true 時回報位移 0，讓 Presenter 把拖曳中蓋掉的畫面（例如預覽的區域框）還原。
void SelectTool::cancelDrag(IToolHost& host, bool report) {
    if (!drag_) return;
    const Drag drag = *drag_;
    drag_.reset();
    host.showRegionDraft(std::nullopt, DraftMode::Region);
    if (report && drag.moving) {
        if (ICanvasController* controller = host.canvasController()) controller->onComponentsMoved(drag.ids, QPointF(0, 0));
    }
}

bool SelectTool::startDrag(const ToolEvent& event, IToolHost& host, const std::vector<int>& ids) {
    Drag drag;
    drag.ids = ids;
    drag.start = event.scenePos;
    drag.outline = host.outlineOf(ids);
    if (!drag.outline.isNull()) drag_ = std::move(drag);  // 找不到外框（不該發生）：當作一次點擊
    return true;
}

bool SelectTool::handle(const ToolEvent& event, IToolHost& host) {
    using Type = ToolEvent::Type;
    if (event.type == Type::KeyPress) {
        if (event.key == Qt::Key_Escape && drag_) {
            cancelDrag(host, true);
            return true;
        }
        return false;
    }
    if (event.type == Type::Move) {
        if (!drag_) return false;
        const QPointF delta = event.scenePos - drag_->start;
        if (!drag_->moving) {
            if (std::hypot(delta.x(), delta.y()) * event.viewScale < kDragStartScreenPx) return true;
            drag_->moving = true;
        }
        host.showRegionDraft(drag_->outline.translated(delta), DraftMode::Region);
        return true;
    }
    if (event.type == Type::Release) {
        if (!drag_) return false;
        const Drag drag = *drag_;
        const QPointF delta = event.scenePos - drag.start;
        drag_.reset();
        if (!drag.moving) return true;  // 只是點擊：選取在按下時已經處理了
        host.showRegionDraft(std::nullopt, DraftMode::Region);
        if (ICanvasController* controller = host.canvasController()) controller->onComponentsMoved(drag.ids, delta);
        return true;
    }
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
        std::vector<int> dragIds;
        if (event.modifiers & Qt::ShiftModifier) {
            next = event.selected;
            const auto it = std::find(next.begin(), next.end(), hit);
            if (it == next.end()) next.push_back(hit);
            else next.erase(it);
        } else if (std::find(event.selected.begin(), event.selected.end(), hit) != event.selected.end()) {
            return startDrag(event, host, event.selected);  // 按在已選取的元件上：整個選取一起走，選取不變
        } else {
            next = {hit};
            dragIds = next;
        }
        selection->select(next);
        if (!dragIds.empty()) startDrag(event, host, dragIds);
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
