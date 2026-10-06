#include "presentation/shell/MainWindow.h"

#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QSlider>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <string>

#include "presentation/canvas/ImageCanvas.h"
#include "presentation/i18n/Translations.h"
#include "presentation/shell/EmptyCard.h"
#include "presentation/objects/ObjectsPanel.h"
#include "presentation/params/ParamsPanel.h"
#include "presentation/shell/MessageBar.h"
#include "presentation/theme/ThemeManager.h"

namespace aoi::ui {
namespace {

std::string utf8(const QString& s) { return s.toUtf8().toStdString(); }

QString withShortcut(const QString& name, const QString& keys) { return QStringLiteral("%1 (%2)").arg(name, keys); }

}  // namespace

MainWindow::MainWindow(ThemeManager& theme, QWidget* parent) : QMainWindow(parent), theme_(theme) {
    const ThemeTokens& tokens = theme.tokens();
    setMinimumSize(tokens.sizes.windowMin);
    resize(1280, 800);
    setAcceptDrops(true);

    // 中央：畫布在上，訊息列在下（固定高度，所以畫布大小不會因訊息而變）。
    auto* central = new QWidget(this);
    auto* column = new QVBoxLayout(central);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    canvas_ = new ImageCanvas(tokens, central);
    messageBar_ = new MessageBar(tokens, central);
    column->addWidget(canvas_, 1);
    column->addWidget(messageBar_, 0);
    setCentralWidget(central);

    emptyCard_ = new EmptyCard(canvas_->viewport());
    canvas_->viewport()->installEventFilter(this);
    connect(emptyCard_, &EmptyCard::openRequested, this, &MainWindow::promptOpen);

    buildActions();
    buildMenus();
    buildToolbars();
    buildDocks();
    buildStatusBar();

    connect(&theme_, &ThemeManager::changed, this, [this] { canvas_->applyTheme(theme_.colors()); });
    canvas_->applyTheme(theme_.colors());

    connect(canvas_, &ImageCanvas::zoomChanged, this, [this](double percent) {
        zoomLabel_->setText(uiText("ui.status.zoom", {{"percent", QString::number(qRound(percent))}}));
    });
    connect(canvas_, &ImageCanvas::cursorMoved, this, [this](const QPointF& p, bool inside) {
        cursorLabel_->setText(inside ? uiText("ui.status.cursor", {{"x", QString::number(qRound(p.x()))},
                                                                   {"y", QString::number(qRound(p.y()))}})
                                     : QString());
    });
    connect(canvas_, &ImageCanvas::cursorLeft, this, [this] { cursorLabel_->clear(); });
    connect(canvas_, &ImageCanvas::imageChanged, this, [this](bool) { refreshState(); });
    connect(canvas_, &ImageCanvas::activeToolChanged, this, [this](ToolId id) {
        if (QAction* a = toolAction(id)) a->setChecked(true);  // 程式換工具（〔重畫樣本〕）時，按鈕跟著換
    });
    // View 的操作 → Controller。Controller 是 setControllers 交進來的介面；還沒設定時沒有人聽（獨立的視窗測試）。
    // 按過訊息卡上的按鈕之後，鍵盤焦點還給畫布：Del、方向鍵、Esc 不必再點一下畫布才有用。
    connect(messageBar_, &MessageBar::actionTriggered, this, [this](int i) {
        if (controllers_.message) controllers_.message->applyAdvice(i);
        canvas_->setFocus(Qt::OtherFocusReason);
    });
    connect(messageBar_, &MessageBar::cancelRequested, this, [this] {
        if (controllers_.proposal) controllers_.proposal->cancelSearch();
        canvas_->setFocus(Qt::OtherFocusReason);
    });
    objectsPanel_->view()->installEventFilter(this);  // 焦點在物件樹上時，Enter、Esc、Del 也要到得了 Presenter
    connect(objectsPanel_, &ObjectsPanel::selectionChangedByUser, this, [this](const std::vector<int>& ids) {
        if (controllers_.selection) controllers_.selection->select(ids);
    });
    connect(paramsPanel_, &ParamsPanel::thresholdChanged, this, [this](double v) {
        if (controllers_.params) controllers_.params->setScoreThreshold(v);
    });
    connect(paramsPanel_, &ParamsPanel::angleRangeChanged, this, [this](double lo, double hi) {
        if (controllers_.params) controllers_.params->setAngleRange(lo, hi);
    });
    connect(paramsPanel_, &ParamsPanel::scopeChanged, this, [this](int i) {
        if (controllers_.params) controllers_.params->setScanScope(i);
    });
    connect(paramsPanel_, &ParamsPanel::significantBitsChanged, this, [this](int bits) {
        if (controllers_.params) controllers_.params->setSignificantBits(bits);
    });

    retranslateUi();
    refreshState();
    zoomLabel_->setText(uiText("ui.status.zoom", {{"percent", QStringLiteral("100")}}));
}

MainWindow::~MainWindow() {
    // Controller 不歸視窗所有，畫布以原始指標持有它們；視窗解構時畫布還在，先解除以免解構過程中的事件走到已釋放的物件。
    if (canvas_) {
        canvas_->setController(nullptr);
        canvas_->setSelectionController(nullptr);
        canvas_->setKeyboardController(nullptr);
    }
}

void MainWindow::setControllers(const Controllers& controllers) {
    controllers_ = controllers;
    canvas_->setController(controllers.canvas);
    canvas_->setSelectionController(controllers.selection);
    canvas_->setKeyboardController(controllers.keyboard);
}

QAction* MainWindow::toolAction(ToolId id) const {
    switch (id) {
        case ToolId::Select: return toolSelect_;
        case ToolId::Sample: return toolSample_;
        case ToolId::Region: return toolRegion_;
        case ToolId::Add: return toolAdd_;
    }
    return nullptr;
}

// ---- 組裝 ----------------------------------------------------------------------------------------------

void MainWindow::buildActions() {
    openAction_ = new QAction(this);
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, this, &MainWindow::promptOpen);
    saveAction_ = new QAction(this);
    saveAction_->setShortcut(QKeySequence::Save);
    saveAction_->setEnabled(false);  // 配方的儲存層在 S06（infrastructure/storage）；存檔與開啟配方的 UI 還沒排進任何一步（docs/current-status.md）
    exportAction_ = new QAction(this);
    exportAction_->setEnabled(false);  // 匯出在 S12
    quitAction_ = new QAction(this);
    quitAction_->setShortcut(QKeySequence::Quit);
    connect(quitAction_, &QAction::triggered, this, &QWidget::close);

    undoAction_ = new QAction(this);
    undoAction_->setShortcut(QKeySequence::Undo);
    undoAction_->setEnabled(false);  // Presenter 依歷史堆疊的狀態啟用（IHistoryView）
    connect(undoAction_, &QAction::triggered, this, [this] {
        if (controllers_.history) controllers_.history->undo();
    });
    redoAction_ = new QAction(this);
    redoAction_->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    redoAction_->setEnabled(false);
    connect(redoAction_, &QAction::triggered, this, [this] {
        if (controllers_.history) controllers_.history->redo();
    });

    fitAction_ = new QAction(this);
    fitAction_->setShortcut(Qt::Key_F);
    connect(fitAction_, &QAction::triggered, this, [this] { canvas_->fit(); });
    fitWidthAction_ = new QAction(this);
    fitWidthAction_->setShortcut(Qt::Key_W);
    connect(fitWidthAction_, &QAction::triggered, this, [this] { canvas_->fitWidth(); });

    toolGroup_ = new QActionGroup(this);
    toolGroup_->setExclusive(true);
    const struct {
        QAction** action;
        ToolId id;
        const char* glyph;
    } tools[] = {{&toolSelect_, ToolId::Select, "V"},
                 {&toolSample_, ToolId::Sample, "S"},
                 {&toolRegion_, ToolId::Region, "R"},
                 {&toolAdd_, ToolId::Add, "＋"}};
    for (const auto& t : tools) {
        QAction* a = new QAction(QString::fromUtf8(t.glyph), this);
        a->setCheckable(true);
        toolGroup_->addAction(a);
        const ToolId id = t.id;
        connect(a, &QAction::triggered, this, [this, id] { canvas_->setInteraction(id); });
        *t.action = a;
    }
    toolSelect_->setShortcut(Qt::Key_V);
    toolSample_->setShortcut(Qt::Key_S);
    toolRegion_->setShortcut(Qt::Key_R);
    toolAdd_->setShortcuts({QKeySequence(Qt::Key_A), QKeySequence(Qt::Key_Plus)});
    toolSelect_->setChecked(true);
}

void MainWindow::buildMenus() {
    fileMenu_ = menuBar()->addMenu(QString());
    fileMenu_->addAction(openAction_);
    fileMenu_->addAction(saveAction_);
    fileMenu_->addAction(exportAction_);
    fileMenu_->addSeparator();
    fileMenu_->addAction(quitAction_);

    editMenu_ = menuBar()->addMenu(QString());
    editMenu_->addAction(undoAction_);
    editMenu_->addAction(redoAction_);

    viewMenu_ = menuBar()->addMenu(QString());
    viewMenu_->addAction(fitAction_);
    viewMenu_->addAction(fitWidthAction_);
    viewMenu_->addSeparator();

    themeMenu_ = viewMenu_->addMenu(QString());
    auto* themeGroup = new QActionGroup(this);
    const struct {
        QAction** action;
        ThemeMode mode;
    } themes[] = {{&themeLight_, ThemeMode::Light}, {&themeDark_, ThemeMode::Dark}, {&themeSystem_, ThemeMode::System}};
    for (const auto& t : themes) {
        QAction* a = new QAction(this);
        a->setCheckable(true);
        a->setChecked(theme_.mode() == t.mode);
        themeGroup->addAction(a);
        themeMenu_->addAction(a);
        const ThemeMode mode = t.mode;
        connect(a, &QAction::triggered, this, [this, mode] { theme_.setMode(mode); });
        *t.action = a;
    }

    fontMenu_ = viewMenu_->addMenu(QString());
    auto* fontGroup = new QActionGroup(this);
    for (double scale : theme_.tokens().fontScaleOptions) {
        QAction* a = new QAction(QStringLiteral("%1%").arg(qRound(scale * 100)), this);
        a->setCheckable(true);
        a->setChecked(qFuzzyCompare(theme_.fontScale(), scale));
        fontGroup->addAction(a);
        fontMenu_->addAction(a);
        connect(a, &QAction::triggered, this, [this, scale] { theme_.setFontScale(scale); });
    }

    languageMenu_ = viewMenu_->addMenu(QString());
    auto* langGroup = new QActionGroup(this);
    langZh_ = new QAction(QStringLiteral("繁體中文"), this);  // 語言名稱用該語言自己的寫法，不翻譯
    langEn_ = new QAction(QStringLiteral("English"), this);
    for (QAction* a : {langZh_, langEn_}) {
        a->setCheckable(true);
        langGroup->addAction(a);
        languageMenu_->addAction(a);
    }
    langZh_->setChecked(Translations::current() == QLatin1String("zh_TW"));
    langEn_->setChecked(Translations::current() == QLatin1String("en"));
    connect(langZh_, &QAction::triggered, this, [] { Translations::install(QStringLiteral("zh_TW")); });
    connect(langEn_, &QAction::triggered, this, [] { Translations::install(QStringLiteral("en")); });

    panelsMenu_ = viewMenu_->addMenu(QString());  // dock 的開關在 buildDocks 之後加入
}

void MainWindow::buildToolbars() {
    topBar_ = addToolBar(QString());
    topBar_->setObjectName(QStringLiteral("topBar"));
    topBar_->setMovable(false);
    topBar_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    topBar_->addAction(openAction_);
    topBar_->addAction(saveAction_);
    topBar_->addSeparator();
    backendLabel_ = new QLabel(topBar_);
    backendCombo_ = new QComboBox(topBar_);
    backendCombo_->setEnabled(false);  // 執行中切換 back end 要等 S07（現在只有一個可用的）；清單由 setBackends 填
    topBar_->addWidget(backendLabel_);
    topBar_->addWidget(backendCombo_);
    auto* spacer = new QWidget(topBar_);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    topBar_->addWidget(spacer);
    topBar_->addAction(undoAction_);
    topBar_->addAction(redoAction_);
    topBar_->addAction(fitAction_);
    topBar_->addAction(fitWidthAction_);
    topBar_->addAction(exportAction_);

    toolRail_ = new QToolBar(this);
    toolRail_->setObjectName(QStringLiteral("toolRail"));
    toolRail_->setOrientation(Qt::Vertical);
    toolRail_->setMovable(false);
    toolRail_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    addToolBar(Qt::LeftToolBarArea, toolRail_);
    for (QAction* a : {toolSelect_, toolSample_, toolRegion_, toolAdd_}) toolRail_->addAction(a);
}

void MainWindow::buildDocks() {
    const int minWidth = theme_.tokens().sizes.dockMinWidth;
    auto makeDock = [&](const char* name, QWidget* content) {
        auto* dock = new QDockWidget(this);
        dock->setObjectName(QLatin1String(name));
        dock->setMinimumWidth(minWidth);
        dock->setWidget(content);
        addDockWidget(Qt::RightDockWidgetArea, dock);
        panelsMenu_->addAction(dock->toggleViewAction());
        return dock;
    };
    auto placeholder = [](QLabel*& label, QWidget* parent) {
        label = new QLabel(parent);
        label->setWordWrap(true);
        label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        label->setForegroundRole(QPalette::PlaceholderText);
        label->setMargin(12);
    };

    // 樣本庫（空殼：篩選欄與空狀態文字）。清單與縮圖在 S06／S04。
    auto* libraryBody = new QWidget(this);
    auto* libraryColumn = new QVBoxLayout(libraryBody);
    libraryColumn->setContentsMargins(8, 8, 8, 8);
    libraryFilter_ = new QLineEdit(libraryBody);
    libraryFilter_->setEnabled(false);
    libraryFilter_->setClearButtonEnabled(true);
    placeholder(libraryEmpty_, libraryBody);
    libraryColumn->addWidget(libraryFilter_);
    libraryColumn->addWidget(libraryEmpty_, 1);
    libraryDock_ = makeDock("libraryDock", libraryBody);

    // 物件樹：Board › Component › 框群組，可依狀態篩選，與畫布雙向選取同步（S04）。
    objectsPanel_ = new ObjectsPanel(this);
    objectsDock_ = makeDock("objectsDock", objectsPanel_);

    // 比對參數：分數門檻（拖動即時重新過濾）、進階的角度範圍（防抖後重新搜尋）、比對範圍。
    paramsPanel_ = new ParamsPanel(this);
    paramsDock_ = makeDock("paramsDock", paramsPanel_);
}

void MainWindow::buildStatusBar() {
    summaryLabel_ = new QLabel(this);
    cursorLabel_ = new QLabel(this);
    cursorLabel_->setMinimumWidth(110);
    zoomLabel_ = new QLabel(this);
    zoomLabel_->setMinimumWidth(90);
    zoomLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(summaryLabel_, 1);
    statusBar()->addPermanentWidget(cursorLabel_);
    statusBar()->addPermanentWidget(zoomLabel_);
}

// ---- 文字與狀態 ----------------------------------------------------------------------------------------

void MainWindow::retranslateUi() {
    setWindowTitle(imageName_.isEmpty() ? qtTrId("app.title")
                                        : QStringLiteral("%1 — %2").arg(imageName_, qtTrId("app.title")));

    openAction_->setText(qtTrId("action.open"));
    saveAction_->setText(qtTrId("action.save"));
    exportAction_->setText(qtTrId("action.export"));
    quitAction_->setText(qtTrId("action.quit"));
    undoAction_->setText(QStringLiteral("↶ ") + qtTrId("action.undo"));
    redoAction_->setText(QStringLiteral("↷ ") + qtTrId("action.redo"));
    fitAction_->setText(QStringLiteral("⤢ ") + qtTrId("action.fit"));
    fitAction_->setToolTip(withShortcut(qtTrId("action.fit"), QStringLiteral("F")));
    fitWidthAction_->setText(QStringLiteral("↔ ") + qtTrId("action.fitWidth"));
    fitWidthAction_->setToolTip(withShortcut(qtTrId("action.fitWidth"), QStringLiteral("W")));
    toolSelect_->setToolTip(withShortcut(qtTrId("tool.select"), QStringLiteral("V")));
    toolSample_->setToolTip(withShortcut(qtTrId("tool.sample"), QStringLiteral("S")));
    toolRegion_->setToolTip(withShortcut(qtTrId("tool.scan"), QStringLiteral("R")));
    toolAdd_->setToolTip(withShortcut(qtTrId("tool.add"), QStringLiteral("A")));

    fileMenu_->setTitle(qtTrId("menu.file"));
    editMenu_->setTitle(qtTrId("menu.edit"));
    viewMenu_->setTitle(qtTrId("menu.view"));
    themeMenu_->setTitle(qtTrId("menu.theme"));
    fontMenu_->setTitle(qtTrId("ui.settings.fontScale"));
    languageMenu_->setTitle(qtTrId("menu.language"));
    panelsMenu_->setTitle(qtTrId("menu.panels"));
    themeLight_->setText(qtTrId("ui.settings.theme.light"));
    themeDark_->setText(qtTrId("ui.settings.theme.dark"));
    themeSystem_->setText(qtTrId("ui.settings.theme.system"));

    backendLabel_->setText(qtTrId("ui.backend.label") + QStringLiteral(" "));

    libraryDock_->setWindowTitle(qtTrId("ui.library.title"));
    objectsDock_->setWindowTitle(qtTrId("ui.objects.title"));
    paramsDock_->setWindowTitle(qtTrId("ui.params.title"));
    libraryFilter_->setPlaceholderText(qtTrId("ui.library.filter"));
    libraryEmpty_->setText(qtTrId("ui.library.empty"));
    objectsPanel_->retranslate();
    paramsPanel_->retranslate();
    updateHistoryTips();
    updateSummaryLabel();
    zoomLabel_->setText(uiText("ui.status.zoom", {{"percent", QString::number(qRound(canvas_->zoomPercent()))}}));
    emptyCard_->retranslate();
    emptyCard_->adjustSize();
    centerEmptyCard();
    langZh_->setChecked(Translations::current() == QLatin1String("zh_TW"));
    langEn_->setChecked(Translations::current() == QLatin1String("en"));
}

void MainWindow::refreshState() {
    const bool has = canvas_->hasImage();
    for (QAction* a : {toolSelect_, toolSample_, toolRegion_, toolAdd_}) a->setEnabled(has);
    fitAction_->setEnabled(has);
    fitWidthAction_->setEnabled(has);
    emptyCard_->setVisible(!has);
    centerEmptyCard();
}

void MainWindow::centerEmptyCard() {
    QWidget* vp = canvas_->viewport();
    emptyCard_->adjustSize();
    emptyCard_->move((vp->width() - emptyCard_->width()) / 2, (vp->height() - emptyCard_->height()) / 2);
    emptyCard_->raise();
}

// ---- 開啟影像 ------------------------------------------------------------------------------------------

void MainWindow::promptOpen() {
    const QString filter =
        QStringLiteral("%1 (*.png *.bmp *.jpg *.jpeg *.tif *.tiff)").arg(qtTrId("ui.shell.imageFilter"));
    const QString path = QFileDialog::getOpenFileName(this, qtTrId("ui.shell.openTitle"), QString(), filter);
    if (!path.isEmpty()) openImage(path);
}

bool MainWindow::openImage(const QString& path) {
    const QString name = QFileInfo(path).fileName();
    if (imageSource_ == nullptr) {
        emit imageOpenFailed(name, QStringLiteral("No image source is set."));
        return false;
    }
    const aoi::Result<aoi::ImageView> loaded = imageSource_->load(utf8(path));
    if (!loaded) {
        // 舊影像保留。錯誤卡片是 Presenter 的事（它知道目前的狀態，Dismiss 要回到 Idle 還是 Empty）。
        emit imageOpenFailed(name, QString::fromStdString(loaded.error().message));
        return false;
    }
    setImage(loaded.value(), name);
    return true;
}

void MainWindow::setImage(const aoi::ImageView& image, const QString& name) {
    imageName_ = name;
    canvas_->showImage(image);
    setWindowTitle(QStringLiteral("%1 — %2").arg(name, qtTrId("app.title")));
    canvas_->setFocus(Qt::OtherFocusReason);
    emit imageOpened(image, name);
}

void MainWindow::selectTool(ToolId id) {
    if (QAction* a = toolAction(id)) {
        if (a->isEnabled()) a->trigger();
    }
}

void MainWindow::setBackends(const QStringList& names, int current) {
    backendCombo_->clear();
    backendCombo_->addItems(names);
    backendCombo_->setCurrentIndex(current);
    backendCombo_->setEnabled(false);
}

// ---- IStatusView ---------------------------------------------------------------------------------------

void MainWindow::updateSummaryLabel() {
    switch (busyLevel_) {
        case BusyLevel::None: summaryLabel_->setText(summaryText_); break;
        case BusyLevel::Busy: summaryLabel_->setText(uiText("ui.search.busy")); break;
        case BusyLevel::Progress:
            summaryLabel_->setText(uiText("ui.search.progress", {{"percent", QString::number(qRound(progress_ * 100.0))}}));
            break;
    }
}

void MainWindow::showSummary(const QString& text) {
    summaryText_ = text;
    updateSummaryLabel();
}

void MainWindow::showBusy(BusyLevel level) {
    busyLevel_ = level;
    if (level == BusyLevel::Progress) showProgress(progress_);
    else messageBar_->clearProgress();
    updateSummaryLabel();
}

void MainWindow::showProgress(double fraction) {
    progress_ = fraction;
    updateSummaryLabel();
    if (busyLevel_ != BusyLevel::Progress) return;
    messageBar_->showProgress(uiText("ui.search.progress", {{"percent", QString::number(qRound(fraction * 100.0))}}), fraction,
                              uiText("action.cancel") + QStringLiteral(" (Esc)"));
}

void MainWindow::showToast(const QString& text) { messageBar_->showToast(text); }

void MainWindow::setHint(const QString& text) { messageBar_->setHint(text); }

// ---- IMessageView --------------------------------------------------------------------------------------

void MainWindow::showCard(const MessageCard& card) {
    messageBar_->showCard(card.title, card.body, card.advice, card.actions);
}

void MainWindow::hideCard() { messageBar_->clearCard(); }

// ---- IHistoryView --------------------------------------------------------------------------------------

void MainWindow::updateHistoryTips() {
    undoAction_->setToolTip(undoLabel_.isEmpty() ? withShortcut(qtTrId("action.undo"), QStringLiteral("Ctrl+Z"))
                                                 : uiText("ui.history.undo", {{"label", undoLabel_}}));
    redoAction_->setToolTip(redoLabel_.isEmpty() ? withShortcut(qtTrId("action.redo"), QStringLiteral("Ctrl+Y"))
                                                 : uiText("ui.history.redo", {{"label", redoLabel_}}));
}

void MainWindow::setUndo(bool enabled, const QString& label) {
    undoAction_->setEnabled(enabled);
    undoLabel_ = enabled ? label : QString();
    updateHistoryTips();
}

void MainWindow::setRedo(bool enabled, const QString& label) {
    redoAction_->setEnabled(enabled);
    redoLabel_ = enabled ? label : QString();
    updateHistoryTips();
}

// ---- 事件 ----------------------------------------------------------------------------------------------

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    QMainWindow::changeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile() && imageSource_ != nullptr && imageSource_->canRead(utf8(url.toLocalFile()))) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile() && imageSource_ != nullptr && imageSource_->canRead(utf8(url.toLocalFile()))) {
            openImage(url.toLocalFile());
            event->acceptProposedAction();
            return;
        }
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == canvas_->viewport() && event->type() == QEvent::Resize) centerEmptyCard();
    if (watched == objectsPanel_->view() && event->type() == QEvent::KeyPress && controllers_.keyboard) {
        // 樹自己用方向鍵、Home、End 導覽；Enter、Esc、Del、Backspace 的意義和畫布上一樣，交給 Presenter（它不處理就還給樹）。
        const auto* key = static_cast<QKeyEvent*>(event);
        switch (key->key()) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Escape:
            case Qt::Key_Delete:
            case Qt::Key_Backspace:
                if (controllers_.keyboard->onKeyPressed(key->key(), key->modifiers())) return true;
                break;
            default: break;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

}  // namespace aoi::ui
