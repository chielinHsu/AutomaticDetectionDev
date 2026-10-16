#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <memory>

#include "application/ports/IImageSource.h"
#include "presentation/canvas/ICanvasController.h"
#include "presentation/canvas/ImageViewMetaType.h"
#include "presentation/compare/CompareInterfaces.h"
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

class CompareWindow;
class EmptyCard;
class GuidancePanel;
class ImageCanvas;
class MessageBar;
class ObjectsPanel;
class ParamsPanel;
class SamplePanel;
class ThemeManager;

namespace Ui {
class MainWindow;
}

// 主視窗骨架（docs/ux/ux-spec.md 第 4 節；版面在 MainWindow.ui，用 Qt Designer 編輯）：上方工具列、左側工具、中央畫布與訊息列、
// 右側三個 dock（樣本庫：2026-10-14 起列出板子上的樣本與縮圖，SamplePanel；物件樹與選取資訊表單，ObjectsPanel；比對參數）、狀態列。
// 這一層只負責組裝與事件接線，不含業務規則。S04 起它同時是 Presenter 的幾個 View：
// 狀態列與提示（IStatusView）、訊息卡（IMessageView）、復原／重做按鈕（IHistoryView）、back end 的選單、標示與提醒卡片
// （IBackendView）、A/B 比較的入口（ICompareEntryView，S09）；Presenter 只看到這些窄介面。
// S09 起它也擁有 A/B 比較的視窗（CompareWindow，另一個頂層視窗；主視窗關掉時一起關）。
// 使用者的操作經 setControllers 交進來的 Controller 介面送出，這個類別不認識 Presenter。
class MainWindow : public QMainWindow,
                   public IStatusView,
                   public IMessageView,
                   public IHistoryView,
                   public IBackendView,
                   public ICompareEntryView {
    Q_OBJECT
public:
    explicit MainWindow(ThemeManager& theme, QWidget* parent = nullptr);
    ~MainWindow() override;

    // View 對外送出操作的出口（都不歸視窗所有，空指標 ＝ 沒有人聽）。
    struct Controllers {
        ICanvasController* canvas = nullptr;
        ISelectionController* selection = nullptr;
        ISelectionEditController* selectionEdit = nullptr;  // 選取資訊表單（2026-10-14）
        IMatchRegionEditController* matchEdit = nullptr;    // 選取資訊表單的比對區（ADR-028）
        IKeyboardController* keyboard = nullptr;
        IProposalController* proposal = nullptr;
        IMessageController* message = nullptr;
        IHistoryController* history = nullptr;
        IParamsController* params = nullptr;
        IBackendController* backend = nullptr;
        ICompareController* compare = nullptr;  // 主視窗的〔A/B 比較…〕與比較視窗（S09）
    };
    void setControllers(const Controllers& controllers);

    ImageCanvas* canvas() const { return canvas_; }
    ObjectsPanel* objectsPanel() const { return objectsPanel_; }
    ParamsPanel* paramsPanel() const { return paramsPanel_; }
    SamplePanel* samplePanel() const { return samplePanel_; }  // 樣本庫 dock：板子上的樣本（2026-10-14）
    MessageBar* messageBar() const { return messageBar_; }
    EmptyCard* emptyCard() const { return emptyCard_; }
    CompareWindow* compareWindow() const { return compareWindow_; }
    QAction* toolAction(ToolId id) const;
    QAction* searchAction() const { return searchAction_; }  // 〔搜尋〕（F5；ADR-025）
    QAction* selectAllAction() const { return selectAllAction_; }
    QAction* selectSameTypeAction() const { return selectSameTypeAction_; }
    QAction* fitAction() const { return fitAction_; }
    QAction* fitWidthAction() const { return fitWidthAction_; }
    QAction* openAction() const { return openAction_; }
    QAction* compareAction() const { return compareAction_; }
    QAction* undoAction() const { return undoAction_; }
    QAction* redoAction() const { return redoAction_; }
    QComboBox* backendCombo() const { return backendCombo_; }
    QDockWidget* libraryDock() const { return libraryDock_; }
    QDockWidget* objectsDock() const { return objectsDock_; }
    QDockWidget* paramsDock() const { return paramsDock_; }
    // 操作步驟（2026-10-15，ADR-027）：左側的 dock 與它的面板（IGuidanceView）；〔說明 › 操作步驟〕（F1）把它打開。
    QDockWidget* guidanceDock() const { return guidanceDock_; }
    GuidancePanel* guidancePanel() const { return guidancePanel_; }
    QAction* guideAction() const { return guideAction_; }
    QLabel* zoomLabel() const { return zoomLabel_; }
    QLabel* cursorLabel() const { return cursorLabel_; }
    QLabel* summaryLabel() const { return summaryLabel_; }
    QLabel* backendStatusLabel() const { return backendStatus_; }

    // 影像從哪裡來（S06：IImageSource，由 app 組裝；不歸視窗所有，要活得比視窗久）。沒有設定時 openImage 一律失敗。
    void setImageSource(aoi::IImageSource* source) { imageSource_ = source; }
    // 經 IImageSource 解碼影像檔並顯示（零複製：畫布直接畫 view 的像素）。失敗時舊影像保留，發出 imageOpenFailed
    // （錯誤卡片由 Presenter 顯示）。
    bool openImage(const QString& path);
    // 已有影像時（示範場景、測試）。name 只用在視窗標題。成功後發出 imageOpened。
    void setImage(const aoi::ImageView& image, const QString& name);
    // 工具列的工具換成 id（工具、按鈕、畫布一起換）。
    void selectTool(ToolId id);
    // ---- IBackendView（BackendPresenter）----
    // 上方 back end 選單：目前用的與其他已註冊的；選了別的就在執行中切換（ADR-021，交給 IBackendController::selectBackend）。
    // 切換中（switching）整個選單停用。S08：不可用的項目停用、名稱後加「✕」，提示（tooltip）說原因；
    // 可用但有提醒的（例如 HALCON 的授權）也寫在提示裡。
    using BackendChoice = aoi::ui::BackendChoice;
    void setBackends(const QList<BackendChoice>& choices, int current, bool switching) override;
    // 狀態列最左邊的 back end 標示（「HALCON · runtime 授權」），滑鼠移上去看 detail。
    void setBackendStatus(const QString& text, const QString& detail) override;
    // 訊息列的提醒卡片（MessageBar::showNotice）：比搜尋的卡片低一級，按鈕交給 IBackendController::applyNoticeAction。
    void showNotice(const MessageCard& card) override;
    void hideNotice() override;

    // ---- ICompareEntryView（ComparePresenter）----
    // 檢視選單與工具列的〔A/B 比較…〕：有樣本、而且有兩個能用的 back end 時才能按；tip 說明為什麼（不能按時說怎麼做）。
    void setCompareAvailable(bool enabled, const QString& tip) override;

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
    void repeatabilityRequested();
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
    void refreshBackends();

    ThemeManager& theme_;
    std::unique_ptr<Ui::MainWindow> ui_;  // MainWindow.ui（Qt Designer）產生的外框；下面的指標都指到它建立的物件
    aoi::IImageSource* imageSource_ = nullptr;
    ImageCanvas* canvas_ = nullptr;
    MessageBar* messageBar_ = nullptr;
    EmptyCard* emptyCard_ = nullptr;
    CompareWindow* compareWindow_ = nullptr;
    ObjectsPanel* objectsPanel_ = nullptr;
    ParamsPanel* paramsPanel_ = nullptr;
    SamplePanel* samplePanel_ = nullptr;
    GuidancePanel* guidancePanel_ = nullptr;
    Controllers controllers_;

    QActionGroup* toolGroup_ = nullptr;
    QAction* toolSelect_ = nullptr;
    QAction* toolSample_ = nullptr;
    QAction* toolRegion_ = nullptr;
    QAction* toolAdd_ = nullptr;
    QAction* toolBox_ = nullptr;
    QAction* toolMatch_ = nullptr;
    QAction* searchAction_ = nullptr;
    QAction* selectAllAction_ = nullptr;
    QAction* selectSameTypeAction_ = nullptr;
    QAction* openAction_ = nullptr;
    QAction* saveAction_ = nullptr;
    QAction* exportAction_ = nullptr;
    QAction* quitAction_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;
    QAction* fitAction_ = nullptr;
    QAction* fitWidthAction_ = nullptr;
    QAction* compareAction_ = nullptr;
    QAction* repeatabilityAction_ = nullptr;
    QAction* guideAction_ = nullptr;

    QMenu* fileMenu_ = nullptr;
    QMenu* editMenu_ = nullptr;
    QMenu* viewMenu_ = nullptr;
    QMenu* themeMenu_ = nullptr;
    QMenu* fontMenu_ = nullptr;
    QMenu* languageMenu_ = nullptr;
    QMenu* panelsMenu_ = nullptr;
    QMenu* helpMenu_ = nullptr;
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
    QDockWidget* guidanceDock_ = nullptr;

    QLabel* backendStatus_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
    QLabel* cursorLabel_ = nullptr;
    QLabel* zoomLabel_ = nullptr;

    QString imageName_;
    QString summaryText_;
    QList<BackendChoice> backends_;
    int currentBackend_ = 0;
    bool backendSwitching_ = false;
    BusyLevel busyLevel_ = BusyLevel::None;
    double progress_ = 0.0;
    QString undoLabel_;
    QString redoLabel_;
};

}  // namespace aoi::ui

