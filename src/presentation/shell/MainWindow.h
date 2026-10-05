#pragma once

#include <QMainWindow>
#include <QString>
#include <memory>

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
class QImage;

namespace aoi::ui {

class EmptyCard;
class ImageCanvas;
class MessageBar;
class ShellController;
class ThemeManager;

// 主視窗骨架（docs/ux/ux-spec.md 第 4 節）：上方工具列、左側工具、中央畫布與訊息列、
// 右側三個 dock（樣本庫、物件樹、比對參數；本步驟先放空殼）、狀態列。
// 這一層只負責組裝與事件接線，不含業務規則：畫布的操作走 ICanvasController，S04 才接上 use case。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(ThemeManager& theme, QWidget* parent = nullptr);
    ~MainWindow() override;

    ImageCanvas* canvas() const { return canvas_; }
    MessageBar* messageBar() const { return messageBar_; }
    EmptyCard* emptyCard() const { return emptyCard_; }
    QAction* toolAction(ToolId id) const;
    QAction* fitAction() const { return fitAction_; }
    QAction* openAction() const { return openAction_; }
    QAction* undoAction() const { return undoAction_; }
    QDockWidget* libraryDock() const { return libraryDock_; }
    QDockWidget* objectsDock() const { return objectsDock_; }
    QDockWidget* paramsDock() const { return paramsDock_; }
    QLabel* zoomLabel() const { return zoomLabel_; }
    QLabel* cursorLabel() const { return cursorLabel_; }
    QLabel* summaryLabel() const { return summaryLabel_; }

    // 解碼影像檔並顯示；失敗時在訊息列顯示錯誤卡片，舊影像保留。
    bool openImage(const QString& path);
    // 已有 QImage 時（示範場景、測試）。name 只用在視窗標題。
    void setImage(const QImage& image, const QString& name);

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
    void refreshHint();
    void centerEmptyCard();
    void promptOpen();

    ThemeManager& theme_;
    ImageCanvas* canvas_ = nullptr;
    MessageBar* messageBar_ = nullptr;
    EmptyCard* emptyCard_ = nullptr;
    std::unique_ptr<ShellController> controller_;

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
    QLabel* objectsEmpty_ = nullptr;
    QLabel* scoreLabel_ = nullptr;
    QSlider* scoreSlider_ = nullptr;
    QLabel* scopeLabel_ = nullptr;
    QComboBox* scopeCombo_ = nullptr;

    QLabel* summaryLabel_ = nullptr;
    QLabel* cursorLabel_ = nullptr;
    QLabel* zoomLabel_ = nullptr;

    QString imageName_;
    bool dragging_ = false;
    ToolId dragTool_ = ToolId::Select;
};

}  // namespace aoi::ui
