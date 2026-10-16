#include "presentation/shell/MainWindow.h"

#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeView>
#include <QUrl>
#include <string>

#include "presentation/canvas/ImageCanvas.h"
#include "presentation/compare/CompareWindow.h"
#include "presentation/guidance/GuidancePanel.h"
#include "presentation/i18n/Translations.h"
#include "presentation/library/SamplePanel.h"
#include "presentation/shell/EmptyCard.h"
#include "presentation/objects/ObjectsPanel.h"
#include "presentation/params/ParamsPanel.h"
#include "presentation/shell/MessageBar.h"
#include "presentation/theme/ThemeManager.h"
#include "ui_MainWindow.h"

namespace aoi::ui {
namespace {

std::string utf8(const QString& s) { return s.toUtf8().toStdString(); }

QString withShortcut(const QString& name, const QString& keys) { return QStringLiteral("%1 (%2)").arg(name, keys); }

}  // namespace

MainWindow::MainWindow(ThemeManager& theme, QWidget* parent)
    : QMainWindow(parent), theme_(theme), ui_(std::make_unique<Ui::MainWindow>()) {
    // 外框在 MainWindow.ui（Qt Designer）：選單、上方工具列、左側的工具、右側三個 dock 的配置、中央的畫布與訊息列
    // （畫布、訊息列、物件樹與參數面板是提升元件：Designer 裡是佔位，執行時是這些類別）。中央：畫布在上，訊息列在下
    // （固定高度，所以畫布大小不會因訊息而變）。.ui 裡的文字只是佔位、不翻譯，retranslateUi 從 copy.json 設定；
    // Designer 放不了的（快捷鍵、互斥的動作群組、工具列上的 back end 選單、狀態列的標籤、字級選單、dock 的開關）在 build* 裡。
    ui_->setupUi(this);
    const ThemeTokens& tokens = theme.tokens();
    setMinimumSize(tokens.sizes.windowMin);
    resize(1280, 800);
    setAcceptDrops(true);

    canvas_ = ui_->canvas;
    messageBar_ = ui_->messageBar;
    objectsPanel_ = ui_->objectsPanel;
    paramsPanel_ = ui_->paramsPanel;
    samplePanel_ = ui_->samplePanel;

    emptyCard_ = new EmptyCard(canvas_->viewport());
    // A/B 比較的視窗（S09）：另一個頂層視窗，主視窗擁有它（一起關、一起解構）；顯示與內容由 ComparePresenter 決定。
    compareWindow_ = new CompareWindow(tokens, this);
    compareWindow_->hide();
    canvas_->viewport()->installEventFilter(this);
    connect(emptyCard_, &EmptyCard::openRequested, this, &MainWindow::promptOpen);

    buildActions();
    buildMenus();
    buildToolbars();
    buildDocks();
    buildStatusBar();

    connect(&theme_, &ThemeManager::changed, this, [this] {
        canvas_->applyTheme(theme_.colors());
        compareWindow_->applyTheme(theme_.colors());
    });
    canvas_->applyTheme(theme_.colors());
    compareWindow_->applyTheme(theme_.colors());

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
    connect(messageBar_, &MessageBar::noticeActionTriggered, this, [this](int i) {
        if (controllers_.backend) controllers_.backend->applyNoticeAction(i);
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
    // 選取資訊表單（2026-10-14）：一次輸入一個 Command；按過按鈕之後焦點還給畫布。
    connect(objectsPanel_, &ObjectsPanel::centerEdited, this, [this](double x, double y) {
        if (controllers_.selectionEdit) controllers_.selectionEdit->setSelectionCenter(x, y);
    });
    connect(objectsPanel_, &ObjectsPanel::sizeEdited, this, [this](double w, double h) {
        if (controllers_.selectionEdit) controllers_.selectionEdit->setSelectionBodySize(w, h);
    });
    connect(objectsPanel_, &ObjectsPanel::applySizeToAllRequested, this, [this] {
        if (controllers_.selectionEdit) controllers_.selectionEdit->applySizeToAllSamples();
        canvas_->setFocus(Qt::OtherFocusReason);
    });
    // 樣本庫 dock：點一筆 ＝ 在畫布上選取它（和點畫布相同）。
    connect(samplePanel_, &SamplePanel::sampleClicked, this, [this](int id) {
        if (controllers_.selection) controllers_.selection->select({id});
        canvas_->setFocus(Qt::OtherFocusReason);
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
    connect(paramsPanel_, &ParamsPanel::snapToLatticeChanged, this, [this](bool snap) {
        if (controllers_.params) controllers_.params->setSnapToLattice(snap);
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
    compareWindow_->setController(controllers.compare);
}

QAction* MainWindow::toolAction(ToolId id) const {
    switch (id) {
        case ToolId::Select: return toolSelect_;
        case ToolId::Sample: return toolSample_;
        case ToolId::Region: return toolRegion_;
        case ToolId::Add: return toolAdd_;
        case ToolId::Box: return toolBox_;
    }
    return nullptr;
}

// ---- 組裝 ----------------------------------------------------------------------------------------------

void MainWindow::buildActions() {
    // 動作本身（名稱、可勾選、預設停用）在 MainWindow.ui；快捷鍵與行為在這裡（標準快捷鍵跟著平台、重做有兩個快捷鍵，Designer 表達不了）。
    openAction_ = ui_->actionOpen;
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, this, &MainWindow::promptOpen);
    saveAction_ = ui_->actionSave;
    saveAction_->setShortcut(QKeySequence::Save);
    // 〔儲存〕停用（.ui）：配方的儲存層在 S06（infrastructure/storage）；存檔與開啟配方的 UI 還沒排進任何一步（docs/current-status.md）
    exportAction_ = ui_->actionExport;  // 停用（.ui）：匯出在 S12
    quitAction_ = ui_->actionQuit;
    quitAction_->setShortcut(QKeySequence::Quit);
    connect(quitAction_, &QAction::triggered, this, &QWidget::close);

    undoAction_ = ui_->actionUndo;  // 停用（.ui）：Presenter 依歷史堆疊的狀態啟用（IHistoryView）
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, this, [this] {
        if (controllers_.history) controllers_.history->undo();
    });
    redoAction_ = ui_->actionRedo;
    redoAction_->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    connect(redoAction_, &QAction::triggered, this, [this] {
        if (controllers_.history) controllers_.history->redo();
    });

    // 〔搜尋〕（F5；ADR-025）：用板子上全部樣本搜尋。有影像就能按；沒有樣本時 Presenter 用吐司說明。
    searchAction_ = ui_->actionSearch;
    searchAction_->setShortcut(Qt::Key_F5);
    connect(searchAction_, &QAction::triggered, this, [this] {
        if (controllers_.proposal) controllers_.proposal->search();
    });
    // 全選（Ctrl+A）、選取同類型（Ctrl+Shift+A）。
    selectAllAction_ = ui_->actionSelectAll;
    selectAllAction_->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction_, &QAction::triggered, this, [this] {
        if (controllers_.selection) controllers_.selection->selectAll();
    });
    selectSameTypeAction_ = ui_->actionSelectSameType;
    selectSameTypeAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
    connect(selectSameTypeAction_, &QAction::triggered, this, [this] {
        if (controllers_.selection) controllers_.selection->selectSameType();
    });

    fitAction_ = ui_->actionFit;
    fitAction_->setShortcut(Qt::Key_F);
    connect(fitAction_, &QAction::triggered, this, [this] { canvas_->fit(); });
    fitWidthAction_ = ui_->actionFitWidth;
    fitWidthAction_->setShortcut(Qt::Key_W);
    connect(fitWidthAction_, &QAction::triggered, this, [this] { canvas_->fitWidth(); });
    // A/B 比較（S09）：停用（.ui）；ComparePresenter 依「有沒有樣本、有沒有兩個能用的 back end」啟用（ICompareEntryView）。
    compareAction_ = ui_->actionCompare;
    compareAction_->setObjectName(QStringLiteral("actionCompare"));
    connect(compareAction_, &QAction::triggered, this, [this] {
        if (controllers_.compare) controllers_.compare->openCompare();
    });

    // 左側的工具（V、S、R、＋；.ui 裡是可勾選的動作，選取工具預設勾選）：同一時間只有一個。
    toolGroup_ = new QActionGroup(this);
    toolGroup_->setExclusive(true);
    const struct {
        QAction** action;
        QAction* fromUi;
        ToolId id;
    } tools[] = {{&toolSelect_, ui_->actionToolSelect, ToolId::Select},
                 {&toolSample_, ui_->actionToolSample, ToolId::Sample},
                 {&toolRegion_, ui_->actionToolRegion, ToolId::Region},
                 {&toolAdd_, ui_->actionToolAdd, ToolId::Add},
                 {&toolBox_, ui_->actionToolBox, ToolId::Box}};
    for (const auto& t : tools) {
        QAction* a = t.fromUi;
        toolGroup_->addAction(a);
        const ToolId id = t.id;
        connect(a, &QAction::triggered, this, [this, id] { canvas_->setInteraction(id); });
        *t.action = a;
    }
    toolSelect_->setShortcut(Qt::Key_V);
    toolSample_->setShortcut(Qt::Key_S);
    toolRegion_->setShortcut(Qt::Key_R);
    toolAdd_->setShortcuts({QKeySequence(Qt::Key_A), QKeySequence(Qt::Key_Plus)});
    toolBox_->setShortcut(Qt::Key_B);
    toolSelect_->setChecked(true);
}

void MainWindow::buildMenus() {
    // 選單的結構在 MainWindow.ui；互斥的群組、目前的設定與字級的選項（來自 tokens.json）在這裡。
    fileMenu_ = ui_->fileMenu;
    editMenu_ = ui_->editMenu;
    viewMenu_ = ui_->viewMenu;
    themeMenu_ = ui_->themeMenu;
    fontMenu_ = ui_->fontMenu;
    languageMenu_ = ui_->languageMenu;
    panelsMenu_ = ui_->panelsMenu;  // dock 的開關在 buildDocks 加入
    helpMenu_ = ui_->helpMenu;
    // 〔說明 › 操作步驟〕（F1）：把左側的操作步驟 dock 打開並帶到前面（關掉之後也找得回來）。
    guideAction_ = ui_->actionGuide;
    guideAction_->setShortcut(QKeySequence::HelpContents);
    connect(guideAction_, &QAction::triggered, this, [this] {
        guidanceDock_->show();
        guidanceDock_->raise();
    });

    auto* themeGroup = new QActionGroup(this);
    const struct {
        QAction** action;
        QAction* fromUi;
        ThemeMode mode;
    } themes[] = {{&themeLight_, ui_->actionThemeLight, ThemeMode::Light},
                  {&themeDark_, ui_->actionThemeDark, ThemeMode::Dark},
                  {&themeSystem_, ui_->actionThemeSystem, ThemeMode::System}};
    for (const auto& t : themes) {
        QAction* a = t.fromUi;
        a->setChecked(theme_.mode() == t.mode);
        themeGroup->addAction(a);
        const ThemeMode mode = t.mode;
        connect(a, &QAction::triggered, this, [this, mode] { theme_.setMode(mode); });
        *t.action = a;
    }

    auto* fontGroup = new QActionGroup(this);
    for (double scale : theme_.tokens().fontScaleOptions) {
        QAction* a = new QAction(QStringLiteral("%1%").arg(qRound(scale * 100)), this);
        a->setCheckable(true);
        a->setChecked(qFuzzyCompare(theme_.fontScale(), scale));
        fontGroup->addAction(a);
        fontMenu_->addAction(a);
        connect(a, &QAction::triggered, this, [this, scale] { theme_.setFontScale(scale); });
    }

    // 語言名稱用該語言自己的寫法，不翻譯（.ui 裡就是最後的文字）。
    auto* langGroup = new QActionGroup(this);
    langZh_ = ui_->actionLangZh;
    langEn_ = ui_->actionLangEn;
    for (QAction* a : {langZh_, langEn_}) langGroup->addAction(a);
    langZh_->setChecked(Translations::current() == QLatin1String("zh_TW"));
    langEn_->setChecked(Translations::current() == QLatin1String("en"));
    connect(langZh_, &QAction::triggered, this, [] { Translations::install(QStringLiteral("zh_TW")); });
    connect(langEn_, &QAction::triggered, this, [] { Translations::install(QStringLiteral("en")); });
}

void MainWindow::buildToolbars() {
    // 兩條工具列與上面的動作在 MainWindow.ui。back end 的標籤與選單、把右半邊推到右邊的彈性空間是工具列上的 widget，
    // Designer 放不進工具列，所以在這裡插在〔復原〕前面：開啟、儲存 | Back end [選單] ⟷ 復原、重做、適配、匯出。
    topBar_ = ui_->topBar;
    toolRail_ = ui_->toolRail;
    backendLabel_ = new QLabel(topBar_);
    backendCombo_ = new QComboBox(topBar_);
    backendCombo_->setObjectName(QStringLiteral("backendCombo"));
    // 使用者選了一項（程式設定的不算：activated 只在使用者操作時發出）→ 執行中切換（ADR-021）。清單由 setBackends 填。
    connect(backendCombo_, &QComboBox::activated, this, [this](int index) {
        const QString id = backendCombo_->itemData(index).toString();
        if (controllers_.backend && !id.isEmpty()) controllers_.backend->selectBackend(id.toStdString());
        canvas_->setFocus(Qt::OtherFocusReason);
    });
    auto* spacer = new QWidget(topBar_);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    topBar_->insertWidget(undoAction_, backendLabel_);
    topBar_->insertWidget(undoAction_, backendCombo_);
    topBar_->insertAction(undoAction_, compareAction_);  // back end 選單旁邊：〔A/B 比較…〕（S09）
    topBar_->insertWidget(undoAction_, spacer);
}

void MainWindow::buildDocks() {
    // 三個 dock 的配置（右側，由上而下：樣本庫、物件樹、比對參數）在 MainWindow.ui；最小寬度來自 tokens.json。
    //  - 樣本庫（SamplePanel，2026-10-14）：板子上的樣本與縮圖；篩選欄仍是停用的佔位（樣本庫檔案是 P2）。
    //  - 物件樹：Board › Component › 框群組，可依狀態篩選，與畫布雙向選取同步（S04）；下方是選取資訊的表單（2026-10-14）。
    //  - 比對參數：分數門檻（拖動即時重新過濾）、進階的角度範圍（防抖後重新搜尋）、比對範圍、影像位元數。
    //  - 操作步驟（左側，2026-10-15，ADR-027）：五步的清單、目前這一步標亮、每一步的示範（GuidancePanel）。
    libraryDock_ = ui_->libraryDock;
    objectsDock_ = ui_->objectsDock;
    paramsDock_ = ui_->paramsDock;
    guidanceDock_ = ui_->guidanceDock;
    guidancePanel_ = ui_->guidancePanel;
    const int minWidth = theme_.tokens().sizes.dockMinWidth;
    for (QDockWidget* dock : {guidanceDock_, libraryDock_, objectsDock_, paramsDock_}) {
        dock->setMinimumWidth(minWidth);
        panelsMenu_->addAction(dock->toggleViewAction());
    }
}

void MainWindow::buildStatusBar() {
    // 最左邊是 back end（「HALCON · runtime 授權」，一直看得到，搜尋中也是），接著是摘要（耗時、數量；搜尋中換成「搜尋中…」）。
    backendStatus_ = new QLabel(this);
    backendStatus_->setObjectName(QStringLiteral("backendStatus"));
    summaryLabel_ = new QLabel(this);
    summaryLabel_->setObjectName(QStringLiteral("statusSummary"));
    cursorLabel_ = new QLabel(this);
    cursorLabel_->setMinimumWidth(110);
    zoomLabel_ = new QLabel(this);
    zoomLabel_->setMinimumWidth(90);
    zoomLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusBar()->addWidget(backendStatus_, 0);
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
    compareAction_->setText(qtTrId("ui.compare.action"));
    searchAction_->setText(QStringLiteral("▶ ") + qtTrId("action.search"));
    searchAction_->setToolTip(withShortcut(qtTrId("action.search"), QStringLiteral("F5")));
    selectAllAction_->setText(qtTrId("action.selectAll"));
    selectSameTypeAction_->setText(qtTrId("action.selectSameType"));
    toolSelect_->setToolTip(withShortcut(qtTrId("tool.select"), QStringLiteral("V")));
    toolSample_->setToolTip(withShortcut(qtTrId("tool.sample"), QStringLiteral("S")));
    toolRegion_->setToolTip(withShortcut(qtTrId("tool.scan"), QStringLiteral("R")));
    toolAdd_->setToolTip(withShortcut(qtTrId("tool.add"), QStringLiteral("A")));
    toolBox_->setToolTip(withShortcut(qtTrId("tool.box"), QStringLiteral("B")));

    fileMenu_->setTitle(qtTrId("menu.file"));
    editMenu_->setTitle(qtTrId("menu.edit"));
    viewMenu_->setTitle(qtTrId("menu.view"));
    themeMenu_->setTitle(qtTrId("menu.theme"));
    fontMenu_->setTitle(qtTrId("ui.settings.fontScale"));
    languageMenu_->setTitle(qtTrId("menu.language"));
    panelsMenu_->setTitle(qtTrId("menu.panels"));
    helpMenu_->setTitle(qtTrId("menu.help"));
    guideAction_->setText(qtTrId("action.guide"));
    themeLight_->setText(qtTrId("ui.settings.theme.light"));
    themeDark_->setText(qtTrId("ui.settings.theme.dark"));
    themeSystem_->setText(qtTrId("ui.settings.theme.system"));

    backendLabel_->setText(qtTrId("ui.backend.label") + QStringLiteral(" "));
    refreshBackends();

    libraryDock_->setWindowTitle(qtTrId("ui.library.title"));
    objectsDock_->setWindowTitle(qtTrId("ui.objects.title"));
    paramsDock_->setWindowTitle(qtTrId("ui.params.title"));
    guidanceDock_->setWindowTitle(qtTrId("ui.guide.title"));
    guidancePanel_->retranslate();
    samplePanel_->retranslate();
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
    for (QAction* a : {toolSelect_, toolSample_, toolRegion_, toolAdd_, toolBox_, searchAction_, selectAllAction_, selectSameTypeAction_})
        a->setEnabled(has);
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

void MainWindow::setBackends(const QList<BackendChoice>& choices, int current, bool switching) {
    backends_ = choices;
    currentBackend_ = current;
    backendSwitching_ = switching;
    refreshBackends();
}

void MainWindow::refreshBackends() {
    backendCombo_->clear();
    QStringList tips;
    auto* model = qobject_cast<QStandardItemModel*>(backendCombo_->model());
    for (int i = 0; i < backends_.size(); ++i) {
        const BackendChoice& c = backends_[i];
        backendCombo_->addItem(c.available ? c.name : c.name + QStringLiteral(" ✕"), c.id);
        if (QStandardItem* item = model != nullptr ? model->item(i) : nullptr) item->setEnabled(c.available);
        QString tip = c.name;
        if (!c.available) tip = uiText("ui.backend.unavailable", {{"name", c.name}, {"reason", c.note}});
        else if (!c.note.isEmpty()) tip += QStringLiteral(": ") + c.note;
        backendCombo_->setItemData(i, tip, Qt::ToolTipRole);
        tips << tip;
    }
    backendCombo_->setCurrentIndex(currentBackend_);
    backendCombo_->setToolTip(tips.join(QLatin1Char('\n')));
    // 切換中停用；只有一個 back end 時也沒什麼可選。
    backendCombo_->setEnabled(!backendSwitching_ && backends_.size() > 1);
}

void MainWindow::setBackendStatus(const QString& text, const QString& detail) {
    backendStatus_->setText(text);
    backendStatus_->setToolTip(detail);
}

void MainWindow::showNotice(const MessageCard& card) {
    messageBar_->showNotice(card.title, card.body, card.advice, card.actions);
}

void MainWindow::hideNotice() { messageBar_->clearNotice(); }

// ---- ICompareEntryView ---------------------------------------------------------------------------------

void MainWindow::setCompareAvailable(bool enabled, const QString& tip) {
    compareAction_->setEnabled(enabled);
    compareAction_->setToolTip(tip);
    compareAction_->setStatusTip(tip);
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
