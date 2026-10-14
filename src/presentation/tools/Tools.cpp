#include "presentation/tools/Tools.h"

#include <algorithm>
#include <cmath>

#include "presentation/canvas/RoiGraphicsItem.h"

namespace aoi::ui {

// ---- SelectTool ------------------------------------------------------------------------------------------

void SelectTool::deactivate(IToolHost& host) {
    if (drag_) cancelDrag(host, true);
    if (resize_ || marquee_) {
        resize_.reset();
        marquee_.reset();
        host.showRegionDraft(std::nullopt, DraftMode::Region);
    }
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

// 拖著某個把手時，框變成什麼樣：角的把手改兩邊，邊的把手只改那一邊；對面的邊不動。翻過去時正規化（寬高不會是負的）。
QRectF SelectTool::resizedRect(const Resize& resize, const QPointF& scenePos) {
    using Handle = RoiGraphicsItem::Handle;
    QRectF r = resize.rect;
    const QPointF delta = scenePos - resize.start;
    const auto handle = static_cast<Handle>(resize.handle);
    const bool left = handle == Handle::TopLeft || handle == Handle::Left || handle == Handle::BottomLeft;
    const bool right = handle == Handle::TopRight || handle == Handle::Right || handle == Handle::BottomRight;
    const bool top = handle == Handle::TopLeft || handle == Handle::Top || handle == Handle::TopRight;
    const bool bottom = handle == Handle::BottomLeft || handle == Handle::Bottom || handle == Handle::BottomRight;
    if (left) r.setLeft(r.left() + delta.x());
    if (right) r.setRight(r.right() + delta.x());
    if (top) r.setTop(r.top() + delta.y());
    if (bottom) r.setBottom(r.bottom() + delta.y());
    r = r.normalized();
    if (r.width() < kMinResizeSidePx) r.setWidth(kMinResizeSidePx);
    if (r.height() < kMinResizeSidePx) r.setHeight(kMinResizeSidePx);
    return r;
}

bool SelectTool::handle(const ToolEvent& event, IToolHost& host) {
    using Type = ToolEvent::Type;
    if (event.type == Type::KeyPress) {
        if (event.key == Qt::Key_Escape && (drag_ || resize_ || marquee_)) {
            cancelDrag(host, true);
            resize_.reset();
            marquee_.reset();
            host.showRegionDraft(std::nullopt, DraftMode::Region);
            return true;
        }
        return false;
    }
    if (event.type == Type::Move) {
        if (resize_) {
            const QPointF delta = event.scenePos - resize_->start;
            if (!resize_->moving) {
                if (std::hypot(delta.x(), delta.y()) * event.viewScale < kDragStartScreenPx) return true;
                resize_->moving = true;
            }
            host.showRegionDraft(resizedRect(*resize_, event.scenePos), DraftMode::Sample);
            return true;
        }
        if (marquee_) {
            const QPointF delta = event.scenePos - marquee_->start;
            if (!marquee_->moving) {
                if (std::hypot(delta.x(), delta.y()) * event.viewScale < kDragStartScreenPx) return true;
                marquee_->moving = true;
            }
            host.showRegionDraft(QRectF(marquee_->start, event.scenePos).normalized(), DraftMode::Region);
            return true;
        }
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
        if (resize_) {
            const Resize resize = *resize_;
            resize_.reset();
            if (!resize.moving) return true;  // 只是點了一下把手
            host.showRegionDraft(std::nullopt, DraftMode::Sample);
            if (ICanvasController* controller = host.canvasController())
                controller->onComponentResized(resize.id, resizedRect(resize, event.scenePos));
            return true;
        }
        if (marquee_) {
            const Marquee marquee = *marquee_;
            marquee_.reset();
            if (!marquee.moving) {
                // 沒拖動：和點空白處一樣（清除選取；Shift 不動）。
                ISelectionController* selection = host.selectionController();
                if (selection && !marquee.additive && !marquee.selectedAtPress.empty()) selection->select({});
                return true;
            }
            host.showRegionDraft(std::nullopt, DraftMode::Region);
            const QRectF rect = QRectF(marquee.start, event.scenePos).normalized();
            std::vector<int> ids = host.componentsIn(rect);
            if (marquee.additive) {
                for (int id : marquee.selectedAtPress) {
                    if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
                }
            }
            if (ISelectionController* selection = host.selectionController()) selection->select(ids);
            return true;
        }
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
        // 把手（單一選取的元件）：改大小。
        if (event.hitHandle != 0 && event.selected.size() == 1 && !event.hitHandleRect.isNull()) {
            Resize resize;
            resize.id = event.selected.front();
            resize.handle = event.hitHandle;
            resize.start = event.scenePos;
            resize.rect = event.hitHandleRect;
            resize_ = resize;
            return true;
        }
        const int hit = event.hitComponent;
        if (hit < 0) {
            // 空白處：框選（放開時沒拖動就是清除選取；Shift＋框選＝加入、Shift＋點空白處不動，避免手滑丟掉多選）。
            Marquee marquee;
            marquee.start = event.scenePos;
            marquee.selectedAtPress = event.selected;
            marquee.additive = event.modifiers & Qt::ShiftModifier;
            marquee_ = marquee;
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
            host.showRegionDraft(rect, mode_);  // 框留著：區域掃描的搜尋中與預覽時都要讓使用者看到掃了哪裡
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
