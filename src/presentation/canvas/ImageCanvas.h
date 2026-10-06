#pragma once

#include <QGraphicsView>
#include <QImage>
#include <optional>
#include <vector>

#include "presentation/canvas/ICanvasController.h"
#include "presentation/canvas/ICanvasView.h"
#include "presentation/canvas/ImageItem.h"
#include "presentation/canvas/ImageViewMetaType.h"
#include "presentation/canvas/OverlayStyle.h"
#include "presentation/canvas/RoiGraphicsItem.h"
#include "presentation/canvas/RoiLayerItem.h"
#include "presentation/presenters/ViewInterfaces.h"
#include "presentation/theme/ThemeTokens.h"
#include "presentation/tools/ITool.h"
#include "presentation/tools/ToolRegistry.h"

class QGraphicsScene;

namespace aoi::ui {

// 影像畫布：游標中心縮放、平移、適配視窗、適配寬度、疊圖，以及把指標事件交給目前的工具。
//
// 場景座標 ＝ 影像像素（原點在影像左上角，x 向右、y 向下）。縮放百分比 ＝ 螢幕（裝置無關）像素／影像像素 × 100。
// 縮放、平移、適配是畫布自己的檢視狀態，不經過 Presenter，也不進復原堆疊。
class ImageCanvas : public QGraphicsView, public ICanvasView, public IToolHost, public ICanvasInputGate {
    Q_OBJECT
public:
    // 目前的檢視是不是「適配」出來的：是的話，視窗大小改變時照同一種方式重新適配；使用者縮放或平移之後就是 None。
    enum class FitMode { None, Window, Width };

    explicit ImageCanvas(const ThemeTokens& tokens, QWidget* parent = nullptr);
    ~ImageCanvas() override;

    // ---- ICanvasView ----
    void showImage(const aoi::ImageView& image) override;
    void showComponents(SnapshotPtr snapshot) override;
    void showRegionDraft(std::optional<QRectF> rect, DraftMode mode) override;  // 也實作 IToolHost
    void setSelection(const std::vector<int>& componentIds) override;
    void setInteraction(ToolId tool) override;

    // ---- ICanvasInputGate ----
    // 搜尋中、錯誤時畫布暫停：工具不收指標與按鍵（縮放、平移照常）。忙碌時游標變 progress。
    void setToolsEnabled(bool enabled) override;
    void setBusyCursor(bool busy) override;
    bool toolsEnabled() const { return toolsEnabled_; }
    bool busyCursor() const { return busyCursor_; }

    // 影像檔解碼之後的路徑（QImage 隱式共享，不複製像素）。空影像＝清除。
    void setImage(const QImage& image);

    // ---- IToolHost ----
    ICanvasController* canvasController() override { return controller_; }
    ISelectionController* selectionController() override { return selectionController_; }
    QRectF outlineOf(const std::vector<int>& componentIds) override { return layerItem_->outlineOf(componentIds); }

    // 兩個 controller 都不歸畫布所有。
    void setController(ICanvasController* controller) { controller_ = controller; }
    void setSelectionController(ISelectionController* controller) { selectionController_ = controller; }
    // 工具沒用到的按鍵（Enter、Esc、Del、方向鍵…）轉給它；沒有設定時，Esc 清除選取、Del 刪除所選（S02 的行為）。
    void setKeyboardController(IKeyboardController* controller) { keyboard_ = controller; }

    ToolRegistry& tools() { return tools_; }
    ToolId activeTool() const { return activeTool_; }

    bool hasImage() const { return !imageItem_->image().isNull(); }
    QRectF imageRect() const { return imageItem_->boundingRect(); }
    const std::vector<int>& selection() const { return selected_; }

    double viewScale() const { return transform().m11(); }
    double zoomPercent() const { return viewScale() * 100.0; }
    // 以 viewPos（視埠座標）為中心縮放，factor > 1 放大。結果限制在 5%–3200%。游標下的影像點不會移位。
    void zoomAt(const QPointF& viewPos, double factor);
    // 適配視窗：整張影像放得下，四周留 fitMarginPx。載入影像時用這個。
    void fit();
    // 適配寬度：影像寬度撐滿視窗（左右留 fitMarginPx），超出視窗的上下部分靠平移看。垂直方向保留目前
    // 視窗中央那一列，但上緣、下緣不會離開邊界；影像比視窗矮時垂直置中。縮放同樣限制在 5%–3200%。
    void fitWidth();
    FitMode fitMode() const { return fitMode_; }
    bool isFitted() const { return fitMode_ == FitMode::Window; }
    void panBy(const QPoint& deltaViewPx);  // 內容跟著 delta 移動（拖曳平移用）

    void applyTheme(const ThemeColors& colors);

    // 供測試與效能量測。
    ImageItem* imageItem() { return imageItem_; }
    RoiLayerItem* layerItem() { return layerItem_; }
    RoiGraphicsItem* draftItem() { return draftItem_; }
    QGraphicsScene* canvasScene() { return scene_; }
    int paintCount() const { return paintCount_; }
    double lastPaintMs() const { return lastPaintMs_; }
    OverlayStyle& overlayStyle() { return style_; }
    const OverlayStyle& overlayStyle() const { return style_; }

signals:
    void zoomChanged(double percent);
    void cursorMoved(const QPointF& imagePos, bool insideImage);
    void cursorLeft();
    void imageChanged(bool hasImage);
    void framePainted(double ms);
    // 左鍵在畫布上按下並被工具接手（true），或放開（false）。訊息列用它換「放開滑鼠開始搜尋」的提示。
    void pointerDragChanged(bool active, ToolId tool);
    // 目前的工具換了（程式換的也算，例如〔重畫樣本〕）。工具列的按鈕靠它同步。
    void activeToolChanged(ToolId tool);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    ToolEvent makeToolEvent(ToolEvent::Type type, const QPointF& viewPos, Qt::MouseButton button,
                            Qt::KeyboardModifiers modifiers, bool withHit) const;
    bool dispatch(const ToolEvent& event);
    void beginPan(const QPoint& pos);
    void endPan();
    void updateCursor();
    void applyScale(double newScale, const QPointF& anchorViewPos);
    void applyFitWidth();  // 用 fitWidthRow_ 當垂直的錨點
    void notifyScale();

    const ThemeTokens& tokens_;
    OverlayStyle style_;
    QGraphicsScene* scene_;
    ImageItem* imageItem_;
    RoiLayerItem* layerItem_;
    RoiGraphicsItem* draftItem_;

    ToolRegistry tools_;
    ToolId activeTool_ = ToolId::Select;
    ICanvasController* controller_ = nullptr;
    ISelectionController* selectionController_ = nullptr;
    IKeyboardController* keyboard_ = nullptr;
    std::vector<int> selected_;
    bool toolsEnabled_ = true;
    bool busyCursor_ = false;

    FitMode fitMode_ = FitMode::Window;
    double fitWidthRow_ = 0;  // 適配寬度時保持在視窗中央的影像列（場景座標 y）
    bool spaceDown_ = false;
    bool panning_ = false;
    bool toolDragging_ = false;
    QPoint lastPanPos_;

    int paintCount_ = 0;
    double lastPaintMs_ = 0;
};

}  // namespace aoi::ui
