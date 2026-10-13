#include "presentation/canvas/ImageCanvas.h"

#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace aoi::ui {

ImageCanvas::ImageCanvas(QWidget* parent) : ImageCanvas(ThemeTokens::builtin(), parent) {}

ImageCanvas::ImageCanvas(const ThemeTokens& tokens, QWidget* parent)
    : QGraphicsView(parent),
      tokens_(tokens),
      style_(tokens),
      scene_(new QGraphicsScene(this)),
      tools_(ToolRegistry::makeDefault()) {
    imageItem_ = new ImageItem;
    layerItem_ = new RoiLayerItem(style_);
    draftItem_ = new RoiGraphicsItem(style_);
    imageItem_->setZValue(0);
    layerItem_->setZValue(1);
    draftItem_->setZValue(2);
    draftItem_->setVisible(false);
    scene_->addItem(imageItem_);
    scene_->addItem(layerItem_);
    scene_->addItem(draftItem_);
    setScene(scene_);

    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::NoAnchor);
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    setCacheMode(QGraphicsView::CacheNone);
    // 所有 Item 都自己設定用到的畫筆、筆刷與算繪提示，所以 View 不必在每個 Item 前後存取畫筆狀態。
    setOptimizationFlags(QGraphicsView::DontSavePainterState | QGraphicsView::DontAdjustForAntialiasing);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setMouseTracking(true);
    setBackgroundBrush(tokens.dark.surface.canvas);

    tools_.find(activeTool_)->activate(*this);
    updateCursor();
}

ImageCanvas::~ImageCanvas() = default;

// ---- ICanvasView ---------------------------------------------------------------------------------------

void ImageCanvas::showImage(const aoi::ImageView& image) { setImage(ImageItem::wrap(image)); }

void ImageCanvas::setImage(const QImage& image) {
    imageItem_->setImage(image);
    const QRectF rect = imageItem_->boundingRect();
    layerItem_->setBounds(rect);
    // 場景比影像大一圈：縮得很小或平移到邊緣時，影像可以離開視窗中心。
    scene_->setSceneRect(rect.adjusted(-rect.width(), -rect.height(), rect.width(), rect.height()));
    layerItem_->setSnapshot(nullptr);
    selected_.clear();
    layerItem_->setSelection({});
    draftItem_->setVisible(false);
    fit();
    emit imageChanged(!image.isNull());
}

void ImageCanvas::showComponents(SnapshotPtr snapshot) { layerItem_->setSnapshot(std::move(snapshot)); }

void ImageCanvas::showRegionDraft(std::optional<QRectF> rect, DraftMode mode) {
    if (!rect) {
        draftItem_->setVisible(false);
        return;
    }
    draftItem_->setViewScale(viewScale());
    draftItem_->setDraft(*rect, mode);
    draftItem_->setVisible(true);
}

void ImageCanvas::setSelection(const std::vector<int>& componentIds) {
    selected_ = componentIds;
    layerItem_->setSelection(componentIds);
}

void ImageCanvas::setInteraction(ToolId tool) {
    if (tool == activeTool_) return;
    ITool* next = tools_.find(tool);
    if (!next) return;
    if (ITool* previous = tools_.find(activeTool_)) previous->deactivate(*this);
    activeTool_ = tool;
    next->activate(*this);
    updateCursor();
    emit activeToolChanged(tool);
}

void ImageCanvas::setToolsEnabled(bool enabled) {
    if (enabled == toolsEnabled_) return;
    toolsEnabled_ = enabled;
    if (!enabled) {
        // 暫停時放棄進行中的操作（例如拖到一半的框），免得放開時才發現已經不能搜尋。
        if (ITool* tool = tools_.find(activeTool_)) tool->deactivate(*this);
        if (toolDragging_) {
            toolDragging_ = false;
            emit pointerDragChanged(false, activeTool_);
        }
    } else if (ITool* tool = tools_.find(activeTool_)) {
        tool->activate(*this);
    }
    updateCursor();
}

void ImageCanvas::setBusyCursor(bool busy) {
    busyCursor_ = busy;
    updateCursor();
}

// ---- 縮放、平移、適配 --------------------------------------------------------------------------------------

void ImageCanvas::zoomAt(const QPointF& viewPos, double factor) {
    if (!(factor > 0)) return;
    applyScale(viewScale() * factor, viewPos);
    fitMode_ = FitMode::None;
}

void ImageCanvas::applyScale(double newScale, const QPointF& anchorViewPos) {
    const double lo = tokens_.interaction.zoomMinPercent / 100.0;
    const double hi = tokens_.interaction.zoomMaxPercent / 100.0;
    newScale = std::clamp(newScale, lo, hi);
    const double current = viewScale();
    if (newScale == current) return;

    // 精確的游標下影像點：用浮點的視埠轉換，不用 mapToScene 的整數版本。
    const QPointF exactScene = viewportTransform().inverted().map(anchorViewPos);

    setTransform(QTransform::fromScale(newScale, newScale));
    // 縮放後那個影像點跑到哪裡了？把內容捲回去，使它回到游標下（捲動量是整數像素，誤差 ≤ 0.5 px）。
    const QPointF nowAt = viewportTransform().map(exactScene);
    const QPointF delta = nowAt - anchorViewPos;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + qRound(delta.x()));
    verticalScrollBar()->setValue(verticalScrollBar()->value() + qRound(delta.y()));
    notifyScale();
}

void ImageCanvas::fit() {
    fitMode_ = FitMode::Window;
    if (!hasImage()) return;
    const QRectF rect = imageRect();
    const double margin = tokens_.interaction.fitMarginPx;
    const double availW = std::max(1.0, viewport()->width() - 2 * margin);
    const double availH = std::max(1.0, viewport()->height() - 2 * margin);
    double s = std::min(availW / rect.width(), availH / rect.height());
    s = std::clamp(s, tokens_.interaction.zoomMinPercent / 100.0, tokens_.interaction.zoomMaxPercent / 100.0);
    setTransform(QTransform::fromScale(s, s));
    centerOn(rect.center());
    notifyScale();
}

void ImageCanvas::fitWidth() {
    // 改縮放之前，先記下視窗中央對到影像的哪一列（從「適配視窗」切過來時就是影像的中線）。
    // 之後視窗大小改變時沿用這一列、不重新讀：resize 的錨點是 NoAnchor，重新讀的話每次都會偏一點。
    fitWidthRow_ = viewportTransform().inverted().map(QPointF(viewport()->width() / 2.0, viewport()->height() / 2.0)).y();
    applyFitWidth();
}

void ImageCanvas::applyFitWidth() {
    fitMode_ = FitMode::Width;
    if (!hasImage()) return;
    const QRectF rect = imageRect();
    const double margin = tokens_.interaction.fitMarginPx;
    const double vw = viewport()->width(), vh = viewport()->height();
    double s = std::max(1.0, vw - 2 * margin) / rect.width();
    s = std::clamp(s, tokens_.interaction.zoomMinPercent / 100.0, tokens_.interaction.zoomMaxPercent / 100.0);
    // 中心的範圍：最高到「影像上緣剛好在上邊界」，最低到「影像下緣剛好在下邊界」，所以不會露出影像外的空白；
    // 影像比視窗矮（範圍是空的）就垂直置中，和「適配視窗」一樣。
    const double halfH = vh / (2.0 * s);
    const double highest = rect.top() - margin / s + halfH;
    const double lowest = rect.bottom() + margin / s - halfH;
    const double centerY = highest <= lowest ? std::clamp(fitWidthRow_, highest, lowest) : rect.center().y();
    setTransform(QTransform::fromScale(s, s));
    centerOn(QPointF(rect.center().x(), centerY));
    notifyScale();
}

void ImageCanvas::panBy(const QPoint& delta) {
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    fitMode_ = FitMode::None;
}

void ImageCanvas::notifyScale() {
    if (draftItem_->isVisible()) draftItem_->setViewScale(viewScale());
    emit zoomChanged(zoomPercent());
}

void ImageCanvas::applyTheme(const ThemeColors& colors) {
    setBackgroundBrush(colors.surface.canvas);
    style_.setAccent(colors.surface.accent);
    layerItem_->invalidateGlyphs();  // 狀態色不隨主題變（tokens），清掉只是保險
    viewport()->update();
}

// ---- 事件 ----------------------------------------------------------------------------------------------

ToolEvent ImageCanvas::makeToolEvent(ToolEvent::Type type, const QPointF& viewPos, Qt::MouseButton button,
                                     Qt::KeyboardModifiers modifiers, bool withHit) const {
    ToolEvent e;
    e.type = type;
    e.scenePos = viewportTransform().inverted().map(viewPos);
    e.button = button;
    e.modifiers = modifiers;
    e.viewScale = viewScale();
    e.imageBounds = hasImage() ? imageRect() : QRectF();
    if (withHit) {
        e.hitComponent = layerItem_->componentAt(e.scenePos);
        e.selected = selected_;
    }
    return e;
}

bool ImageCanvas::dispatch(const ToolEvent& event) {
    ITool* tool = tools_.find(activeTool_);
    return tool && tool->handle(event, *this);
}

void ImageCanvas::beginPan(const QPoint& pos) {
    panning_ = true;
    lastPanPos_ = pos;
    updateCursor();
}

void ImageCanvas::endPan() {
    panning_ = false;
    updateCursor();
}

void ImageCanvas::updateCursor() {
    if (busyCursor_ && !panning_) {
        viewport()->setCursor(Qt::BusyCursor);
    } else if (panning_) {
        viewport()->setCursor(Qt::ClosedHandCursor);
    } else if (spaceDown_) {
        viewport()->setCursor(Qt::OpenHandCursor);
    } else if (ITool* tool = tools_.find(activeTool_)) {
        viewport()->setCursor(tool->cursor());
    }
}

void ImageCanvas::wheelEvent(QWheelEvent* event) {
    const double notches = event->angleDelta().y() / 120.0;
    if (notches == 0) {
        event->ignore();
        return;
    }
    zoomAt(event->position(), std::pow(tokens_.interaction.zoomStepPerNotch, notches));
    event->accept();
}

void ImageCanvas::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);  // Space＋拖曳與 Esc 需要畫布有焦點
    const bool panGesture = event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && spaceDown_);
    if (panGesture) {
        beginPan(event->pos());
        event->accept();
        return;
    }
    if (!hasImage() || !toolsEnabled_) {
        event->ignore();
        return;
    }
    const ToolEvent te =
        makeToolEvent(ToolEvent::Type::Press, event->position(), event->button(), event->modifiers(), true);
    if (dispatch(te)) {
        if (event->button() == Qt::LeftButton) {
            toolDragging_ = true;
            emit pointerDragChanged(true, activeTool_);
        }
        event->accept();
        return;
    }
    event->ignore();
}

void ImageCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (panning_) {
        const QPoint pos = event->pos();
        panBy(pos - lastPanPos_);
        lastPanPos_ = pos;
        event->accept();
        return;
    }
    if (hasImage()) {
        const QPointF scenePos = viewportTransform().inverted().map(event->position());
        emit cursorMoved(scenePos, imageRect().contains(scenePos));
        if (toolDragging_) {
            dispatch(makeToolEvent(ToolEvent::Type::Move, event->position(), Qt::NoButton, event->modifiers(), false));
        }
    }
    event->accept();
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (panning_ && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        endPan();
        event->accept();
        return;
    }
    if (toolDragging_ && event->button() == Qt::LeftButton) {
        toolDragging_ = false;
        dispatch(makeToolEvent(ToolEvent::Type::Release, event->position(), event->button(), event->modifiers(), false));
        emit pointerDragChanged(false, activeTool_);
        event->accept();
        return;
    }
    event->ignore();
}

void ImageCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
    if (!hasImage() || !toolsEnabled_) {
        event->ignore();
        return;
    }
    const ToolEvent te =
        makeToolEvent(ToolEvent::Type::DoubleClick, event->position(), event->button(), event->modifiers(), true);
    event->setAccepted(dispatch(te));
}

void ImageCanvas::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space) {
        if (!event->isAutoRepeat() && !spaceDown_) {
            spaceDown_ = true;
            updateCursor();
        }
        event->accept();
        return;
    }
    if (!toolsEnabled_) {
        // 畫布暫停：只有 Esc 還是有意義（取消搜尋、關閉訊息），交給 Presenter。
        if (keyboard_ && keyboard_->onKeyPressed(event->key(), event->modifiers())) {
            event->accept();
            return;
        }
        QGraphicsView::keyPressEvent(event);
        return;
    }
    ToolEvent te;
    te.type = ToolEvent::Type::KeyPress;
    te.key = event->key();
    te.modifiers = event->modifiers();
    te.viewScale = viewScale();
    te.imageBounds = hasImage() ? imageRect() : QRectF();
    if (dispatch(te)) {
        event->accept();
        return;
    }
    // 工具沒用到的鍵：先問 Presenter（Enter 接受、Esc 退回一層、Del 刪除、方向鍵微調）。
    if (keyboard_ && keyboard_->onKeyPressed(event->key(), event->modifiers())) {
        event->accept();
        return;
    }
    // 沒有 Presenter（S02 的獨立畫布）：Esc 清除選取，Del／Backspace 刪除所選。
    if (!keyboard_ && event->key() == Qt::Key_Escape && !selected_.empty() && selectionController_) {
        selectionController_->select({});
        event->accept();
        return;
    }
    if (!keyboard_ && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && !selected_.empty() &&
        selectionController_) {
        selectionController_->deleteSelected();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void ImageCanvas::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        spaceDown_ = false;
        updateCursor();
        event->accept();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void ImageCanvas::focusOutEvent(QFocusEvent* event) {
    // 失去焦點時收不到放開事件，要自己重設，否則游標會卡在「手」。
    spaceDown_ = false;
    if (panning_) endPan();
    else updateCursor();
    QGraphicsView::focusOutEvent(event);
}

void ImageCanvas::leaveEvent(QEvent* event) {
    emit cursorLeft();
    QGraphicsView::leaveEvent(event);
}

void ImageCanvas::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    if (!hasImage()) return;
    if (fitMode_ == FitMode::Window) fit();
    else if (fitMode_ == FitMode::Width) applyFitWidth();
}

void ImageCanvas::paintEvent(QPaintEvent* event) {
    QElapsedTimer timer;
    timer.start();
    QGraphicsView::paintEvent(event);
    lastPaintMs_ = static_cast<double>(timer.nsecsElapsed()) / 1e6;
    ++paintCount_;
    emit framePainted(lastPaintMs_);
}

}  // namespace aoi::ui
