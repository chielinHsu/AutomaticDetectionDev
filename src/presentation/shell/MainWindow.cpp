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
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QSlider>
#include <QStatusBar>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#include "presentation/canvas/ImageCanvas.h"
#include "presentation/i18n/Translations.h"
#include "presentation/shell/EmptyCard.h"
#include "presentation/shell/ImageLoader.h"
#include "presentation/shell/MessageBar.h"
#include "presentation/shell/ShellController.h"
#include "presentation/theme/ThemeManager.h"

namespace aoi::ui {
namespace {

bool isImageFile(const QString& path) {
    static const QStringList suffixes = {"png", "bmp", "jpg", "jpeg", "tif", "tiff"};
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}

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

    controller_ = std::make_unique<ShellController>(*canvas_, *messageBar_, tokens);
    canvas_->setController(controller_.get());
    canvas_->setSelectionController(controller_.get());

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
    connect(canvas_, &ImageCanvas::pointerDragChanged, this, [this](bool active, ToolId tool) {
        dragging_ = active;
        dragTool_ = tool;
        refreshHint();
    });
    connect(messageBar_, &MessageBar::actionTriggered, this, [this](int) { messageBar_->clearCard(); });

    retranslateUi();
    refreshState();
    zoomLabel_->setText(uiText("ui.status.zoom", {{"percent", QStringLiteral("100")}}));
}

MainWindow::~MainWindow() {
    // controller_ 被畫布以原始指標持有；視窗解構時畫布還在，先解除以免解構過程中的事件走到已釋放的物件。
    if (canvas_) {
        canvas_->setController(nullptr);
        canvas_->setSelectionController(nullptr);
    }
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
    saveAction_->setEnabled(false);  // 配方儲存在 S06
    exportAction_ = new QAction(this);
    exportAction_->setEnabled(false);  // 匯出在 S12
    quitAction_ = new QAction(this);
    quitAction_->setShortcut(QKeySequence::Quit);
    connect(quitAction_, &QAction::triggered, this, &QWidget::close);

    undoAction_ = new QAction(this);
    undoAction_->setShortcut(QKeySequence::Undo);
    undoAction_->setEnabled(false);  // 復原堆疊在 S04
    redoAction_ = new QAction(this);
    redoAction_->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)});
    redoAction_->setEnabled(false);

    fitAction_ = new QAction(this);
    fitAction_->setShortcut(Qt::Key_F);
    connect(fitAction_, &QAction::triggered, this, [this] { canvas_->fit(); });

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
        connect(a, &QAction::triggered, this, [this, id] {
            canvas_->setInteraction(id);
            refreshHint();
        });
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
    backendCombo_->setEnabled(false);  // back end 清單在 S03 的 BackendRegistry
    backendCombo_->addItem(QString());
    topBar_->addWidget(backendLabel_);
    topBar_->addWidget(backendCombo_);
    auto* spacer = new QWidget(topBar_);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    topBar_->addWidget(spacer);
    topBar_->addAction(undoAction_);
    topBar_->addAction(redoAction_);
    topBar_->addAction(fitAction_);
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

    // 物件樹（空殼）。Board › Component › 框群組在 S04。
    auto* objectsBody = new QWidget(this);
    auto* objectsColumn = new QVBoxLayout(objectsBody);
    objectsColumn->setContentsMargins(8, 8, 8, 8);
    placeholder(objectsEmpty_, objectsBody);
    objectsColumn->addWidget(objectsEmpty_, 1);
    objectsDock_ = makeDock("objectsDock", objectsBody);

    // 比對參數（空殼：控制項在，但停用）。
    auto* paramsBody = new QWidget(this);
    auto* form = new QFormLayout(paramsBody);
    form->setContentsMargins(12, 8, 12, 8);
    scoreLabel_ = new QLabel(paramsBody);
    scoreSlider_ = new QSlider(Qt::Horizontal, paramsBody);
    scoreSlider_->setRange(30, 100);
    scoreSlider_->setValue(80);
    scoreSlider_->setEnabled(false);
    scopeLabel_ = new QLabel(paramsBody);
    scopeCombo_ = new QComboBox(paramsBody);
    scopeCombo_->setEnabled(false);
    for (int i = 0; i < 3; ++i) scopeCombo_->addItem(QString());
    form->addRow(scoreLabel_, scoreSlider_);
    form->addRow(scopeLabel_, scopeCombo_);
    paramsDock_ = makeDock("paramsDock", paramsBody);
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
    backendCombo_->setItemText(0, qtTrId("ui.shell.backendNone"));

    libraryDock_->setWindowTitle(qtTrId("ui.library.title"));
    objectsDock_->setWindowTitle(qtTrId("ui.objects.title"));
    paramsDock_->setWindowTitle(qtTrId("ui.params.title"));
    libraryFilter_->setPlaceholderText(qtTrId("ui.library.filter"));
    libraryEmpty_->setText(qtTrId("ui.library.empty"));
    objectsEmpty_->setText(qtTrId("ui.objects.empty"));
    scoreLabel_->setText(qtTrId("ui.params.score"));
    scopeLabel_->setText(qtTrId("ui.params.scanScope"));
    scopeCombo_->setItemText(0, qtTrId("ui.params.scope.auto"));
    scopeCombo_->setItemText(1, qtTrId("ui.params.scope.board"));
    scopeCombo_->setItemText(2, qtTrId("ui.params.scope.all"));

    summaryLabel_->setText(uiText("ui.status.none", {{"backend", qtTrId("ui.shell.backendNone")}}));
    zoomLabel_->setText(uiText("ui.status.zoom", {{"percent", QString::number(qRound(canvas_->zoomPercent()))}}));
    emptyCard_->retranslate();
    emptyCard_->adjustSize();
    centerEmptyCard();
    refreshHint();
    langZh_->setChecked(Translations::current() == QLatin1String("zh_TW"));
    langEn_->setChecked(Translations::current() == QLatin1String("en"));
}

void MainWindow::refreshState() {
    const bool has = canvas_->hasImage();
    for (QAction* a : {toolSelect_, toolSample_, toolRegion_, toolAdd_}) a->setEnabled(has);
    fitAction_->setEnabled(has);
    emptyCard_->setVisible(!has);
    centerEmptyCard();
    refreshHint();
}

void MainWindow::refreshHint() {
    if (!canvas_->hasImage()) {
        messageBar_->setHint(QString());
        return;
    }
    if (dragging_ && dragTool_ == ToolId::Sample) messageBar_->setHint(qtTrId("ui.sample.hint"));
    else if (dragging_ && dragTool_ == ToolId::Region) messageBar_->setHint(qtTrId("ui.scan.hint"));
    else messageBar_->setHint(qtTrId("ui.idle.hint"));
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
    const LoadedImage loaded = loadImageFile(path);
    const QString name = QFileInfo(path).fileName();
    if (!loaded.ok()) {
        messageBar_->showCard(qtTrId("ui.error.imageOpen.title"),
                              uiText("ui.error.imageOpen.body", {{"name", name}}), {}, {qtTrId("action.dismiss")});
        return false;
    }
    setImage(loaded.image, name);
    return true;
}

void MainWindow::setImage(const QImage& image, const QString& name) {
    messageBar_->clearCard();
    imageName_ = name;
    canvas_->setImage(image);
    setWindowTitle(QStringLiteral("%1 — %2").arg(name, qtTrId("app.title")));
    canvas_->setFocus(Qt::OtherFocusReason);
}

// ---- 事件 ----------------------------------------------------------------------------------------------

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    QMainWindow::changeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile() && isImageFile(url.toLocalFile())) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile() && isImageFile(url.toLocalFile())) {
            openImage(url.toLocalFile());
            event->acceptProposedAction();
            return;
        }
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == canvas_->viewport() && event->type() == QEvent::Resize) centerEmptyCard();
    return QMainWindow::eventFilter(watched, event);
}

}  // namespace aoi::ui
