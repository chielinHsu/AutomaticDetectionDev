#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>

#include "application/ports/IImageSource.h"
#include "presentation/canvas/ICanvasController.h"
#include "presentation/canvas/ImageViewMetaType.h"
#include "presentation/presenters/ControllerInterfaces.h"
#include "presentation/presenters/ViewInterfaces.h"
#include "presentation/tools/ToolId.h"

class QAction;
class QActionGroup;
class QComboBox;
class QDockWidget;
class QLabel;
class QLineEdit;
class QMenu;
class QSlider;
class QToolBar;

namespace aoi::ui {

class EmptyCard;
class ImageCanvas;
class MessageBar;
class ObjectsPanel;
class ParamsPanel;
class ThemeManager;

// 主視窗骨架（docs/ux/ux-spec.md 第 4 節）：上方工具列、左側工具、中央畫布與訊息列、
// 右側三個 dock（樣本庫：S06 之前是空殼；物件樹；比對參數）、狀態列。
// 這一層只負責組裝與事件接線，不含業務規則。S04 起它同時是 Presenter 的三個 View：
// 狀態列與提示（IStatusView）、訊息卡（IMessageView）、復原／重做按鈕（IHistoryView）；Presenter 只看到這些窄介面。
// 使用者的操作經 setControllers 交進來的 Controller 介面送出，這個類別不認識 Presenter。
class MainWindow : public QMainWindow, public IStatusView, public IMessageView, public IHistoryView {
    Q_OBJECT
public:
    explicit MainWindow(ThemeManager& theme, QWidget* parent = nullptr);
    ~MainWindow() override;

    // View 對外送出操作的出口（都不歸視窗所有，空指標 ＝ 沒有人聽）。
    struct Controllers {
        ICanvasController* canvas = nullptr;
        ISelectionController* selection = nullptr;
        IKeyboardController* keyboard = nullptr;
        IProposalController* proposal = nullptr;
        IMessageController* message = nullptr;
        IHistoryController* history = nullptr;
        IParamsController* params = nullptr;
    };
    void setControllers(const Controllers& controllers);

    ImageCanvas* canvas() const { return canvas_; }
    ObjectsPanel* objectsPanel() const { return objectsPanel_; }
    ParamsPanel* paramsPanel() const { return paramsPanel_; }
    MessageBar* messageBar() const { return messageBar_; }
    EmptyCard* emptyCard() const { return emptyCard_; }
    QAction* toolAction(ToolId id) const;
    QAction* fitAction() const { return fitAction_; }
    QAction* fitWidthAction() const { return fitWidthAction_; }
    QAction* openAction() const { return openAction_; }
    QAction* undoAction() const { return undoAction_; }
    QAction* redoAction() const { return redoAction_; }
    QComboBox* backendCombo() const { return backendCombo_; }
    QDockWidget* libraryDock() const { return libraryDock_; }
    QDockWidget* objectsDock() const { return objectsDock_; }
    QDockWidget* paramsDock() const { return paramsDock_; }
    QLabel* zoomLabel() const { return zoomLabel_; }
    QLabel* cursorLabel() const { return cursorLabel_; }
    QLabel* summaryLabel() const { return summaryLabel_; }

    // 影像從哪裡來（S06：IImageSource，由 app 組裝；不歸視窗所有，要活得比視窗久）。沒有設定時 openImage 一律失敗。
    void setImageSource(aoi::IImageSource* source) { imageSource_ = source; }
    // 經 IImageSource 解碼影像檔並顯示（零複製：畫布直接畫 view 的像素）。失敗時舊影像保留，發出 imageOpenFailed
    // （錯誤卡片由 Presenter 顯示）。
    bool openImage(const QString& path);
    // 已有影像時（示範場景、測試）。name 只用在視窗標題。成功後發出 imageOpened。
    void setImage(const aoi::ImageView& image, const QString& name);
    // 工具列的工具換成 id（工具、按鈕、畫布一起換）。
    void selectTool(ToolId id);
    // 上方 back end 選單：顯示目前用的與其他已註冊的。S04 不能在執行中切換（只有一個可用的 back end，S07 才有第二個），所以是停用的。
    void setBackends(const QStringList& names, int current);

    // ---- IStatusView ----
    void showSummary(const QString& text) override;
    void showBusy(BusyLevel level) override;
    void showProgress(double fraction) override;
    void showToast(const QString& text) override;
    void setHint(const QString& text) override;

    // ---- IMessageView ----
    void showCard(const MessageCard& card) override;
    void hideCard() override;

    // ---- IHistoryView ----
    void setUndo(bool enabled, const QString& label) override;
    void setRedo(bool enabled, const QString& label) override;

signals:
    void imageOpened(const aoi::ImageView& image, const QString& name);
    void imageOpenFailed(const QString& name, const QString& reason);

protected:
    void changeEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildActions();
    void buildMenus();
    void buildToolbars();
    void buildDocks();
    void buildStatusBar();
    void retranslateUi();
    void refreshState();
    void updateSummaryLabel();
    void updateHistoryTips();
    void centerEmptyCard();
    void promptOpen();

    ThemeManager& theme_;
    aoi::IImageSource* imageSource_ = nullptr;
    ImageCanvas* canvas_ = nullptr;
    MessageBar* messageBar_ = nullptr;
    EmptyCard* emptyCard_ = nullptr;
    ObjectsPanel* objectsPanel_ = nullptr;
    ParamsPanel* paramsPanel_ = nullptr;
    Controllers controllers_;

    QActionGroup* toolGroup_ = nullptr;
    QAction* toolSelect_ = nullptr;
    QAction* toolSample_ = nullptr;
    QAction* toolRegion_ = nullptr;
    QAction* toolAdd_ = nullptr;
    QAction* openAction_ = nullptr;
    QAction* saveAction_ = nullptr;
    QAction* exportAction_ = nullptr;
    QAction* quitAction_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;
    QAction* fitAction_ = nullptr;
    QAction* fitWidthAction_ = nullptr;

    QMenu* fileMenu_ = nullptr;
    QMenu* editMenu_ = nullptr;
    QMenu* viewMenu_ = nullptr;
    QMenu* themeMenu_ = nullptr;
    QMenu* fontMenu_ = nullptr;
    QMenu* languageMenu_ = nullptr;
    QMenu* panelsMenu_ = nullptr;
    QAction* themeLight_ = nullptr;
    QAction* themeDark_ = nullptr;
    QAction* themeSystem_ = nullptr;
    QAction* langZh_ = nullptr;
    QAction* langEn_ = nullptr;

    QToolBar* topBar_ = nullptr;
    QToolBar* toolRail_ = nullptr;
    QLabel* backendLabel_ = nullptr;
    QComboBox* backendCombo_ = nullptr;

    QDockWidget* libraryDock_ = nullptr;
    QDockWidget* objectsDock_ = nullptr;
    QDockWidget* paramsDock_ = nullptr;
    QLabel* libraryEmpty_ = nullptr;
    QLineEdit* libraryFilter_ = nullptr;

    QLabel* summaryLabel_ = nullptr;
    QLabel* cursorLabel_ = nullptr;
    QLabel* zoomLabel_ = nullptr;

    QString imageName_;
    QString summaryText_;
    BusyLevel busyLevel_ = BusyLevel::None;
    double progress_ = 0.0;
    QString undoLabel_;
    QString redoLabel_;
};

}  // namespace aoi::ui

