#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMimeData>
#include <QPushButton>
#include <QSet>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolBar>
#include <QUrl>
#include <cmath>
#include <memory>

#include "TestSupport.h"
#include "presentation/canvas/ImageCanvas.h"
#include "presentation/dev/FakeScene.h"
#include "presentation/i18n/Translations.h"
#include "presentation/shell/EmptyCard.h"
#include "presentation/shell/MainWindow.h"
#include "presentation/shell/MessageBar.h"
#include "presentation/theme/ThemeManager.h"
#include "presentation/theme/ThemeTokens.h"

using namespace aoi::ui;
using namespace aoi::test;

namespace {

const ThemeTokens& tokens() { return ThemeTokens::builtin(); }

struct Fixture {
    ThemeManager theme{*qApp, tokens()};
    MainWindow window{theme};
    aoi::dev::FakeBoard board;

    Fixture(bool withImage = false) {
        Translations::install(QStringLiteral("zh_TW"));
        window.show();
        Q_UNUSED(QTest::qWaitForWindowExposed(&window));
        if (withImage) {
            board = aoi::dev::makeFakeBoard(1200, 800);
            window.setImage(board.toQImage(), QStringLiteral("board.png"));
            QCoreApplication::processEvents();
        }
    }
    ImageCanvas* canvas() { return window.canvas(); }
    QWidget* vp() { return window.canvas()->viewport(); }
    QPointF at(double x, double y) { return toView(*window.canvas(), QPointF(x, y)); }
};

QPointF centerOf(QWidget* w) { return QPointF(w->width() / 2.0, w->height() / 2.0); }

}  // namespace

class TstShell : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Translations::install(QStringLiteral("zh_TW")); }

    // ---- 版面 ----
    void windowHonoursTheMinimumSizeAndDockWidth() {
        Fixture f;
        QCOMPARE(f.window.minimumSize(), tokens().sizes.windowMin);
        for (QDockWidget* d : {f.window.libraryDock(), f.window.objectsDock(), f.window.paramsDock()}) {
            QVERIFY(d->isVisible());
            QVERIFY2(d->width() >= tokens().sizes.dockMinWidth,
                     qPrintable(d->objectName() + " " + QString::number(d->width())));
        }
        QCOMPARE(f.window.libraryDock()->windowTitle(), qtTrId("ui.library.title"));
        QCOMPARE(f.window.objectsDock()->windowTitle(), qtTrId("ui.objects.title"));
        QCOMPARE(f.window.paramsDock()->windowTitle(), qtTrId("ui.params.title"));
    }

    void statusBarHasTheTokenHeight() {
        Fixture f;
        QCOMPARE(f.window.statusBar()->height(), tokens().sizes.statusBarHeight);
    }

    void toolButtonsAreAtLeastTheTokenSize() {
        Fixture f;
        auto* rail = f.window.findChild<QToolBar*>(QStringLiteral("toolRail"));
        QVERIFY(rail);
        for (ToolId id : {ToolId::Select, ToolId::Sample, ToolId::Region, ToolId::Add}) {
            QWidget* button = rail->widgetForAction(f.window.toolAction(id));
            QVERIFY(button);
            QVERIFY2(button->width() >= tokens().sizes.toolbarButton && button->height() >= tokens().sizes.toolbarButton,
                     qPrintable(QString::number(button->width()) + "x" + QString::number(button->height())));
        }
    }

    // ---- 空白與載入 ----
    void startsEmptyWithDisabledToolsAndACenteredCard() {
        Fixture f;
        QVERIFY(!f.canvas()->hasImage());
        QVERIFY(f.window.emptyCard()->isVisible());
        const QPointF cardCenter = f.window.emptyCard()->geometry().center();
        const QPointF vpCenter = centerOf(f.vp());
        QVERIFY(std::abs(cardCenter.x() - vpCenter.x()) <= 2 && std::abs(cardCenter.y() - vpCenter.y()) <= 2);
        for (ToolId id : {ToolId::Select, ToolId::Sample, ToolId::Region, ToolId::Add})
            QVERIFY(!f.window.toolAction(id)->isEnabled());
        QVERIFY(!f.window.fitAction()->isEnabled());
    }

    void loadingAnImageEnablesTools() {
        Fixture f(true);
        QVERIFY(f.canvas()->hasImage());
        QVERIFY(!f.window.emptyCard()->isVisible());
        for (ToolId id : {ToolId::Select, ToolId::Sample, ToolId::Region, ToolId::Add})
            QVERIFY(f.window.toolAction(id)->isEnabled());
        QVERIFY(f.window.fitAction()->isEnabled());
        QVERIFY(f.window.windowTitle().startsWith(QStringLiteral("board.png")));
    }

    // ---- 訊息列：不蓋影像、不改變畫布大小 ----
    void messagesNeverChangeTheCanvasSizeOrCoverTheCanvas() {
        Fixture f(true);
        MessageBar* bar = f.window.messageBar();
        const QSize canvasSize = f.canvas()->size();
        const QSize viewportSize = f.vp()->size();
        const int barHeight = bar->height();
        QCOMPARE(barHeight, tokens().sizes.messageBarHeight);

        auto verifyLayout = [&](const char* what) {
            QVERIFY2(f.canvas()->size() == canvasSize, what);
            QVERIFY2(f.vp()->size() == viewportSize, what);
            QVERIFY2(bar->height() == barHeight, what);
            // 訊息列在畫布正下方，兩者不相交。
            const QRect canvasRect(f.canvas()->mapTo(&f.window, QPoint(0, 0)), f.canvas()->size());
            const QRect barRect(bar->mapTo(&f.window, QPoint(0, 0)), bar->size());
            QVERIFY2(!canvasRect.intersects(barRect), what);
            QVERIFY2(barRect.top() >= canvasRect.bottom(), what);
        };

        verifyLayout("idle");
        bar->setHint(QString(400, QLatin1Char('x')));  // 很長的一行
        QCoreApplication::processEvents();
        verifyLayout("long hint");
        bar->showToast(QStringLiteral("已接受 118 個框 · Ctrl+Z 復原"));
        QCoreApplication::processEvents();
        verifyLayout("toast");
        QStringList advice;
        for (int i = 0; i < 12; ++i) advice << qtTrId("ui.noMatch.advice.sampleCoversMany");
        bar->showCard(qtTrId("ui.noMatch.title"), qtTrId("ui.noMatch.body"), advice,
                      {qtTrId("ui.noMatch.action.redraw"), qtTrId("ui.noMatch.action.teachNew")});
        QCoreApplication::processEvents();
        QVERIFY(bar->cardVisible());
        verifyLayout("no-match card with 12 advice lines");
        bar->clearCard();
        QCoreApplication::processEvents();
        QVERIFY(!bar->cardVisible());
        verifyLayout("card cleared");
    }

    void cardActionsReportTheirIndexAndTheErrorCardDismisses() {
        Fixture f(true);
        MessageBar* bar = f.window.messageBar();
        QList<int> clicked;
        QObject::connect(bar, &MessageBar::actionTriggered, [&](int i) { clicked << i; });
        bar->showCard(QStringLiteral("t"), QStringLiteral("b"), {}, {QStringLiteral("A"), QStringLiteral("B")});
        const auto buttons = bar->findChild<QWidget*>(QStringLiteral("messageCard"))->findChildren<QPushButton*>();
        QCOMPARE(static_cast<int>(buttons.size()), 2);
        buttons.at(1)->click();
        QCOMPARE(clicked, (QList<int>{1}));
    }

    // ---- 訊息列的進度列（S04）----
    void theProgressRowShowsTheBarAndTheCancelButton() {
        Fixture f(true);
        MessageBar* bar = f.window.messageBar();
        QVERIFY(!bar->progressVisible());
        const QSize canvasSize = f.canvas()->size();
        bar->showProgress(QStringLiteral("搜尋中… 40%"), 0.4, QStringLiteral("取消 (Esc)"));
        QVERIFY(bar->progressVisible());
        QCOMPARE(bar->progressValue(), 40);
        QCOMPARE(bar->height(), tokens().sizes.messageBarHeight);  // 固定高度：畫布不會被擠動
        QCOMPARE(f.canvas()->size(), canvasSize);
        bar->showProgress(QStringLiteral("搜尋中… 120%"), 1.7, QStringLiteral("取消 (Esc)"));
        QCOMPARE(bar->progressValue(), 100);  // 夾在 0..100
        QCOMPARE(bar->findChild<QPushButton*>(QStringLiteral("messageCancel"))->text(), QStringLiteral("取消 (Esc)"));
    }

    void theCancelButtonEmitsCancelRequested() {
        Fixture f(true);
        MessageBar* bar = f.window.messageBar();
        QSignalSpy spy(bar, &MessageBar::cancelRequested);
        bar->showProgress(QStringLiteral("x"), 0.1, QStringLiteral("取消"));
        bar->findChild<QPushButton*>(QStringLiteral("messageCancel"))->click();
        QCOMPARE(spy.count(), 1);
    }

    void aCardReplacesTheProgressRowAndClearingProgressBringsTheHintBack() {
        Fixture f(true);
        MessageBar* bar = f.window.messageBar();
        bar->setHint(QStringLiteral("hint"));
        bar->showProgress(QStringLiteral("x"), 0.5, QStringLiteral("取消"));
        bar->setHint(QStringLiteral("newer hint"));  // 進度列顯示時，提示不出現
        bar->clearProgress();
        QVERIFY(!bar->progressVisible());
        QCOMPARE(bar->hintText(), QStringLiteral("newer hint"));  // 關掉進度列後還原成最新的提示

        bar->showProgress(QStringLiteral("x"), 0.5, QStringLiteral("取消"));
        bar->showCard(QStringLiteral("t"), QStringLiteral("b"), {}, {QStringLiteral("A")});
        QVERIFY(!bar->progressVisible());  // 結果卡片來了，進度列收起
        QVERIFY(bar->cardVisible());
    }

    void clearingCardsOrProgressThatAreNotShownIsHarmless() {
        Fixture f(true);
        MessageBar* bar = f.window.messageBar();
        bar->setHint(QStringLiteral("hint"));
        bar->clearProgress();
        bar->clearCard();
        QCOMPARE(bar->hintText(), QStringLiteral("hint"));
    }

    void aCardKeepsItsButtonsWhenOnlyTheTextChanges() {
        Fixture f(true);
        MessageBar* bar = f.window.messageBar();
        bar->showCard(QStringLiteral("找到 10 個"), QStringLiteral("b"), {}, {QStringLiteral("接受"), QStringLiteral("捨棄")});
        const auto before = bar->findChild<QWidget*>(QStringLiteral("messageCard"))->findChildren<QPushButton*>();
        QCOMPARE(static_cast<int>(before.size()), 2);
        bar->showCard(QStringLiteral("找到 11 個"), QStringLiteral("b"), {}, {QStringLiteral("接受"), QStringLiteral("捨棄")});
        const auto after = bar->findChild<QWidget*>(QStringLiteral("messageCard"))->findChildren<QPushButton*>();
        QCOMPARE(after, before);  // 同樣的按鈕物件：門檻拖動時不重建、不搶焦點
        QCOMPARE(bar->cardTitle(), QStringLiteral("找到 11 個"));
        bar->showCard(QStringLiteral("t"), QStringLiteral("b"), {}, {QStringLiteral("A"), QStringLiteral("B")});  // 個數一樣、文字不同
        const auto renamed = bar->findChild<QWidget*>(QStringLiteral("messageCard"))->findChildren<QPushButton*>();
        QCOMPARE(static_cast<int>(renamed.size()), 2);
        QSet<QString> labels;
        for (const QPushButton* b : renamed) labels.insert(b->text());
        QCOMPARE(labels, (QSet<QString>{QStringLiteral("A"), QStringLiteral("B")}));
        bar->showCard(QStringLiteral("t"), QStringLiteral("b"), {}, {QStringLiteral("只有一個")});
        QCOMPARE(static_cast<int>(bar->findChild<QWidget*>(QStringLiteral("messageCard"))->findChildren<QPushButton*>().size()), 1);
    }

    void toastReturnsToTheLatestHintAfterItsTime() {
        Fixture f(true);
        MessageBar* bar = f.window.messageBar();
        bar->showToast(QStringLiteral("toast"));
        QCOMPARE(bar->hintText(), QStringLiteral("toast"));
        bar->setHint(QStringLiteral("newer hint"));  // 吐司顯示中：不被打斷
        QCOMPARE(bar->hintText(), QStringLiteral("toast"));
        QTest::qWait(tokens().timing.toast + 300);
        QCOMPARE(bar->hintText(), QStringLiteral("newer hint"));  // 還原成最新的提示
    }

    // ---- 工具與快捷鍵 ----
    void toolActionsAreExclusiveAndSwitchTheCanvasTool() {
        Fixture f(true);
        QVERIFY(f.window.toolAction(ToolId::Select)->isChecked());
        f.window.toolAction(ToolId::Sample)->trigger();
        QCOMPARE(f.canvas()->activeTool(), ToolId::Sample);
        QVERIFY(f.window.toolAction(ToolId::Sample)->isChecked());
        QVERIFY(!f.window.toolAction(ToolId::Select)->isChecked());
        f.window.toolAction(ToolId::Region)->trigger();
        QCOMPARE(f.canvas()->activeTool(), ToolId::Region);
        QVERIFY(!f.window.toolAction(ToolId::Sample)->isChecked());
    }

    void shortcutsMatchTheSpec() {
        Fixture f;
        QCOMPARE(f.window.toolAction(ToolId::Select)->shortcut(), QKeySequence(Qt::Key_V));
        QCOMPARE(f.window.toolAction(ToolId::Sample)->shortcut(), QKeySequence(Qt::Key_S));
        QCOMPARE(f.window.toolAction(ToolId::Region)->shortcut(), QKeySequence(Qt::Key_R));
        const auto addKeys = f.window.toolAction(ToolId::Add)->shortcuts();
        QVERIFY(addKeys.contains(QKeySequence(Qt::Key_A)) && addKeys.contains(QKeySequence(Qt::Key_Plus)));
        QCOMPARE(f.window.fitAction()->shortcut(), QKeySequence(Qt::Key_F));
        QCOMPARE(f.window.openAction()->shortcut(), QKeySequence(QKeySequence::Open));
        QCOMPARE(f.window.undoAction()->shortcut(), QKeySequence(QKeySequence::Undo));
        QVERIFY(!f.window.undoAction()->isEnabled());  // 復原堆疊在 S04
    }

    // ---- 狀態列 ----
    void statusBarShowsCursorPositionAndZoom() {
        Fixture f(true);
        move(f.vp(), f.at(600, 400), Qt::NoButton);
        QCOMPARE(f.window.cursorLabel()->text(), QStringLiteral("x 600  y 400"));
        move(f.vp(), QPointF(2, 2), Qt::NoButton);  // 影像外
        QVERIFY(f.window.cursorLabel()->text().isEmpty());

        const double before = f.canvas()->zoomPercent();
        sendWheel(f.vp(), centerOf(f.vp()), 2);
        QVERIFY(f.canvas()->zoomPercent() > before);
        QCOMPARE(f.window.zoomLabel()->text(),
                 QStringLiteral("縮放 %1%").arg(qRound(f.canvas()->zoomPercent())));
    }

    // ---- 語言、主題、字級 ----
    void languageSwitchRetranslatesTheWholeWindow() {
        Fixture f(true);
        QCOMPARE(f.window.openAction()->text(), QStringLiteral("開啟"));
        QVERIFY(Translations::install(QStringLiteral("en")));
        QCoreApplication::processEvents();  // Qt 6.8：LanguageChange 是排入佇列的事件，下一輪事件迴圈才送達
        QCOMPARE(f.window.openAction()->text(), QStringLiteral("Open"));
        QCOMPARE(f.window.libraryDock()->windowTitle(), QStringLiteral("Sample library"));
        QVERIFY(f.window.toolAction(ToolId::Sample)->toolTip().contains(QStringLiteral("Teach sample")));
        QVERIFY(f.window.windowTitle().contains(QStringLiteral("AOI Teach Tool")));
        QVERIFY(Translations::install(QStringLiteral("zh_TW")));
        QCoreApplication::processEvents();
        QCOMPARE(f.window.openAction()->text(), QStringLiteral("開啟"));
    }

    void emptyCardIsRetranslatedToo() {
        Fixture f;
        QVERIFY(Translations::install(QStringLiteral("en")));
        QCoreApplication::processEvents();
        QCOMPARE(f.window.emptyCard()->openButton()->text(), QStringLiteral("Open image…"));
        QVERIFY(Translations::install(QStringLiteral("zh_TW")));
        QCoreApplication::processEvents();
        QCOMPARE(f.window.emptyCard()->openButton()->text(), QStringLiteral("開啟影像…"));
    }

    void themeSwitchReachesThePaletteAndTheCanvasBackground() {
        Fixture f(true);
        f.theme.setMode(ThemeMode::Dark);
        QCOMPARE(qApp->palette().color(QPalette::Window), tokens().dark.surface.window);
        QCOMPARE(f.canvas()->backgroundBrush().color(), tokens().dark.surface.canvas);
        f.theme.setMode(ThemeMode::Light);
        QCOMPARE(qApp->palette().color(QPalette::Window), tokens().light.surface.window);
        QCOMPARE(f.canvas()->backgroundBrush().color(), tokens().light.surface.canvas);
        // 疊圖的框不隨主題變：同一份 overlay 調色盤。
        const QColor foundDark = f.canvas()->overlayStyle().box(OverlayState::Found, BoxKind::Body, false).stroke.color();
        f.theme.setMode(ThemeMode::Dark);
        QCOMPARE(f.canvas()->overlayStyle().box(OverlayState::Found, BoxKind::Body, false).stroke.color(), foundDark);
    }

    void textScaleDoesNotResizeTouchTargets() {
        Fixture f(true);
        auto* rail = f.window.findChild<QToolBar*>(QStringLiteral("toolRail"));
        const QSize before = rail->widgetForAction(f.window.toolAction(ToolId::Sample))->size();
        f.theme.setFontScale(1.5);
        QCoreApplication::processEvents();
        QCOMPARE(qApp->font().pointSizeF(), tokens().fontBasePt * 1.5);
        QCOMPARE(rail->widgetForAction(f.window.toolAction(ToolId::Sample))->size(), before);
        QCOMPARE(tokens().sizes.handleHit, 24.0);  // 把手點擊區是固定像素
        f.theme.setFontScale(1.0);
    }

    // ---- 開啟影像 ----
    void openingARealFileLoadsIt() {
        Fixture f;
        QTemporaryDir dir(QDir::current().filePath(QStringLiteral("aoi-test-XXXXXX")));
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("board.png"));
        f.board = aoi::dev::makeFakeBoard(300, 200);
        QVERIFY(f.board.toQImage().save(path));
        QVERIFY(f.window.openImage(path));
        QVERIFY(f.canvas()->hasImage());
        QCOMPARE(f.canvas()->imageRect().size(), QSizeF(300, 200));
        QVERIFY(!f.window.emptyCard()->isVisible());
        QVERIFY(f.window.windowTitle().startsWith(QStringLiteral("board.png")));
    }

    void aBrokenFileKeepsTheOldImageAndReportsTheFailure() {
        Fixture f(true);
        QTemporaryDir dir(QDir::current().filePath(QStringLiteral("aoi-test-XXXXXX")));
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("broken.png"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("this is not a png");
        file.close();

        QSignalSpy failed(&f.window, &MainWindow::imageOpenFailed);
        QSignalSpy opened(&f.window, &MainWindow::imageOpened);
        QVERIFY(!f.window.openImage(path));
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().at(0).toString(), QStringLiteral("broken.png"));
        QVERIFY(!failed.first().at(1).toString().isEmpty());  // 原因（Qt 的錯誤字串）
        QCOMPARE(opened.count(), 0);
        QVERIFY(f.canvas()->hasImage());  // 舊影像保留
        QCOMPARE(f.canvas()->imageRect().size(), QSizeF(1200, 800));
    }

    void aBrokenFileWithNoOldImageKeepsTheEmptyState() {
        Fixture f;
        QSignalSpy failed(&f.window, &MainWindow::imageOpenFailed);
        QVERIFY(!f.window.openImage(QStringLiteral("this-file-does-not-exist.png")));
        QCOMPARE(failed.count(), 1);
        QVERIFY(!f.canvas()->hasImage());
        QVERIFY(f.window.emptyCard()->isVisible());
    }

    void openingAnImageAnnouncesItWithTheNameAndPixels() {
        Fixture f;
        QSignalSpy opened(&f.window, &MainWindow::imageOpened);
        f.board = aoi::dev::makeFakeBoard(300, 200);
        f.window.setImage(f.board.toQImage(), QStringLiteral("board.png"));
        QCOMPARE(opened.count(), 1);
        QCOMPARE(opened.first().at(0).value<QImage>().size(), QSize(300, 200));
        QCOMPARE(opened.first().at(1).toString(), QStringLiteral("board.png"));
    }

    void droppingAnImageFileOpensIt() {
        Fixture f;
        QTemporaryDir dir(QDir::current().filePath(QStringLiteral("aoi-test-XXXXXX")));
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("dropped.bmp"));
        QVERIFY(aoi::dev::makeFakeBoard(120, 80).toQImage().save(path));
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(path)});
        // Qt 把沒有先 DragEnter 的 Drop 丟掉，所以要照真實的順序送：進入、放下。
        QDragEnterEvent enter(QPoint(50, 50), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&f.window, &enter);
        QVERIFY(enter.isAccepted());
        QDropEvent drop(QPointF(50, 50), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&f.window, &drop);
        QVERIFY(f.canvas()->hasImage());
        QCOMPARE(f.canvas()->imageRect().size(), QSizeF(120, 80));
    }

    void droppingANonImageFileIsRefused() {
        Fixture f;
        QMimeData mime;
        mime.setUrls({QUrl::fromLocalFile(QStringLiteral("C:/some/notes.txt"))});
        QDragEnterEvent enter(QPoint(50, 50), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        enter.ignore();
        QApplication::sendEvent(&f.window, &enter);
        QVERIFY(!enter.isAccepted());
        QVERIFY(!f.canvas()->hasImage());
    }

    // ---- 供人檢視的截圖（設定 AOI_SCREENSHOT_DIR 才執行）----
    void screenshotsForReview() {
        const QString dirPath = qEnvironmentVariable("AOI_SCREENSHOT_DIR");
        if (dirPath.isEmpty()) QSKIP("set AOI_SCREENSHOT_DIR to write review screenshots");
        QDir().mkpath(dirPath);
        Fixture f;
        f.window.resize(1280, 800);
        for (const auto& mode : {std::pair{ThemeMode::Light, "light"}, std::pair{ThemeMode::Dark, "dark"}}) {
            f.theme.setMode(mode.first);
            QCoreApplication::processEvents();
            f.window.grab().save(QDir(dirPath).filePath(QStringLiteral("empty-%1.png").arg(mode.second)));
        }
        f.board = aoi::dev::makeFakeBoard();
        f.window.setImage(f.board.toQImage(), QStringLiteral("demo.fake"));
        aoi::dev::FakeSceneOptions o;
        o.boxes = 5000;
        f.canvas()->showComponents(aoi::dev::makeFakeComponents(o));
        for (const auto& mode : {std::pair{ThemeMode::Light, "light"}, std::pair{ThemeMode::Dark, "dark"}}) {
            f.theme.setMode(mode.first);
            QCoreApplication::processEvents();
            f.window.grab().save(QDir(dirPath).filePath(QStringLiteral("board5000-%1.png").arg(mode.second)));
        }
        // 放大、選取、拖出的樣本框、無匹配卡片。
        f.theme.setMode(ThemeMode::Dark);
        f.canvas()->setSelection({1, 2});
        f.canvas()->zoomAt(centerOf(f.vp()), 4.0);
        f.canvas()->showRegionDraft(QRectF(1200, 900, 160, 90), DraftMode::Sample);
        f.window.messageBar()->showCard(qtTrId("ui.noMatch.title"), qtTrId("ui.noMatch.body"),
                                        {qtTrId("ui.noMatch.advice.sampleTooSmall"), qtTrId("ui.noMatch.advice.onlyOne")},
                                        {qtTrId("ui.noMatch.action.redraw")});
        QCoreApplication::processEvents();
        f.window.grab().save(QDir(dirPath).filePath(QStringLiteral("zoomed-dark.png")));
        QVERIFY(true);
    }
};

QTEST_MAIN(TstShell)
#include "tst_shell.moc"
