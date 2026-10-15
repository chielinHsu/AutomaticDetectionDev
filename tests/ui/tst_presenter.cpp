// Presenter 的整合測試：使用者的操作（滑鼠、鍵盤、按鈕）進去，畫面上的結果出來。
// 用產品的接線（core → MainWindow → AppPresenters）加上假 back end，在 offscreen 平台跑真的 QThreadPool。
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QListWidget>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <QtTest>

#include "PresenterRig.h"
#include "domain/image_buffer.h"
#include "domain/image_depth.h"
#include "presentation/library/SamplePanel.h"
#include "presentation/params/ParamsPanel.h"
#include "QtTestPrinters.h"

using namespace aoi;
using namespace aoi::ui;
using namespace aoi::test;
using S = InteractionState;

namespace {

QString zh(const char* id, TextArgs args = {}) {
    Translations::install(QStringLiteral("zh_TW"));
    return uiText(id, args);
}

// 16 bit 灰階、有紋理的影像，最大值 maxValue（S07 的有效位元數）。看最大值推測的位元數放在 significantBits，和影像來源一樣。
aoi::ImageView gray16Image(int w, int h, int maxValue) {
    aoi::ImageBuffer buffer(w, h, aoi::PixelFormat::Gray16);
    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<std::uint16_t*>(buffer.row(y));
        for (int x = 0; x < w; ++x) row[x] = static_cast<std::uint16_t>(((x / 8 + y / 8) % 2 ? maxValue : maxValue / 4) - (x + y) % 16);
    }
    aoi::ImageView view = buffer.view();
    view.significantBits = aoi::detectSignificantBits(view);
    return view;
}

}  // namespace

class TstPresenter : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Translations::install(QStringLiteral("zh_TW")); }
    void init() { Translations::install(QStringLiteral("zh_TW")); }

    // ---- 開始與影像 ----
    void startsEmptyWithTheEmptySummaryAndNoHint() {
        Rig r;
        QCOMPARE_STATE(r.state(), S::Empty);
        QCOMPARE(r.summary(), zh("ui.status.noObjects"));
        QCOMPARE(r.backendStatus(), QStringLiteral("Fake (test)"));  // back end 在狀態列最左邊，摘要不再重複
        QVERIFY(r.hint().isEmpty());
        QVERIFY(!r.window.toolAction(ToolId::Sample)->isEnabled());
        QVERIFY(!r.window.undoAction()->isEnabled());
        QVERIFY(!r.window.redoAction()->isEnabled());
    }

    void openingAnImageGoesIdleWithTheIdleHint() {
        Rig r;
        r.open();
        QCOMPARE_STATE(r.state(), S::Idle);
        QCOMPARE(r.hint(), zh("ui.idle.hint"));
        QVERIFY(r.window.objectsPanel()->emptyLabel()->isVisibleTo(r.window.objectsPanel()));
        QCOMPARE(r.document().board().imageWidth, 1200);
        QCOMPARE(r.document().board().name, std::string("board.png"));
    }

    // S06：開圖的訊號帶 ImageView；Presenter 用它的校正值（不再固定 0），而且拿到的是畫布畫的同一塊像素。
    void theOpenedImageCarriesItsCalibrationIntoTheBoard() {
        Rig r;
        r.board = aoi::dev::makeFakeBoard(640, 480);
        aoi::ImageView view = r.board.view();
        view.umPerPixel = 12.5;
        r.window.setImage(view, QStringLiteral("calibrated.png"));
        QCoreApplication::processEvents();
        QCOMPARE(r.document().board().umPerPixel, 12.5);
        QCOMPARE(r.document().board().imageWidth, 640);
        QVERIFY(r.canvas()->imageItem()->image().constBits() == view.data);
    }

    // ---- 影像的有效位元數（S07；使用者 2026-10-06 決定：存在板子裡、換影像沿用、參數面板的下拉選單）----
    void aDarkTwelveBitImageCanBeToldItsBitDepthAndTheNextImageKeepsIt() {
        Rig r;
        const aoi::ImageView dark = gray16Image(320, 240, 1000);  // 12 bit 相機的暗影像：看最大值會推測成 10 bit
        QCOMPARE(dark.significantBits, 10);
        r.window.setImage(dark, QStringLiteral("dark.tif"));
        QCoreApplication::processEvents();
        QComboBox* combo = r.window.paramsPanel()->bitsCombo();
        QVERIFY(combo->isEnabled());
        QCOMPARE(combo->itemText(0), zh("ui.params.bits.auto", {{"bits", QStringLiteral("10")}}));
        QCOMPARE(r.presenter().image().significantBits, 10);
        // 畫布顯示：推測的 10 bit 把 1000 畫成 1000 >> 2 ＝ 250（太亮）。
        QCOMPARE(qGray(r.canvas()->imageItem()->image().pixel(4, 12)), 250);  // 這個像素是 1000

        const int twelve = combo->findData(12);
        combo->setCurrentIndex(twelve);
        emit combo->activated(twelve);
        QCOMPARE(r.document().board().significantBits, 12);
        QCOMPARE(r.presenter().image().significantBits, 12);
        QVERIFY(r.presenter().image().data == dark.data);  // 只改欄位，不複製像素
        QCOMPARE(qGray(r.canvas()->imageItem()->image().pixel(4, 12)), 62);  // 1000 >> 4：相機真正的亮度
        QVERIFY(r.document().historyDepth() == 0);  // 是設定，不是一個可以復原的修改

        // 下一張影像（同一台相機）：沿用 12 bit。
        r.window.setImage(gray16Image(320, 240, 900), QStringLiteral("dark2.tif"));
        QCoreApplication::processEvents();
        QCOMPARE(r.document().board().significantBits, 12);
        QCOMPARE(r.presenter().image().significantBits, 12);
        QCOMPARE(combo->currentData().toInt(), 12);

        // 改回自動。
        emit combo->activated(0);
        QCOMPARE(r.document().board().significantBits, 0);
        QCOMPARE(r.presenter().image().significantBits, 10);
    }

    void aBitDepthBelowThePixelValuesIsRefused() {
        Rig r;
        r.window.setImage(gray16Image(320, 240, 4000), QStringLiteral("bright.tif"));  // 推測 12 bit
        QCoreApplication::processEvents();
        QComboBox* combo = r.window.paramsPanel()->bitsCombo();
        const int ten = combo->findData(10);
        combo->setCurrentIndex(ten);
        emit combo->activated(ten);
        QCOMPARE(r.document().board().significantBits, 0);  // 沒有收
        QCOMPARE(r.presenter().image().significantBits, 12);
        QCOMPARE(combo->currentIndex(), 0);                   // 選單回到原來的值
        QCOMPARE(r.hint(), zh("ui.params.bits.rejected", {{"requested", QStringLiteral("10")}, {"bits", QStringLiteral("12")}}));
    }

    void anEightBitImageHasNoBitDepthChoice() {
        Rig r;
        r.open(320, 240);
        QVERIFY(!r.window.paramsPanel()->bitsCombo()->isEnabled());
    }

    void aBrokenFileWithoutAnImageShowsAnErrorCardAndDismissesToEmpty() {
        Rig r;
        QVERIFY(!r.window.openImage(QStringLiteral("this-file-does-not-exist.png")));
        QCOMPARE_STATE(r.state(), S::Error);
        QVERIFY(r.window.messageBar()->cardVisible());
        QCOMPARE(r.window.messageBar()->cardTitle(), zh("ui.error.imageOpen.title"));
        QCOMPARE(r.cardButtons().size(), 1);  // 〔關閉〕
        r.cardButtons().first()->click();
        QCOMPARE_STATE(r.state(), S::Empty);
        QVERIFY(!r.window.messageBar()->cardVisible());
        QVERIFY(r.window.emptyCard()->isVisible());
    }

    void aBrokenFileWithAnImageKeepsTheImageAndDismissesToIdle() {
        Rig r;
        r.open();
        QVERIFY(!r.window.openImage(QStringLiteral("this-file-does-not-exist.png")));
        QCOMPARE_STATE(r.state(), S::Error);
        QVERIFY(r.canvas()->hasImage());
        r.cardButtons().first()->click();
        QCOMPARE_STATE(r.state(), S::Idle);
        QCOMPARE(r.canvas()->imageRect().size(), QSizeF(1200, 800));
    }

    // ---- 教樣本：框 → 搜尋 → 提案 ----
    void drawingASampleSearchesOffTheUiThreadAndShowsTheProposal() {
        Rig r;
        r.open();
        QSignalSpy applied(&r.presenter(), &AutoRoiPresenter::searchApplied);
        QThread* uiThread = QThread::currentThread();
        QThread* appliedOn = nullptr;
        QObject::connect(&r.presenter(), &AutoRoiPresenter::searchApplied, &r.presenter(),
                         [&] { appliedOn = QThread::currentThread(); });

        r.searchByDrag(QRectF(560, 360, 60, 32));
        QCOMPARE_STATE(r.state(), S::Searching);  // 按 F5 的瞬間就進入搜尋，UI 沒有被卡住
        QVERIFY(!r.canvas()->draftItem()->isVisible());  // 樣本是板子上的元件（框層會畫），沒有拖曳中的框（ADR-025）

        QVERIFY(r.waitState(S::Preview));
        QCOMPARE(applied.count(), 1);
        QVERIFY(appliedOn == uiThread);  // 結果是在 UI 執行緒套用的
        QVERIFY(r.probe->builds >= 1);

        QVERIFY(r.window.messageBar()->cardVisible());
        QCOMPARE(r.cardButtons().size(), 2);
        QVERIFY(r.cardButtons().at(0)->text().contains(QStringLiteral("Enter")));
        QVERIFY(r.cardButtons().at(1)->text().contains(QStringLiteral("Esc")));
        QVERIFY(r.componentCount() > 5);
        QCOMPARE(r.componentCount(), static_cast<int>(r.scene().snapshot->components.size()));
        QCOMPARE(r.treeRows(), r.componentCount() - 1);  // 樹列的是提案；畫布另外畫板子上的樣本
        // 狀態列的摘要要等「搜尋中…」收起來：搜尋超過忙碌門檻（測試台 40 ms；Debug 常接近）時，標示至少停留
        // busyMinShowMs 才換成摘要（theBusyLabelStaysForItsMinimumTimeAfterAQuickResult）。所以等最終的狀態，不讀當下。
        QTRY_VERIFY2(r.summary().contains(QStringLiteral("找到")), qPrintable(r.summary()));
        QCOMPARE(r.backendStatus(), QStringLiteral("Fake (test)"));
        QVERIFY(!r.canvas()->draftItem()->isVisible());  // 樣本畫在框層，拖曳中的框不留
        QVERIFY(r.window.objectsPanel()->model()->rows()->proposal);
    }

    void theProposalMatchesWhatTheUseCaseReturnsForTheSameInput() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample(QRectF(560, 360, 60, 32)));
        // 假世界：樣本左右各 4 格、上下各 2 格 → 9 × 5 ＝ 45 格；樣本自己是其中一格。
        const SceneCounts& c = r.scene().counts;
        QCOMPARE(c.components, std::size_t{44});  // 樣本自己已經是板子上的元件，不在提案裡（ADR-025）
        QCOMPARE(c.found + c.inferred + c.rejected, c.components);
        QVERIFY(c.found >= 2);
        QVERIFY(c.rejected >= 1);  // 約 20% 分數低
        QVERIFY(c.inferred >= 1);  // 約 10% 缺件
    }

    void aTooSmallSampleSaysSoAndNeverSearches() {
        Rig r;
        r.open();
        r.drag(ToolId::Sample, QRectF(560, 360, 3, 2));
        QCOMPARE_STATE(r.state(), S::Idle);
        QCOMPARE(r.hint(), zh("ui.sample.tooSmall", {{"min", "8"}}));
        QVERIFY(!r.canvas()->draftItem()->isVisible());
        QTest::qWait(50);
        QCOMPARE(r.probe->builds.load(), 0);
    }

    void thePointerPressStartsTheDrawingStateAndTheHintFollowsIt() {
        Rig r;
        r.open();
        r.window.toolAction(ToolId::Sample)->trigger();
        press(r.vp(), r.at(300, 300));
        QCOMPARE_STATE(r.state(), S::DrawingRegion);
        QCOMPARE(r.hint(), zh("ui.sample.hint"));
        sendKey(r.canvas(), Qt::Key_Escape);  // 畫框中的 Esc 歸工具：放棄
        release(r.vp(), r.at(300, 300));
        QCOMPARE_STATE(r.state(), S::Idle);
        QCOMPARE(r.hint(), zh("ui.idle.hint"));

        r.window.toolAction(ToolId::Region)->trigger();
        press(r.vp(), r.at(300, 300));
        QCOMPARE(r.hint(), zh("ui.scan.hint"));
        release(r.vp(), r.at(302, 301));
    }

    void escWhileDrawingOverAProposalKeepsTheProposalAndItsRegion() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        const int before = r.componentCount();
        r.window.toolAction(ToolId::Sample)->trigger();
        press(r.vp(), r.at(100, 100));
        QCOMPARE_STATE(r.state(), S::DrawingRegion);
        sendKey(r.canvas(), Qt::Key_Escape);
        release(r.vp(), r.at(100, 100));
        QCOMPARE_STATE(r.state(), S::Preview);
        QCOMPARE(r.componentCount(), before);
        QVERIFY(!r.canvas()->draftItem()->isVisible());
        QVERIFY(r.window.messageBar()->cardVisible());
    }

    // ---- 接受、捨棄、復原 ----
    void enterAcceptsAndTheComponentsAppearOnTheBoard() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        const std::size_t expected = r.scene().counts.found + r.scene().counts.inferred;
        r.key(Qt::Key_Return);
        QCOMPARE_STATE(r.state(), S::Idle);
        QCOMPARE(r.document().components().size(), expected + 1);  // 加上樣本
        QCOMPARE(r.componentCount(), static_cast<int>(expected + 1));
        QVERIFY(!r.window.messageBar()->cardVisible());
        QVERIFY(r.hint().contains(QStringLiteral("已接受")));
        QVERIFY(r.hint().contains(QString::number(expected)));
        QVERIFY(!r.canvas()->draftItem()->isVisible());
        QVERIFY(!r.window.objectsPanel()->model()->rows()->proposal);
        QVERIFY(r.window.undoAction()->isEnabled());
        QVERIFY(r.window.undoAction()->toolTip().contains(QStringLiteral("接受")));
        // 接受之後 id 不加偏移：畫布上的編號就是文件的元件編號
        QCOMPARE(r.scene().snapshot->components.front().id, r.document().components().front().id);
    }

    void theAcceptButtonDoesTheSameAsEnter() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.cardButtons().at(0)->click();
        QCOMPARE_STATE(r.state(), S::Idle);
        QVERIFY(!r.document().components().empty());
    }

    void undoAfterAcceptGoesBackToPreviewAndRedoAcceptsAgain() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        const int proposalSize = r.componentCount();
        r.key(Qt::Key_Return);
        const std::size_t accepted = r.document().components().size();

        r.window.undoAction()->trigger();
        QCOMPARE_STATE(r.state(), S::Preview);       // UndoToPreview
        QCOMPARE(r.document().components().size(), std::size_t{1});  // 只剩樣本
        QCOMPARE(r.componentCount(), proposalSize);  // 提案回來了
        QVERIFY(r.window.messageBar()->cardVisible());
        QVERIFY(!r.canvas()->draftItem()->isVisible());
        QVERIFY(r.window.redoAction()->isEnabled());

        r.window.redoAction()->trigger();
        QCOMPARE_STATE(r.state(), S::Idle);
        QCOMPARE(r.document().components().size(), accepted);
    }

    void escDiscardsTheProposalAndUndoBringsItBack() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        const int proposalSize = r.componentCount();
        r.key(Qt::Key_Escape);
        QCOMPARE_STATE(r.state(), S::Idle);
        QCOMPARE(r.componentCount(), 1);  // 只剩樣本
        QVERIFY(r.hint().contains(QStringLiteral("已捨棄")));
        QCOMPARE(r.document().components().size(), std::size_t{1});

        r.window.undoAction()->trigger();
        QCOMPARE_STATE(r.state(), S::Preview);
        QCOMPARE(r.componentCount(), proposalSize);
    }

    void theDiscardButtonDiscards() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.cardButtons().at(1)->click();
        QCOMPARE_STATE(r.state(), S::Idle);
    }

    void undoInPreviewOnlyTouchesTheProposalNotTheBoard() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.key(Qt::Key_Return);
        r.window.undoAction()->trigger();  // → Preview
        const std::size_t depth = r.document().historyDepth();
        QVERIFY(!r.window.undoAction()->isEnabled());  // 提案堆疊是空的，多餘的復原不可按
        r.window.undoAction()->trigger();
        QCOMPARE(r.document().historyDepth(), depth);
        QCOMPARE_STATE(r.state(), S::Preview);
    }

    // ---- 選取與編輯 ----
    void clickingAComponentSelectsItOnTheCanvasAndInTheTree() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.clickComponent(3);
        const int sceneId = r.scene().snapshot->components[3].id;
        QCOMPARE(r.canvas()->selection(), (std::vector<int>{sceneId}));
        QCOMPARE(r.window.objectsPanel()->selectedIds(), (std::vector<int>{sceneId}));
        QCOMPARE(r.presenter().session().selection(), (std::vector<int>{sceneId - kProposalIdBase}));
    }

    void selectingInTheTreeSelectsOnTheCanvas() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        const int sceneId = r.scene().snapshot->components[5].id;
        ObjectsPanel* panel = r.window.objectsPanel();
        const QModelIndex index = panel->model()->indexForId(sceneId);
        QVERIFY(index.isValid());
        panel->view()->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        QCOMPARE(r.canvas()->selection(), (std::vector<int>{sceneId}));
        QCOMPARE(r.presenter().session().selection(), (std::vector<int>{sceneId - kProposalIdBase}));
    }

    void programmaticSelectionDoesNotEchoBackAsAUserOperation() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        QSignalSpy user(r.window.objectsPanel(), &ObjectsPanel::selectionChangedByUser);
        r.clickComponent(2);  // 畫布 → Presenter → 樹：樹不可以再回報一次
        QCOMPARE(user.count(), 0);
    }

    void shiftClickAddsAndRemovesFromTheSelection() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.clickComponent(2);
        r.clickComponent(4, Qt::ShiftModifier);
        QCOMPARE(r.canvas()->selection().size(), std::size_t{2});
        r.clickComponent(2, Qt::ShiftModifier);
        QCOMPARE(r.canvas()->selection().size(), std::size_t{1});
    }

    void escWithASelectionClearsTheSelectionBeforeDiscarding() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.clickComponent(3);
        r.key(Qt::Key_Escape);
        QCOMPARE_STATE(r.state(), S::Preview);  // 第一下只清除選取
        QVERIFY(r.canvas()->selection().empty());
        r.key(Qt::Key_Escape);
        QCOMPARE_STATE(r.state(), S::Idle);     // 第二下才捨棄
    }

    void deleteInPreviewRemovesTheSelectedProposalObjectAndUndoRestoresIt() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        const int before = r.componentCount();
        r.clickComponent(3);
        r.key(Qt::Key_Delete);
        QCOMPARE(r.componentCount(), before - 1);
        QCOMPARE(r.treeRows(), before - 2);  // 樹只列提案（畫布的數量含板子上的樣本）
        QCOMPARE(r.document().components().size(), std::size_t{1});
        QVERIFY(r.window.undoAction()->isEnabled());
        r.window.undoAction()->trigger();
        QCOMPARE(r.componentCount(), before);
        QCOMPARE_STATE(r.state(), S::Preview);
    }

    void deleteOnTheBoardRemovesComponentsAndUndoRestoresThem() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.key(Qt::Key_Return);
        const std::size_t before = r.document().components().size();
        r.clickComponent(2);
        r.clickComponent(5, Qt::ShiftModifier);
        r.key(Qt::Key_Delete);
        QCOMPARE(r.document().components().size(), before - 2);
        QCOMPARE(r.componentCount(), static_cast<int>(before) - 2);
        QVERIFY(r.window.undoAction()->toolTip().contains(QStringLiteral("刪除 2 個元件")));
        r.window.undoAction()->trigger();
        QCOMPARE(r.document().components().size(), before);
        QCOMPARE_STATE(r.state(), S::Idle);  // 復原的是刪除，不是接受
    }

    void arrowKeysNudgeTheSelectionAndConsecutiveNudgesAreOneStep() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.key(Qt::Key_Return);
        r.clickComponent(2);
        const int id = r.presenter().session().selection().front();
        const double x0 = r.document().find(id)->pose.x;
        const std::size_t depth = r.document().historyDepth();
        r.key(Qt::Key_Right);
        r.key(Qt::Key_Right);
        r.key(Qt::Key_Right, Qt::ShiftModifier);  // Shift ＝ 10 px
        QCOMPARE(r.document().find(id)->pose.x, x0 + 12);
        QCOMPARE(r.document().historyDepth(), depth + 1);  // 三次微調，一步
        r.window.undoAction()->trigger();
        QCOMPARE(r.document().find(id)->pose.x, x0);
    }

    void draggingAComponentOnTheBoardMovesItAsOneCommand() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.key(Qt::Key_Return);
        r.window.toolAction(ToolId::Select)->trigger();
        const QPointF c = r.centerOfComponent(2);
        const int id = r.scene().snapshot->components[2].id;
        const Pose2D before = r.document().find(id)->pose;
        const std::size_t depth = r.document().historyDepth();

        press(r.vp(), r.at(c.x(), c.y()));
        move(r.vp(), r.at(c.x() + 20, c.y() + 10));
        QVERIFY(r.canvas()->draftItem()->isVisible());  // 拖曳中：放開後會落在哪裡的外框
        move(r.vp(), r.at(c.x() + 60, c.y() + 40));
        release(r.vp(), r.at(c.x() + 60, c.y() + 40));

        QVERIFY(std::abs(r.document().find(id)->pose.x - (before.x + 60)) < 0.6);
        QVERIFY(std::abs(r.document().find(id)->pose.y - (before.y + 40)) < 0.6);
        QCOMPARE(r.document().historyDepth(), depth + 1);  // 一次拖曳 ＝ 一個 Command，不是每個滑鼠移動一個
        QVERIFY(!r.canvas()->draftItem()->isVisible());
        r.window.undoAction()->trigger();
        QCOMPARE(r.document().find(id)->pose.x, before.x);  // 精確還原
    }

    void draggingAProposalObjectMovesOnlyTheProposalAndLeavesTheBoardAlone() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        const std::size_t boardBefore = r.document().components().size();  // 只有樣本
        const std::uint64_t revision = r.document().revision();
        r.window.toolAction(ToolId::Select)->trigger();
        const QPointF c = r.centerOfComponent(3);  // 索引 0 是板子上的樣本，之後是提案物件
        press(r.vp(), r.at(c.x(), c.y()));
        move(r.vp(), r.at(c.x() + 30, c.y() + 20));
        QVERIFY(r.canvas()->draftItem()->isVisible());  // 拖曳中：放開後會落在哪裡的外框
        move(r.vp(), r.at(c.x() + 50, c.y() + 30));
        release(r.vp(), r.at(c.x() + 50, c.y() + 30));
        QCOMPARE_STATE(r.state(), S::Preview);
        QCOMPARE(r.document().components().size(), boardBefore);
        QCOMPARE(r.document().revision(), revision);           // 文件沒有動
        QVERIFY(!r.canvas()->draftItem()->isVisible());        // 拖曳的外框收起來；樣本畫在框層，沒有區域框要還原
        QVERIFY(r.window.undoAction()->isEnabled());
        QVERIFY(r.window.undoAction()->toolTip().contains(QStringLiteral("移動 1 個元件")));
    }

    void escapeDuringADragLeavesEverythingAsItWas() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        const std::size_t depthBefore = r.presenter().session().proposal()->editCount();
        r.window.toolAction(ToolId::Select)->trigger();
        const QPointF c = r.centerOfComponent(3);
        press(r.vp(), r.at(c.x(), c.y()));
        move(r.vp(), r.at(c.x() + 50, c.y() + 30));
        QVERIFY(r.canvas()->draftItem()->isVisible());  // 拖曳中的外框
        sendKey(r.canvas(), Qt::Key_Escape);
        release(r.vp(), r.at(c.x() + 50, c.y() + 30));
        QCOMPARE_STATE(r.state(), S::Preview);               // Esc 只放棄拖曳，不捨棄提案
        QVERIFY(!r.canvas()->draftItem()->isVisible());
        QCOMPARE(r.presenter().session().proposal()->editCount(), depthBefore);  // 沒有 Command
    }

    void keysPressedInTheObjectTreeStillReachThePresenter() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        const int before = r.componentCount();
        const int sceneId = r.scene().snapshot->components[4].id;
        ObjectsPanel* panel = r.window.objectsPanel();
        panel->view()->selectionModel()->select(panel->model()->indexForId(sceneId),
                                                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        sendKey(panel->view(), Qt::Key_Delete);       // 焦點在樹上，Del 照樣刪除所選
        QCOMPARE(r.componentCount(), before - 1);
        sendKey(panel->view(), Qt::Key_Return);       // Enter 接受
        QCOMPARE_STATE(r.state(), S::Idle);
    }

    void afterAClickOnACardButtonTheCanvasGetsTheKeyboardBack() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.window.messageBar()->findChild<QWidget*>(QStringLiteral("messageCard"))->findChildren<QPushButton*>().at(1)->setFocus();
        r.cardButtons().at(1)->click();               // 〔捨棄〕
        QCOMPARE_STATE(r.state(), S::Idle);
        QCOMPARE(r.window.focusWidget(), static_cast<QWidget*>(r.canvas()));  // Del、方向鍵、Esc 不必再點一下畫布
    }

    void changingTheThresholdWithNothingToShowDoesNotRebuildTheScene() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.key(Qt::Key_Return);                         // Idle，板上有元件，沒有提案
        const SnapshotPtr before = r.scene().snapshot;
        r.window.paramsPanel()->scoreSlider()->setValue(90);
        QCOMPARE(r.presenter().session().threshold(), 0.90);  // 門檻記下來了
        QVERIFY(r.scene().snapshot == before);                // 但場景沒有重建：畫面上沒有東西依賴它
        r.searchByDrag(QRectF(200, 150, 40, 24));
        QVERIFY(r.waitState(S::Preview));
        QCOMPARE(r.presenter().session().proposal()->threshold(), 0.90);  // 下一份提案用它
    }

    void addToolNeedsATypeBeforeAnythingIsAccepted() {
        Rig r;
        r.open();
        r.window.toolAction(ToolId::Add)->trigger();
        press(r.vp(), r.at(300, 300));
        release(r.vp(), r.at(300, 300));
        QCOMPARE(r.hint(), zh("ui.add.needType"));
        QVERIFY(r.document().components().empty());
    }

    void addToolAddsAManualComponentOnceATypeExists() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.key(Qt::Key_Return);
        const std::size_t before = r.document().components().size();
        r.window.toolAction(ToolId::Add)->trigger();
        press(r.vp(), r.at(100, 100));
        release(r.vp(), r.at(100, 100));
        QCOMPARE(r.document().components().size(), before + 1);
        const ComponentInstance& added = r.document().components().back();
        QVERIFY(added.origin == ObjectOrigin::Manual);
        QVERIFY(!added.score.has_value());
        QCOMPARE(added.pose.x, 100.0);
        QVERIFY(r.scene().snapshot->components.back().label().contains(zh("origin.manual")));
    }

    // ---- 門檻：重新過濾，不重新搜尋（U3）----
    void draggingTheThresholdRefiltersWithoutSearchingAgain() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        const int builds = r.probe->builds;
        const int finds = r.probe->finds;
        const std::size_t foundAt70 = r.scene().counts.found;

        r.window.paramsPanel()->scoreSlider()->setValue(95);
        QCOMPARE_STATE(r.state(), S::Preview);
        QVERIFY(r.scene().counts.found < foundAt70);
        QCOMPARE(r.window.paramsPanel()->scoreValue()->text(), QStringLiteral("0.95"));
        QCOMPARE(r.probe->builds.load(), builds);  // 沒有重新搜尋
        QCOMPARE(r.probe->finds.load(), finds);
        QCOMPARE(r.componentCount(), static_cast<int>(r.scene().snapshot->components.size()));

        r.window.paramsPanel()->scoreSlider()->setValue(70);
        QCOMPARE(r.scene().counts.found, foundAt70);  // 拖回去就回來
        QVERIFY(r.window.undoAction()->isEnabled() == false);  // 門檻不進復原堆疊
    }

    void thresholdChangesKeepTheTreeScrollSelectionAndTheCanvasSelection() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.clickComponent(4);
        const std::vector<int> selected = r.canvas()->selection();
        QSignalSpy resets(r.window.objectsPanel()->model(), &QAbstractItemModel::modelReset);
        r.window.paramsPanel()->scoreSlider()->setValue(80);
        QCOMPARE(resets.count(), 0);  // 列沒變，樹只發 dataChanged，不 reset
        QCOMPARE(r.canvas()->selection(), selected);
        QCOMPARE(r.window.objectsPanel()->selectedIds(), selected);
    }

    void aThresholdAboveEveryScoreExplainsAndOffersToLowerIt() {
        auto world = defaultWorld();
        world.weakFraction = 0.0;
        world.missingFraction = 0.0;
        world.strongScoreMin = 0.90;
        world.strongScoreMax = 0.95;
        Rig r(world);
        r.open();
        r.window.paramsPanel()->scoreSlider()->setValue(99);  // Idle：只記下
        QCOMPARE_STATE(r.state(), S::Idle);
        r.searchByDrag(QRectF(560, 360, 60, 32));
        QVERIFY(r.waitState(S::NoMatch));
        QVERIFY(r.window.messageBar()->cardVisible());
        QCOMPARE(r.window.messageBar()->cardTitle(), zh("ui.noMatch.title"));
        QVERIFY(!r.canvas()->draftItem()->isVisible());  // 樣本畫在框層；訊息照舊不蓋在影像上
        const int builds = r.probe->builds;

        // 〔降低門檻到 {suggested}〕：不重新搜尋，直接過濾快取
        QVERIFY(r.cardButtons().first()->text().contains(QStringLiteral("降低門檻到")));
        r.cardButtons().first()->setFocus();
        r.cardButtons().first()->click();
        QCOMPARE_STATE(r.state(), S::Preview);
        QVERIFY(r.window.messageBar()->cardVisible());  // 新的卡（〔接受〕〔捨棄〕）接著出現，按鈕可能是同一批
        QCOMPARE(r.window.focusWidget(), static_cast<QWidget*>(r.canvas()));  // 不管按鈕還在不在，鍵盤都回到畫布
        QCOMPARE(r.probe->builds.load(), builds);
        QVERIFY(r.window.paramsPanel()->scoreSlider()->value() < 99);  // 滑桿跟著動
        QVERIFY(r.scene().counts.found >= 2);
    }

    // ---- 流程 B：區域掃描（樣本庫還沒有實作，結果是「樣本庫是空的」）----
    void scanningARegionSaysTheLibraryIsEmptyAndOffersToTeachASample() {
        Rig r;
        r.open();
        r.drag(ToolId::Region, QRectF(200, 200, 300, 200));
        QCOMPARE_STATE(r.state(), S::NoMatch);
        QCOMPARE(r.window.messageBar()->cardTitle(), zh("ui.noMatch.title"));
        QCOMPARE(r.probe->builds.load(), 0);  // 沒有拿教樣本的搜尋去假裝比對
        QCOMPARE(r.cardButtons().size(), 2);  // 〔教一個新樣本〕〔關閉〕
        QCOMPARE(r.cardButtons().first()->text(), zh("ui.noMatch.action.teachNew"));
        QVERIFY(r.canvas()->draftItem()->isVisible());

        r.cardButtons().first()->click();
        QCOMPARE_STATE(r.state(), S::Idle);
        QVERIFY(r.window.toolAction(ToolId::Sample)->isChecked());  // 工具換成「教樣本」
        QCOMPARE(r.canvas()->activeTool(), ToolId::Sample);
    }

    void dismissingTheNoMatchCardGoesToIdle() {
        Rig r;
        r.open();
        r.drag(ToolId::Region, QRectF(200, 200, 300, 200));
        r.cardButtons().last()->click();
        QCOMPARE_STATE(r.state(), S::Idle);
        QVERIFY(!r.window.messageBar()->cardVisible());
        QVERIFY(!r.canvas()->draftItem()->isVisible());
    }

    void escDismissesTheNoMatchCard() {
        Rig r;
        r.open();
        r.drag(ToolId::Region, QRectF(200, 200, 300, 200));
        r.key(Qt::Key_Escape);
        QCOMPARE_STATE(r.state(), S::Idle);
    }

    // ---- 錯誤與再試 ----
    void aFailedSearchShowsAnErrorCardAndRetryRunsItAgain() {
        Rig r;
        r.open();
        r.probe->fail = true;
        r.searchByDrag(QRectF(560, 360, 60, 32));
        QVERIFY(r.waitState(S::Error));
        QCOMPARE(r.window.messageBar()->cardTitle(), zh("ui.error.searchFailed.title"));
        QCOMPARE(r.cardButtons().size(), 2);  // 〔關閉〕〔再試一次〕
        QVERIFY(!r.canvas()->draftItem()->isVisible());  // 樣本是元件，拖曳中的框不留

        r.probe->fail = false;
        r.cardButtons().at(1)->click();
        QCOMPARE_STATE(r.state(), S::Searching);
        QVERIFY(r.waitState(S::Preview));
    }

    void dismissingASearchErrorGoesToIdleAndClearsTheBox() {
        Rig r;
        r.open();
        r.probe->fail = true;
        r.searchByDrag(QRectF(560, 360, 60, 32));
        QVERIFY(r.waitState(S::Error));
        r.cardButtons().first()->click();
        QCOMPARE_STATE(r.state(), S::Idle);
        QVERIFY(!r.canvas()->draftItem()->isVisible());
    }

    // ---- 忙碌、進度與取消（ux-spec 第 9 節）----
    void aSlowSearchShowsBusyThenProgressAndEscCancelsToIdleAtOnce() {
        auto world = defaultWorld();
        world.latencyMs = 1500;
        Rig r(world);
        r.open();
        QSignalSpy applied(&r.presenter(), &AutoRoiPresenter::searchApplied);
        r.searchByDrag(QRectF(560, 360, 60, 32));
        QCOMPARE_STATE(r.state(), S::Searching);
        QCOMPARE(r.presenter().busyLevel(), BusyLevel::None);  // 剛開始：什麼標示都沒有

        QVERIFY(QTest::qWaitFor([&] { return r.presenter().busyLevel() == BusyLevel::Busy; }, 2000));
        QCOMPARE(r.summary(), zh("ui.search.busy"));      // 狀態列「搜尋中…」
        QVERIFY(r.canvas()->busyCursor());                // 游標變忙碌
        QVERIFY(!r.window.messageBar()->progressVisible());

        QVERIFY(QTest::qWaitFor([&] { return r.presenter().busyLevel() == BusyLevel::Progress; }, 2000));
        QVERIFY(r.window.messageBar()->progressVisible()); // 訊息列的進度條與〔取消〕
        QVERIFY(r.window.messageBar()->findChild<QPushButton*>(QStringLiteral("messageCancel"))->text().contains(QStringLiteral("Esc")));

        r.key(Qt::Key_Escape);                             // Esc 取消
        QCOMPARE_STATE(r.state(), S::Idle);                // UI 立刻回到 Idle，不等 worker
        QVERIFY(r.hint().contains(QStringLiteral("已取消搜尋")));
        QVERIFY(!r.window.messageBar()->progressVisible());
        QVERIFY(!r.canvas()->draftItem()->isVisible());

        // 忙碌標示至少停留 busyMinShow，之後消失；已取消的搜尋結果不會被套用。
        QVERIFY(QTest::qWaitFor([&] { return r.presenter().busyLevel() == BusyLevel::None; }, 2000));
        QTest::qWait(300);
        QCOMPARE(applied.count(), 0);
        QCOMPARE_STATE(r.state(), S::Idle);
        QCOMPARE(r.componentCount(), 1);  // 只有樣本
        QVERIFY(!r.canvas()->busyCursor());
    }

    void theBusyLabelStaysForItsMinimumTimeAfterAQuickResult() {
        auto world = defaultWorld();
        world.latencyMs = 120;  // 搜尋比忙碌門檻（30 ms）久一點，但很快就結束
        Rig r(world);
        r.open();
        PresenterSettings s = r.presenter().settings();
        s.busyAfterMs = 30;
        s.progressAfterMs = 5000;  // 只要小標示，不要進度列
        s.busyMinShowMs = 600;
        r.presenter().setSettings(s);
        r.searchByDrag(QRectF(560, 360, 60, 32));
        QVERIFY(QTest::qWaitFor([&] { return r.presenter().busyLevel() == BusyLevel::Busy; }, 2000));
        QElapsedTimer sinceShown;
        sinceShown.start();
        QVERIFY(r.waitState(S::Preview));                       // 結果馬上套用，不等標示
        QCOMPARE(r.presenter().busyLevel(), BusyLevel::Busy);   // 但小標示不會一閃而過
        QVERIFY(QTest::qWaitFor([&] { return r.presenter().busyLevel() == BusyLevel::None; }, 3000));
        QVERIFY2(sinceShown.elapsed() >= 500, qPrintable(QString::number(sinceShown.elapsed())));  // 至少停留 busyMinShow
    }

    void aCancelHidesTheBusyLabelAtOnce() {
        auto world = defaultWorld();
        world.latencyMs = 1500;
        Rig r(world);
        r.open();
        PresenterSettings s = r.presenter().settings();
        s.busyAfterMs = 30;
        s.progressAfterMs = 5000;
        s.busyMinShowMs = 600;
        r.presenter().setSettings(s);
        r.searchByDrag(QRectF(560, 360, 60, 32));
        QVERIFY(QTest::qWaitFor([&] { return r.presenter().busyLevel() == BusyLevel::Busy; }, 2000));
        r.key(Qt::Key_Escape);
        QCOMPARE_STATE(r.state(), S::Idle);
        QCOMPARE(r.presenter().busyLevel(), BusyLevel::None);  // 取消是使用者的決定：「搜尋中…」不能留在 Idle 的畫面上
        QVERIFY(!r.canvas()->busyCursor());
        QVERIFY(r.summary() != zh("ui.search.busy"));
    }

    // 取消旗標升起的時候，use case 可能已經在最後一個階段、不可中斷的函式庫呼叫裡：它會「成功」返回一份結果，
    // JobRunner 只能把這份報告標成 Cancelled。這份遲到的、成功的結果不能被套用：靠世代丟掉。
    void aLateSuccessfulResultOfAReplacedSearchIsDropped() {
        Rig r;
        r.open();
        r.probe->gateWindows = true;
        QSignalSpy applied(&r.presenter(), &AutoRoiPresenter::searchApplied);
        r.searchByDrag(QRectF(560, 360, 60, 32));  // 搜尋 A
        QVERIFY(QTest::qWaitFor([&] { return r.probe->windowEntered.load() == 1; }, 5000));  // A 卡在最後一步
        const quint64 generationA = r.presenter().generation();
        r.presenters->params().setAngleRange(-12.0, 12.0);
        r.presenters->params().flush();  // 參數變了 → 重新搜尋 B（新的世代；A 的取消旗標升起，但 A 停不下來）
        QVERIFY(r.presenter().generation() > generationA);
        QVERIFY(QTest::qWaitFor([&] { return r.probe->windowEntered.load() == 2; }, 5000));  // B 也走到最後一步
        r.probe->windowReleased = 1;                        // 放行 A：它成功返回
        QTest::qWait(300);                                  // A 的報告（Cancelled、有結果）已經回到 UI 執行緒
        QCOMPARE_STATE(r.state(), S::Searching);            // 還在等 B：A 的結果被丟掉了
        QCOMPARE(applied.count(), 0);
        QCOMPARE(r.componentCount(), 1);  // 只有樣本
        r.probe->windowReleased = 2;                        // 放行 B
        QVERIFY(r.waitState(S::Preview));
        QCOMPARE(applied.count(), 1);
        QCOMPARE(applied.first().first().toULongLong(), r.presenter().generation());
    }

    void theCancelButtonInTheProgressRowCancels() {
        auto world = defaultWorld();
        world.latencyMs = 1500;
        Rig r(world);
        r.open();
        r.searchByDrag(QRectF(560, 360, 60, 32));
        QVERIFY(QTest::qWaitFor([&] { return r.presenter().busyLevel() == BusyLevel::Progress; }, 3000));
        QPushButton* cancel = r.window.messageBar()->findChild<QPushButton*>(QStringLiteral("messageCancel"));
        cancel->setFocus();
        cancel->click();
        QCOMPARE_STATE(r.state(), S::Idle);
        QCOMPARE(r.window.focusWidget(), static_cast<QWidget*>(r.canvas()));  // 取消之後鍵盤也回到畫布
    }

    void aFastSearchNeverFlashesTheBusyIndicator() {
        Rig r;
        r.open();
        PresenterSettings s = r.presenter().settings();
        s.busyAfterMs = 5000;  // 搜尋在這之前就結束
        r.presenter().setSettings(s);
        QVERIFY(r.teachSample());
        QCOMPARE(r.presenter().busyLevel(), BusyLevel::None);
        QVERIFY(!r.canvas()->busyCursor());
    }

    void theCanvasIsPausedWhileSearching() {
        auto world = defaultWorld();
        world.latencyMs = 800;
        Rig r(world);
        r.open();
        r.searchByDrag(QRectF(560, 360, 60, 32));
        QCOMPARE_STATE(r.state(), S::Searching);
        QVERIFY(!r.canvas()->toolsEnabled());
        r.window.toolAction(ToolId::Sample)->trigger();
        press(r.vp(), r.at(100, 100));  // 搜尋中不能再畫新的框
        QCOMPARE_STATE(r.state(), S::Searching);
        release(r.vp(), r.at(160, 140));
        QCOMPARE_STATE(r.state(), S::Searching);
        r.presenter().cancelSearch();
        QVERIFY(r.canvas()->toolsEnabled());
    }

    // ---- latest-wins 與防抖 ----
    void changingTheAngleRangeSearchesOnceAfterTheDebounce() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        QCOMPARE(r.probe->builds.load(), 1);

        QDoubleSpinBox* min = r.window.paramsPanel()->angleMin();
        min->setValue(-8.0);
        min->setValue(-9.0);
        min->setValue(-10.0);                          // 拖曳中連續變動
        QVERIFY(r.presenters->params().searchPending());
        QCOMPARE(r.probe->builds.load(), 1);           // 防抖期間不搜尋
        QCOMPARE_STATE(r.state(), S::Preview);

        QVERIFY(QTest::qWaitFor([&] { return r.probe->builds.load() == 2; }, 3000));
        QVERIFY(r.waitState(S::Preview));
        QTest::qWait(150);
        QCOMPARE(r.probe->builds.load(), 2);           // 三次變動 ＝ 一次搜尋
        QCOMPARE(r.presenter().searchParams().angleMinDeg, -10.0);
    }

    void aNewSearchReplacesAnOlderOneThatIsStillRunning() {
        auto world = defaultWorld();
        world.latencyMs = 400;
        Rig r(world);
        r.open();
        QSignalSpy applied(&r.presenter(), &AutoRoiPresenter::searchApplied);
        r.searchByDrag(QRectF(560, 360, 60, 32));
        QCOMPARE_STATE(r.state(), S::Searching);
        const quint64 first = r.presenter().generation();
        r.presenters->params().setAngleRange(-12.0, 12.0);
        r.presenters->params().flush();               // 搜尋中參數變了 → 重新搜尋，最新的優先
        QVERIFY(r.presenter().generation() > first);
        QVERIFY(r.waitState(S::Preview));
        QTest::qWait(200);
        QCOMPARE(applied.count(), 1);                  // 舊的搜尋結果被丟掉，只套用最新的一次
        QCOMPARE(applied.first().first().toULongLong(), r.presenter().generation());
    }

    // ---- 換影像、語言 ----
    void openingAnotherImageResetsEverythingIncludingTheHistory() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.key(Qt::Key_Return);
        QVERIFY(r.window.undoAction()->isEnabled());
        r.open(640, 480);
        QCOMPARE_STATE(r.state(), S::Idle);
        QVERIFY(r.document().components().empty());
        QVERIFY(!r.window.undoAction()->isEnabled());
        QCOMPARE(r.componentCount(), 0);
        QCOMPARE(r.treeRows(), 0);
        QCOMPARE(r.document().board().imageWidth, 640);
    }

    void openingAnotherImageWhileSearchingCancelsTheSearch() {
        auto world = defaultWorld();
        world.latencyMs = 800;
        Rig r(world);
        r.open();
        QSignalSpy applied(&r.presenter(), &AutoRoiPresenter::searchApplied);
        r.searchByDrag(QRectF(560, 360, 60, 32));
        QCOMPARE_STATE(r.state(), S::Searching);
        r.open(640, 480);
        QCOMPARE_STATE(r.state(), S::Idle);
        QTest::qWait(300);
        QCOMPARE(applied.count(), 0);
        QCOMPARE(r.componentCount(), 0);
    }

    void switchingLanguageRetranslatesTheCardTheHintAndTheSummary() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        QCOMPARE(r.window.messageBar()->cardTitle().left(2), QStringLiteral("找到"));
        QVERIFY(Translations::install(QStringLiteral("en")));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();  // LanguageChange 先到視窗，再排隊到 Presenter
        QVERIFY2(r.window.messageBar()->cardTitle().startsWith(QStringLiteral("Found")),
                 qPrintable(r.window.messageBar()->cardTitle()));
        QVERIFY(r.cardButtons().at(0)->text().startsWith(QStringLiteral("Accept")));
        QTRY_VERIFY2(r.summary().contains(QStringLiteral("Found")), qPrintable(r.summary()));  // 「搜尋中…」可能還在（見上）
        QVERIFY(Translations::install(QStringLiteral("zh_TW")));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QCOMPARE(r.window.messageBar()->cardTitle().left(2), QStringLiteral("找到"));
    }

    void theHintAfterALanguageSwitchFollowsTheState() {
        Rig r;
        r.open();
        QVERIFY(Translations::install(QStringLiteral("en")));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QVERIFY(r.hint().startsWith(QStringLiteral("S draws")));
        QVERIFY(Translations::install(QStringLiteral("zh_TW")));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
    }

    // ---- back end 的提醒（使用者 2026-10-07、2026-10-08 決定：卡片按〔關閉〕才收起；被搜尋的卡片蓋過之後再出現；
    //      狀態列的 back end 名稱後面一直標示，滑鼠移上去看完整說明）----
    static std::vector<aoi::BackendNotice> runtimeLicenseNotice() {
        return {{"ui.backend.halcon.sdkDongleMissing", "ui.backend.halcon.runtimeLicense",
                 {{"inUse", "3-7614034"}, {"missing", "3-7081033"}}, "HALCON runs with the runtime license."}};
    }

    void aBackendNoticeShowsACardThatStaysUntilClosed() {
        Rig r(defaultWorld(), true, aoi::app::ExecutorKind::QtThreadPool, runtimeLicenseNotice());
        MessageBar* bar = r.window.messageBar();
        QVERIFY(bar->noticeVisible());
        QCOMPARE(bar->noticeTitle(), zh("ui.backend.notice.title", {{"backend", "Fake (test)"}}));
        const QString detail = zh("ui.backend.halcon.sdkDongleMissing", {{"inUse", "3-7614034"}, {"missing", "3-7081033"}});
        QVERIFY(bar->findChild<QWidget*>(QStringLiteral("messageNotice"))->findChildren<QLabel*>().size() >= 2);
        // 狀態列：名稱 · 標示，滑鼠移上去是完整說明
        QCOMPARE(r.backendStatus(), QStringLiteral("Fake (test) · ") + zh("ui.backend.halcon.runtimeLicense"));
        QCOMPARE(r.window.backendStatusLabel()->toolTip(), detail);
        // 不是吐司：過了吐司的時間還在
        QTest::qWait(ThemeTokens::builtin().timing.toast + 300);
        QVERIFY(bar->noticeVisible());

        // 搜尋的卡片優先：預覽時讓位，接受之後再出現
        r.open();
        QVERIFY(r.teachSample());
        QVERIFY(bar->cardVisible());
        QVERIFY(!bar->noticeVisible());
        r.key(Qt::Key_Return);
        QCOMPARE_STATE(r.state(), S::Idle);
        QVERIFY(!bar->cardVisible());
        QVERIFY(bar->noticeVisible());
        QTRY_VERIFY(!r.summary().isEmpty());
        QVERIFY2(r.backendStatus().contains(zh("ui.backend.halcon.runtimeLicense")), qPrintable(r.backendStatus()));

        // 〔關閉〕才收起；之後的卡片收起時也不再出現
        const auto buttons = bar->findChild<QWidget*>(QStringLiteral("messageNotice"))->findChildren<QPushButton*>();
        QCOMPARE(buttons.size(), 1);
        QCOMPARE(buttons.first()->text(), zh("action.dismiss"));
        buttons.first()->click();
        QVERIFY(!bar->noticeVisible());
        QVERIFY(!r.presenters->backend().noticePending());
        // 提示回來（接受時的吐司還在的話，等它結束）
        QTRY_VERIFY_WITH_TIMEOUT(r.hint().contains(QStringLiteral("F5")), ThemeTokens::builtin().timing.toast + 1000);  // 板上有樣本：Idle 的提示
        QVERIFY(r.teachSample());
        r.key(Qt::Key_Escape);
        r.key(Qt::Key_Escape);
        QVERIFY(!bar->noticeVisible());
        // 狀態列的標示一直都在
        QVERIFY(r.backendStatus().contains(zh("ui.backend.halcon.runtimeLicense")));
    }

    void aClosedNoticeStaysClosedAfterALanguageSwitchAndTheMarkIsRetranslated() {
        Rig r(defaultWorld(), true, aoi::app::ExecutorKind::QtThreadPool, runtimeLicenseNotice());
        MessageBar* bar = r.window.messageBar();
        QVERIFY(Translations::install(QStringLiteral("en")));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QCOMPARE(bar->noticeTitle(), QStringLiteral("Note about Fake (test)"));
        QCOMPARE(r.backendStatus(), QStringLiteral("Fake (test) · runtime license"));
        QVERIFY(r.window.backendStatusLabel()->toolTip().startsWith(QStringLiteral("HALCON runs with the runtime license")));
        r.presenters->backend().applyNoticeAction(0);
        QVERIFY(!bar->noticePending());
        QVERIFY(Translations::install(QStringLiteral("zh_TW")));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QVERIFY(!bar->noticePending());
        QVERIFY(!bar->noticeVisible());
        QCOMPARE(r.backendStatus(), QStringLiteral("Fake (test) · runtime 授權"));
    }

    void withoutANoticeThereIsNoCardAndNoMark() {
        Rig r;
        QVERIFY(!r.window.messageBar()->noticePending());
        QCOMPARE(r.backendStatus(), QStringLiteral("Fake (test)"));
        QCOMPARE(r.window.backendStatusLabel()->toolTip(), zh("ui.backend.status.tip", {{"backend", "Fake (test)"}}));
        r.presenters->backend().applyNoticeAction(0);  // 沒有提醒時按鈕不存在；呼叫了也不會怎樣
        QVERIFY(!r.window.messageBar()->noticePending());
        QCOMPARE(r.presenters->backend().availableBackends().size(), std::size_t{4});  // fake、other、broken、off
    }

    // ---- 執行中切換 back end（ADR-021；規則在 application 的 BackendSwitch，tst_backend_switch）----
    static QPushButton* noticeButton(Rig& r, const QString& text) {
        QWidget* notice = r.window.messageBar()->findChild<QWidget*>(QStringLiteral("messageNotice"));
        for (QPushButton* b : notice->findChildren<QPushButton*>())
            if (b->text() == text) return b;
        qFatal("no notice button '%s'", qPrintable(text));
        return nullptr;
    }

    static bool waitSwitch(QSignalSpy& finished, int count = 1) {
        return QTest::qWaitFor([&] { return finished.count() >= count; }, 8000);
    }

    // 建立在背景執行緒，UI 不會卡住：切換中選單停用、狀態列「切換到 …」，事件迴圈照常跑。成功後之後的搜尋用新的。
    void switchingBuildsTheNewBackendInTheBackgroundAndThenUsesIt() {
        Rig r;
        r.open();
        QSignalSpy finished(&r.presenters->backend(), &BackendPresenter::switchFinished);
        QSignalSpy changed(&r.presenters->backend(), &BackendPresenter::backendChanged);
        QVERIFY(r.window.backendCombo()->isEnabled());
        r.otherProbe->gateCreate = true;
        r.chooseBackend(QStringLiteral("other"));
        QVERIFY(QTest::qWaitFor([&] { return r.otherProbe->createEntered.load(); }, 5000));
        // 工廠卡在閘門上（像 HALCON 的授權檢查），UI 執行緒仍在處理事件
        QTest::qWait(150);
        QVERIFY(r.presenters->backend().switching());
        QVERIFY(!r.window.backendCombo()->isEnabled());
        QCOMPARE(r.window.backendCombo()->currentIndex(), r.comboIndex(QStringLiteral("other")));
        QCOMPARE(r.backendStatus(), zh("ui.backend.switching", {{"backend", "Other (test)"}}));
        QVERIFY2(!r.otherProbe->createdOnUiThread, "the back end must be built off the UI thread");
        QCOMPARE(finished.count(), 0);

        r.otherProbe->createReleased = true;
        QVERIFY(waitSwitch(finished));
        QCOMPARE(finished.first().first().toBool(), true);
        QCOMPARE(r.presenters->backend().current(), std::string("other"));
        QVERIFY(r.window.backendCombo()->isEnabled());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.first().at(0).toString(), QStringLiteral("other"));  // 組裝根記住它
        QCOMPARE(changed.first().at(1).toStringList(), QStringList{QStringLiteral("Other runs with the runtime license.")});
        // 新的 back end 有提醒：卡片與狀態列的標示（第 1 件）
        QVERIFY(r.window.messageBar()->noticeVisible());
        QCOMPARE(r.backendStatus(), QStringLiteral("Other (test) · ") + zh("ui.backend.halcon.runtimeLicense"));

        const int fakeBuilds = r.probe->builds;
        QVERIFY(r.teachSample());
        QVERIFY(r.otherProbe->builds >= 1);
        QCOMPARE(r.probe->builds.load(), fakeBuilds);  // 舊的不再被用到

        // 換回 fake：沒有提醒，卡片收起、標示拿掉
        r.key(Qt::Key_Return);
        r.chooseBackend(QStringLiteral("fake"));
        QVERIFY(waitSwitch(finished, 2));
        QCOMPARE(r.presenters->backend().current(), std::string("fake"));
        QVERIFY(!r.window.messageBar()->noticePending());
        QCOMPARE(r.backendStatus(), QStringLiteral("Fake (test)"));
    }

    // 使用者 2026-10-08 決定：畫面上有提案時，用新的 back end 對同一個區域重新搜尋（舊的提案留著，直到新結果取代它）。
    void switchingWithAProposalSearchesTheSameRegionAgainWithTheNewBackend() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample(QRectF(560, 360, 60, 32)));
        QCOMPARE_STATE(r.state(), S::Preview);
        const Rect region = r.presenter().session().lastSearch()->region;
        QSignalSpy finished(&r.presenters->backend(), &BackendPresenter::switchFinished);
        QSignalSpy applied(&r.presenter(), &AutoRoiPresenter::searchApplied);
        r.chooseBackend(QStringLiteral("other"));
        QVERIFY(waitSwitch(finished));
        QVERIFY(QTest::qWaitFor([&] { return applied.count() >= 1; }, 8000));
        QCOMPARE_STATE(r.state(), S::Preview);
        QVERIFY(r.otherProbe->builds >= 1);
        QCOMPARE(r.presenter().session().lastSearch()->region.x, region.x);
        QCOMPARE(r.presenter().session().lastSearch()->region.w, region.w);
        QCOMPARE(r.document().components().size(), std::size_t{1});  // 沒有自動接受（只有樣本）
    }

    // 搜尋中切換：舊的搜尋先作廢（不等它停下，它的結果被丟掉），換好之後用新的 back end 搜尋同一個區域。
    void switchingDuringASearchDropsItsResultAndSearchesAgainWithTheNewBackend() {
        Rig r;
        r.open();
        r.probe->gateWindows = true;  // fake 的搜尋卡在最後一步（不可中斷的函式庫呼叫）
        QSignalSpy applied(&r.presenter(), &AutoRoiPresenter::searchApplied);
        r.searchByDrag(QRectF(560, 360, 60, 32));
        QVERIFY(QTest::qWaitFor([&] { return r.probe->windowEntered >= 1; }, 5000));
        QCOMPARE_STATE(r.state(), S::Searching);

        QSignalSpy finished(&r.presenters->backend(), &BackendPresenter::switchFinished);
        r.chooseBackend(QStringLiteral("other"));
        QCOMPARE_STATE(r.state(), S::Searching);  // 暫停，不回 Idle
        QVERIFY(waitSwitch(finished));
        QVERIFY(QTest::qWaitFor([&] { return applied.count() >= 1; }, 8000));  // other 的結果
        QCOMPARE_STATE(r.state(), S::Preview);
        QVERIFY(r.otherProbe->builds >= 1);
        const quint64 generation = r.presenter().generation();

        r.probe->windowReleased = 1 << 20;  // 舊的搜尋做完：它的結果回來時被丟掉
        QTest::qWait(300);
        QCOMPARE(applied.count(), 1);
        QCOMPARE(r.presenter().generation(), generation);
        QCOMPARE_STATE(r.state(), S::Preview);
    }

    // 失敗：留在原本的 back end，訊息列的卡片說原因（ui.error.backend.*），按〔關閉〕才收起；提案不動、不重新搜尋。
    void aFailedSwitchKeepsTheOldBackendAndExplainsWhy() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        const int builds = r.probe->builds;
        QSignalSpy finished(&r.presenters->backend(), &BackendPresenter::switchFinished);
        QSignalSpy failed(&r.presenters->backend(), &BackendPresenter::switchFailed);
        r.chooseBackend(QStringLiteral("broken"));
        QVERIFY(waitSwitch(finished));
        QCOMPARE(finished.first().first().toBool(), false);
        QCOMPARE(failed.count(), 1);
        QCOMPARE(failed.first().at(1).toString(), QStringLiteral("no license (test)"));
        QCOMPARE(r.presenters->backend().current(), std::string("fake"));
        QCOMPARE(r.window.backendCombo()->currentIndex(), r.comboIndex(QStringLiteral("fake")));  // 選單回到原本的
        QVERIFY(r.window.backendCombo()->isEnabled());
        QCOMPARE(r.backendStatus(), QStringLiteral("Fake (test)"));
        QCOMPARE_STATE(r.state(), S::Preview);
        QCOMPARE(r.probe->builds.load(), builds);  // 沒有重新搜尋

        // 卡片讓位給預覽；預覽收起之後出現
        MessageBar* bar = r.window.messageBar();
        QVERIFY(bar->noticePending());
        QVERIFY(!bar->noticeVisible());
        r.key(Qt::Key_Return);
        QVERIFY(bar->noticeVisible());
        QCOMPARE(bar->noticeTitle(), zh("ui.error.backend.title", {{"backend", "Broken (test)"}}));
        QWidget* notice = bar->findChild<QWidget*>(QStringLiteral("messageNotice"));
        QStringList texts;
        for (const QLabel* label : notice->findChildren<QLabel*>()) texts << label->text();
        const QString all = texts.join(QLatin1Char('\n'));
        QVERIFY2(all.contains(QStringLiteral("no license (test)")), qPrintable(all));
        QVERIFY2(all.contains(zh("ui.backend.reverted", {{"backend", "Fake (test)"}})), qPrintable(all));
        QVERIFY2(all.contains(zh("ui.error.backend.body", {{"fallback", "Fake (test)"}})), qPrintable(all));
        // 工廠失敗（像授權檢查失敗）可以〔再試一次〕；〔關閉〕才收起
        QStringList buttons;
        for (const QPushButton* b : notice->findChildren<QPushButton*>()) buttons << b->text();
        QCOMPARE(buttons, (QStringList{zh("action.retry"), zh("action.dismiss")}));
        noticeButton(r, zh("action.dismiss"))->click();
        QVERIFY(!bar->noticePending());

        // 之後的搜尋照舊用 fake
        QVERIFY(r.teachSample());
        QVERIFY(r.probe->builds > builds);
    }

    // 第一次換失敗（像 dongle 沒插、授權檢查失敗），狀況改變之後（插上 dongle）按卡片上的〔再試一次〕就換過去，不必重新啟動程式。
    void aFailedSwitchCanBeRetriedFromTheCardOnceTheCauseIsGone() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.otherProbe->failCreate = true;
        QSignalSpy finished(&r.presenters->backend(), &BackendPresenter::switchFinished);
        r.chooseBackend(QStringLiteral("other"));
        QVERIFY(waitSwitch(finished));
        QCOMPARE(finished.last().first().toBool(), false);
        QCOMPARE(r.presenters->backend().current(), std::string("fake"));
        r.key(Qt::Key_Return);  // 接受預覽：卡片讓出位置給提醒卡片
        MessageBar* bar = r.window.messageBar();
        QVERIFY(bar->noticeVisible());
        QCOMPARE(bar->noticeTitle(), zh("ui.error.backend.title", {{"backend", "Other (test)"}}));

        r.otherProbe->failCreate = false;  // 使用者插上 dongle
        noticeButton(r, zh("action.retry"))->click();
        QVERIFY(waitSwitch(finished, 2));
        QCOMPARE(finished.last().first().toBool(), true);
        QCOMPARE(r.presenters->backend().current(), std::string("other"));
        QCOMPARE(r.otherProbe->creates.load(), 2);  // 真的再建立了一次（HALCON：重新檢查授權）
        QCOMPARE(bar->noticeTitle(), zh("ui.backend.notice.title", {{"backend", "Other (test)"}}));  // 失敗的卡片換成新的提醒
        QVERIFY(r.backendStatus().startsWith(QStringLiteral("Other (test)")));
    }

    void aBackendThatIsNotBuiltInOffersNoRetry() {
        Rig r;
        r.presenters->backend().selectBackend("off");
        QWidget* notice = r.window.messageBar()->findChild<QWidget*>(QStringLiteral("messageNotice"));
        QStringList buttons;
        for (const QPushButton* b : notice->findChildren<QPushButton*>()) buttons << b->text();
        QCOMPARE(buttons, QStringList{zh("action.dismiss")});  // 再試也一樣：沒有〔再試一次〕
    }

    // 搜尋時 back end 無法使用（例如執行中 HALCON 的授權不見了，HalconErrors）：卡片的標題是 back end 的名稱（S08 誤把錯誤訊息當成
    // 名稱），原因在做法清單；〔改用 Other (test)〕換過去之後，用新的 back end 再試一次同一個搜尋（2026-10-09）。
    void aBackendThatBecomesUnavailableWhileSearchingNamesItAndOffersToSwitch() {
        Rig r;
        r.open();
        r.probe->unavailable = true;
        r.searchByDrag(QRectF(560, 360, 60, 32));
        QVERIFY(r.waitState(S::Error));
        MessageBar* bar = r.window.messageBar();
        QCOMPARE(bar->cardTitle(), zh("ui.error.backend.title", {{"backend", "Fake (test)"}}));
        QStringList texts;
        for (const QLabel* label : bar->findChild<QWidget*>(QStringLiteral("messageCard"))->findChildren<QLabel*>()) texts << label->text();
        const QString all = texts.join(QStringLiteral(" | "));
        QVERIFY2(all.contains(QStringLiteral("the test license is gone")), qPrintable(all));
        QVERIFY2(all.contains(zh("ui.error.backend.body", {{"fallback", "Other (test)"}})), qPrintable(all));
        QStringList buttons;
        for (const QPushButton* b : r.cardButtons()) buttons << b->text();
        QCOMPARE(buttons, (QStringList{zh("action.dismiss"), zh("action.retry"), zh("ui.backend.switchTo", {{"backend", "Other (test)"}})}));

        QSignalSpy finished(&r.presenters->backend(), &BackendPresenter::switchFinished);
        r.cardButtons().last()->click();
        QVERIFY(waitSwitch(finished));
        QCOMPARE(r.presenters->backend().current(), std::string("other"));
        QVERIFY(r.waitState(S::Preview));  // 換好之後自動用新的 back end 再試一次
        QVERIFY(r.otherProbe->builds >= 1);
    }

    void anUnavailableBackendIsRefusedAtOnceWithItsReason() {
        Rig r;
        QSignalSpy finished(&r.presenters->backend(), &BackendPresenter::switchFinished);
        r.presenters->backend().selectBackend("off");  // 選單上停用；從介面呼叫也一樣不會嘗試
        QCOMPARE(finished.count(), 1);
        QVERIFY(!r.presenters->backend().switching());
        QCOMPARE(r.presenters->backend().current(), std::string("fake"));
        QVERIFY(r.window.messageBar()->noticeVisible());
        QCOMPARE(r.window.messageBar()->noticeTitle(), zh("ui.error.backend.title", {{"backend", "Off (test)"}}));
        r.presenters->backend().selectBackend("turbo");  // 沒有這個 id（例如記住的值過期了）
        QCOMPARE(finished.count(), 2);
        QCOMPARE(r.window.messageBar()->noticeTitle(), zh("ui.error.backend.title", {{"backend", "turbo"}}));
        QCOMPARE(r.presenters->backend().current(), std::string("fake"));
    }

    void selectingTheCurrentBackendOrSelectingWhileSwitchingDoesNothing() {
        Rig r;
        QSignalSpy finished(&r.presenters->backend(), &BackendPresenter::switchFinished);
        r.chooseBackend(QStringLiteral("fake"));
        QCoreApplication::processEvents();
        QCOMPARE(finished.count(), 0);
        QVERIFY(!r.presenters->backend().switching());
        r.otherProbe->gateCreate = true;
        r.chooseBackend(QStringLiteral("other"));
        r.presenters->backend().selectBackend("broken");  // 換的期間不收
        QCOMPARE(r.presenters->backend().switching(), true);
        r.otherProbe->createReleased = true;
        QVERIFY(waitSwitch(finished));
        QCOMPARE(finished.count(), 1);
        QCOMPARE(r.presenters->backend().current(), std::string("other"));
    }

#ifdef AOI_WITH_HALCON
    // 真的 HALCON：第一次建立約 1.5 秒（載入 DLL、授權檢查，ADR-017），要在背景執行緒，UI 執行緒的事件迴圈不能停住。
    // 這台機器拿不到授權時跳過並說原因（跳過不是通過）。
    void switchingToTheRealHalconKeepsTheUiThreadResponsive() {
        struct NullView final : IBackendView {
            void setBackends(const QList<BackendChoice>&, int, bool) override {}
            void setBackendStatus(const QString&, const QString&) override {}
            void showNotice(const MessageCard&) override {}
            void hideNotice() override {}
        } view;
        struct RecordingSink final : ISearchEngineSink {
            std::shared_ptr<const GenerateObjectsFromSample> used;
            InteractionState interactionState() const override { return InteractionState::Idle; }
            void pauseSearch() override {}
            void useSearchEngine(std::shared_ptr<const GenerateObjectsFromSample> useCase, const BackendLabels&) override {
                used = std::move(useCase);
            }
            void researchSameRegion() override {}
        } sink;
        const BackendRegistry registry = app::makeBackendRegistry();
        bool listed = false;
        for (const BackendInfo& info : registry.list()) listed = listed || (info.id == "halcon" && info.available);
        if (!listed) QSKIP("HALCON is not on the PATH here (halconcpp.dll)");
        Result<app::CoreServices> core = app::composeCore(registry, "fake");
        QVERIFY(core.ok());
        BackendServices services;
        services.backends = registry.list();
        services.current = core.value().engine;
        services.create = [&registry](const std::string& id) { return makeSearchEngine(registry, id); };
        services.executor = core.value().executor.get();
        BackendPresenter presenter(view, sink, services);
        QSignalSpy finished(&presenter, &BackendPresenter::switchFinished);
        QSignalSpy failed(&presenter, &BackendPresenter::switchFailed);

        QElapsedTimer clock;
        qint64 last = 0;
        qint64 worstGap = 0;
        QTimer ticker;
        ticker.setInterval(5);
        connect(&ticker, &QTimer::timeout, [&] {
            const qint64 now = clock.elapsed();
            worstGap = std::max(worstGap, now - last);
            last = now;
        });
        clock.start();
        ticker.start();
        presenter.selectBackend("halcon");
        const qint64 returnedAfter = clock.elapsed();
        QVERIFY(QTest::qWaitFor([&] { return finished.count() >= 1; }, 30000));
        const qint64 total = clock.elapsed();
        ticker.stop();
        if (!finished.first().first().toBool())
            QSKIP(qPrintable(QStringLiteral("HALCON cannot be used on this machine: ") + failed.first().at(1).toString()));
        qInfo("switch to HALCON: %lld ms in total, selectBackend returned after %lld ms, longest event-loop gap %lld ms",
              static_cast<long long>(total), static_cast<long long>(returnedAfter), static_cast<long long>(worstGap));
        QCOMPARE(presenter.current(), std::string("halcon"));
        QVERIFY(sink.used != nullptr);
        QVERIFY2(returnedAfter < 100, qPrintable(QString::number(returnedAfter)));
        QVERIFY2(worstGap < 200, qPrintable(QStringLiteral("the UI thread stalled for %1 ms").arg(worstGap)));
    }
#endif

    // ---- 其他 ----
    void theTypeEditorIsNotAvailableYetAndSaysSo() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample());
        r.key(Qt::Key_Return);
        r.window.toolAction(ToolId::Select)->trigger();
        const QPointF c = r.centerOfComponent(2);
        doubleClick(r.vp(), r.at(c.x(), c.y()));
        QCOMPARE(r.hint(), zh("ui.editor.unavailable"));
        QCOMPARE_STATE(r.state(), S::Idle);
    }

    // ---- 供人檢視的截圖（設定 AOI_SCREENSHOT_DIR 才執行）----
    void screenshotsForReview() {
        const QString dirPath = qEnvironmentVariable("AOI_SCREENSHOT_DIR");
        if (dirPath.isEmpty()) QSKIP("set AOI_SCREENSHOT_DIR to write review screenshots");
        QDir().mkpath(dirPath);
        auto world = defaultWorld();
        world.halfCols = 8;
        world.halfRows = 4;
        Rig r(world);
        r.window.resize(1280, 800);
        auto grab = [&](const char* name) {
            QCoreApplication::processEvents();
            r.window.grab().save(QDir(dirPath).filePath(QStringLiteral("%1.png").arg(name)));
        };
        grab("p0-empty");
        r.open(1200, 800);
        grab("p1-idle");
        QVERIFY(r.teachSample(QRectF(560, 360, 60, 32)));
        r.clickComponent(5);
        r.window.objectsPanel()->view()->expandAll();
        grab("p2-preview-light");
        r.theme.setMode(ThemeMode::Dark);
        grab("p3-preview-dark");
        r.key(Qt::Key_Return);
        grab("p4-accepted-dark");
        r.theme.setMode(ThemeMode::Light);
        r.drag(ToolId::Region, QRectF(200, 200, 300, 200));
        grab("p5-nomatch");
        r.cardButtons().last()->click();
        r.probe->fail = true;
        r.searchByDrag(QRectF(100, 100, 60, 32));
        QVERIFY(r.waitState(S::Error));
        grab("p6-error");
        QVERIFY(true);
    }

    void theStdThreadExecutorWorksTheSameWay() {
        Rig r(defaultWorld(), true, app::ExecutorKind::StdThreads);
        r.open();
        QVERIFY(r.teachSample());
        QCOMPARE_STATE(r.state(), S::Preview);
        r.key(Qt::Key_Return);
        QCOMPARE_STATE(r.state(), S::Idle);
    }

    // ---- ADR-025：樣本的建立與搜尋分開 ----
    void drawingASampleOnlyCreatesItAndSearchNeedsF5() {
        Rig r;
        r.open();
        r.window.searchAction()->trigger();  // 還沒有樣本
        QCOMPARE_STATE(r.state(), S::Idle);
        QCOMPARE(r.hint(), zh("ui.search.noSamples"));
        QCOMPARE(r.probe->builds.load(), 0);

        QVERIFY(r.drawSample(QRectF(560, 360, 60, 32)));
        QCOMPARE_STATE(r.state(), S::Idle);                      // 放開不搜尋
        QCOMPARE(r.probe->builds.load(), 0);
        QCOMPARE(r.document().components().size(), std::size_t{1});
        QVERIFY(r.document().components()[0].state == ObjectState::Sample);
        QCOMPARE(r.document().board().types.size(), std::size_t{1});
        QCOMPARE(r.componentCount(), 1);                         // 樣本畫在框層
        QVERIFY(!r.canvas()->draftItem()->isVisible());
        QCOMPARE(r.canvas()->selection(), std::vector<int>{1});  // 建完就是選取的
        QVERIFY(r.canvas()->handleItem()->isVisible());          // 單一選取：有把手
        QCOMPARE(r.canvas()->activeTool(), ToolId::Select);      // 回到選取工具
        QVERIFY(r.hint().contains(QStringLiteral("已建立樣本")));
        QVERIFY(r.window.undoAction()->isEnabled());
        QVERIFY(r.window.undoAction()->toolTip().contains(zh("ui.history.addSample")));

        QVERIFY(r.drawSample(QRectF(200, 150, 40, 24)));  // 第二個樣本：自己的類型
        QCOMPARE(r.document().board().types.size(), std::size_t{2});
        QTRY_VERIFY(r.window.messageBar()->hintText().contains(QStringLiteral("2 個樣本")) ||
                    r.hint().contains(QStringLiteral("2 個樣本")));

        r.key(Qt::Key_F5);  // 鍵盤也行（畫布有焦點時先到 Presenter）
        QCOMPARE_STATE(r.state(), S::Searching);
        QVERIFY(r.waitState(S::Preview));
        QCOMPARE(r.probe->builds.load(), 2);  // 每個樣本各搜一次
        QVERIFY(r.scene().rows->proposal);
    }

    void twoSamplesOnTheSameGridAreMergedAndAcceptReplacesTheLastResult() {
        // 假世界的格子中心距 ＝ 1.5 × 樣本大小：第二個樣本放在第一個的右邊一格（x + 90），兩個樣本看到同一片網格。
        Rig r;
        r.open();
        QVERIFY(r.drawSample(QRectF(560, 360, 60, 32)));
        QVERIFY(r.drawSample(QRectF(650, 360, 60, 32)));
        QVERIFY(r.searchAndWait());
        QCOMPARE_STATE(r.state(), S::Preview);
        const SceneCounts& c = r.scene().counts;
        QVERIFY2(c.merged > 0, "the same positions found by both samples should be merged");
        QVERIFY(r.window.messageBar()->cardAdvice().contains(QStringLiteral("不只一個樣本找到")));
        // 同一個位置只有一筆：任兩個提案物件的中心距離都大於半個格子。
        const auto& comps = r.scene().snapshot->components;
        for (std::size_t i = 0; i < comps.size(); ++i) {
            for (std::size_t j = i + 1; j < comps.size(); ++j) {
                const QPointF d = comps[i].outline.center() - comps[j].outline.center();
                QVERIFY2(std::hypot(d.x(), d.y()) > 30.0, qPrintable(QStringLiteral("%1 and %2 overlap").arg(comps[i].id).arg(comps[j].id)));
            }
        }
        r.key(Qt::Key_Return);
        const std::size_t first = r.document().components().size();
        QVERIFY(first > 10);

        // 再搜尋一次：接受時上一次自動產生的整批換掉，樣本留著，數量不會疊加。
        QVERIFY(r.searchAndWait());
        QVERIFY(r.window.messageBar()->cardAdvice().contains(QStringLiteral("會換成這一次的結果")));
        r.key(Qt::Key_Return);
        QCOMPARE(r.document().components().size(), first);
        std::size_t samples = 0;
        for (const ComponentInstance& comp : r.document().components()) samples += comp.state == ObjectState::Sample ? 1 : 0;
        QCOMPARE(samples, std::size_t{2});
        r.window.undoAction()->trigger();
        QCOMPARE_STATE(r.state(), S::Preview);
        QCOMPARE(r.document().components().size(), first);  // 復原：換掉的回來
    }

    void draggingAHandleResizesTheTypeForEveryPartOfIt() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample(QRectF(560, 360, 60, 32)));
        r.key(Qt::Key_Return);
        const std::size_t parts = r.document().components().size();
        r.clickComponent(0);  // 樣本（編號 1）
        QVERIFY(r.canvas()->handleItem()->isVisible());
        const QRectF outline = r.canvas()->handleItem()->rect();
        QCOMPARE(outline.size(), QSizeF(60, 32));
        // 拖右下角的把手：往右下 20 × 10 像素。框在螢幕上小於 72 px 時把手在角落外面 12 px（場景單位 12 ÷ 縮放）。
        const double push = 12.0 / r.canvas()->viewScale();
        const QPointF handle(outline.right() + push, outline.bottom() + push);
        press(r.vp(), r.at(handle.x(), handle.y()));
        move(r.vp(), r.at(handle.x() + 10, handle.y() + 5));
        QVERIFY(r.canvas()->draftItem()->isVisible());  // 拖曳中畫新的框
        move(r.vp(), r.at(handle.x() + 20, handle.y() + 10));
        release(r.vp(), r.at(handle.x() + 20, handle.y() + 10));
        const std::optional<Size2D> size = r.document().bodySizePx("t1");
        QVERIFY(size.has_value());
        QVERIFY2(std::abs(size->w - 80) < 1.5 && std::abs(size->h - 42) < 1.5, qPrintable(QStringLiteral("%1 x %2").arg(size->w).arg(size->h)));
        QCOMPARE(r.document().components().size(), parts);
        for (const ComponentView& v : r.scene().snapshot->components) QVERIFY(std::abs(v.outline.width() - size->w) < 1e-6);  // 同類型全部一起
        QVERIFY(r.window.undoAction()->toolTip().contains(QStringLiteral("框大小")));
        QVERIFY(r.hint().contains(QStringLiteral("已改大小")));
        r.window.undoAction()->trigger();
        QCOMPARE(r.document().bodySizePx("t1")->w, 60.0);
    }

    void theBoxToolStampsASubBoxOnEveryPartOfTheType() {
        Rig r;
        r.open();
        QVERIFY(r.drawSample(QRectF(560, 360, 60, 32)));  // 樣本（選取中）
        r.window.toolAction(ToolId::Box)->trigger();
        QCOMPARE(r.canvas()->activeTool(), ToolId::Box);
        press(r.vp(), r.at(565, 365));
        move(r.vp(), r.at(575, 372));
        release(r.vp(), r.at(575, 372));
        const ComponentType* t = r.document().findType("t1");
        QVERIFY(t != nullptr);
        QCOMPARE(t->groups.size(), std::size_t{2});
        QVERIFY(t->groups[1].kind == BoxGroupKind::Pin);
        QCOMPARE(r.boxCount(), 2);  // 樣本有兩個框了
        QVERIFY(r.hint().contains(QStringLiteral("已加上子框")));

        QVERIFY(r.searchAndWait());  // 找到的每一個都貼上兩個框（預覽就看得到）
        for (const ComponentView& v : r.scene().snapshot->components) QCOMPARE(v.boxes.size(), std::size_t{2});
        r.key(Qt::Key_Return);
        QCOMPARE(r.boxCount(), 2 * static_cast<int>(r.document().components().size()));

        r.window.toolAction(ToolId::Select)->trigger();
        r.key(Qt::Key_Escape);  // 清除選取
        r.window.toolAction(ToolId::Box)->trigger();
        press(r.vp(), r.at(100, 100));
        release(r.vp(), r.at(120, 110));
        QCOMPARE(r.hint(), zh("ui.box.needOne"));  // 沒有選取：說明
        QCOMPARE(t->groups.size(), std::size_t{2});
    }

    // M 工具（ADR-026）：拖一個框 ＝ 比對區（類型上），選取資訊顯示它、草稿框畫出它；CAD 框（框數、主體框）不變；
    // 特徵與清除走選取資訊表單；搜尋用比對區（假 back end 忽略它，結果和整框相同）；沒有選取時用吐司說明。
    void theMatchToolSetsTheMatchRegionOfTheType() {
        Rig r;
        r.open();
        QVERIFY(r.drawSample(QRectF(560, 360, 60, 32)));  // 樣本（選取中）
        r.window.toolAction(ToolId::Match)->trigger();
        QCOMPARE(r.canvas()->activeTool(), ToolId::Match);
        press(r.vp(), r.at(565, 365));
        move(r.vp(), r.at(590, 385));
        release(r.vp(), r.at(590, 385));
        const ComponentType* t = r.document().findType("t1");
        QVERIFY(t != nullptr && t->match.region);
        QCOMPARE(t->groups.size(), std::size_t{1});  // 不是框群組
        QCOMPARE(r.boxCount(), 1);
        QVERIFY(r.hint().contains(QStringLiteral("已設定比對區")));
        ObjectsPanel* panel = r.window.objectsPanel();
        QVERIFY(panel->matchValue()->text().startsWith(QStringLiteral("矩形")));
        QVERIFY(panel->matchClear()->isEnabled());
        QVERIFY(!panel->matchGrow()->isEnabled());  // 不是自動擷取的遮罩
        // 特徵：選邊緣 → 類型的模式改了（一個 Command）。
        panel->matchMode()->setCurrentIndex(1);
        emit panel->matchMode()->activated(1);
        QVERIFY(r.document().findType("t1")->match.mode == MatchMode::Edges);
        // 搜尋帶著比對區：樣本的規格是比對區的像素（搜尋本身由契約測試驗證；這張示範影像的那一塊對比太低）。
        const std::vector<SampleOnBoard> samples = r.document().samples();
        QVERIFY(samples.front().match.rect && samples.front().match.mode == MatchMode::Edges);
        QVERIFY(samples.front().match.rect->w < samples.front().rect.w);
        panel->matchClear()->click();
        QVERIFY(!r.document().findType("t1")->match.region);
        QVERIFY(r.document().findType("t1")->match.mode == MatchMode::Edges);
        r.window.undoAction()->trigger();
        QVERIFY(r.document().findType("t1")->match.region);

        r.window.toolAction(ToolId::Select)->trigger();
        r.key(Qt::Key_Escape);  // 清除選取
        r.window.toolAction(ToolId::Match)->trigger();
        press(r.vp(), r.at(100, 100));
        release(r.vp(), r.at(120, 110));
        QCOMPARE(r.hint(), zh("ui.match.needOne"));
    }

    void marqueeSelectAllAndSameTypeSelectManyParts() {
        Rig r;
        r.open();
        QVERIFY(r.drawSample(QRectF(560, 360, 60, 32)));
        QVERIFY(r.drawSample(QRectF(200, 150, 40, 24)));  // 第二個類型
        QVERIFY(r.searchAndWait());
        r.key(Qt::Key_Return);
        const std::size_t all = r.document().components().size();
        r.window.toolAction(ToolId::Select)->trigger();

        // 框選：在空白處拖一個框，框到的元件都選起來。
        r.key(Qt::Key_Escape);
        press(r.vp(), r.at(500, 300));
        move(r.vp(), r.at(600, 350));
        QVERIFY(r.canvas()->draftItem()->isVisible());  // 框選中的框
        move(r.vp(), r.at(700, 420));
        release(r.vp(), r.at(700, 420));
        const std::size_t boxed = r.canvas()->selection().size();
        QVERIFY2(boxed >= 2, qPrintable(QString::number(boxed)));
        QVERIFY(boxed < all);
        QVERIFY(!r.canvas()->handleItem()->isVisible());  // 多選：沒有把手

        // 框選到的一起拖：一個 Command，每個都移了。
        const int firstId = r.canvas()->selection().front();
        const Pose2D before = r.document().find(firstId)->pose;
        const std::size_t depth = r.document().historyDepth();
        const QPointF c = r.canvas()->layerItem()->outlineOf({firstId}).center();
        press(r.vp(), r.at(c.x(), c.y()));
        move(r.vp(), r.at(c.x() + 30, c.y()));
        move(r.vp(), r.at(c.x() + 40, c.y()));
        release(r.vp(), r.at(c.x() + 40, c.y()));
        QCOMPARE(r.document().historyDepth(), depth + 1);
        QVERIFY(std::abs(r.document().find(firstId)->pose.x - (before.x + 40)) < 0.6);
        QCOMPARE(r.canvas()->selection().size(), boxed);

        r.key(Qt::Key_A, Qt::ControlModifier);  // 全選
        QCOMPARE(r.canvas()->selection().size(), all);
        r.clickComponent(0);                                       // 第一個樣本（t1）
        r.key(Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);  // 選取同類型
        const std::size_t sameType = r.canvas()->selection().size();
        QVERIFY(sameType > 1 && sameType < all);
        for (int id : r.canvas()->selection()) QCOMPARE(r.document().find(id)->typeId, std::string("t1"));
        r.window.selectAllAction()->trigger();  // 選單上的動作一樣
        QCOMPARE(r.canvas()->selection().size(), all);
    }

    // ---- 選取資訊表單與樣本庫 dock（2026-10-14）----
    void typingASizeInTheSelectionFormResizesTheTypeForEveryPartWithTheCenterFixed() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample(QRectF(560, 360, 60, 32)));
        r.key(Qt::Key_Return);
        ObjectsPanel* panel = r.window.objectsPanel();
        QVERIFY(!panel->sizeW()->isEnabled());  // 沒有選取：表單停用
        QCOMPARE(panel->selectionSummary()->text(), zh("ui.selection.none"));
        r.clickComponent(0);  // 樣本 X1（編號 1）
        QVERIFY(panel->sizeW()->isEnabled());
        QVERIFY(panel->centerX()->isEnabled());
        QVERIFY(panel->applyToAll()->isEnabled());
        QCOMPARE(panel->sizeW()->value(), 60.0);
        QCOMPARE(panel->sizeH()->value(), 32.0);
        QCOMPARE(panel->centerX()->value(), 590.0);
        QCOMPARE(panel->centerY()->value(), 376.0);
        QVERIFY(panel->selectionSummary()->text().contains(QStringLiteral("X1")));
        QCOMPARE(panel->boxesValue()->text(), QStringLiteral("1"));
        QVERIFY(!panel->sizeUm()->isVisibleTo(panel));  // 假板子沒有校正值：不顯示 µm
        QVERIFY(!panel->selectionNote()->isVisibleTo(panel));
        const std::size_t parts = r.document().components().size();
        const std::size_t depth = r.document().historyDepth();

        // 輸入寬 80、按 Enter（editingFinished）：改類型、同類型全部一起、中心不動、一個 Command。
        panel->sizeW()->setValue(80);
        emit panel->sizeW()->editingFinished();
        QCOMPARE(r.document().historyDepth(), depth + 1);
        QCOMPARE(r.document().bodySizePx("t1")->w, 80.0);
        QCOMPARE(r.document().bodySizePx("t1")->h, 32.0);
        QCOMPARE(r.document().find(1)->pose.x, 590.0);
        QCOMPARE(r.document().components().size(), parts);
        for (const ComponentView& v : r.scene().snapshot->components) QVERIFY(std::abs(v.outline.width() - 80.0) < 1e-6);
        QVERIFY(r.hint().contains(QStringLiteral("已改大小")));
        QVERIFY(r.window.undoAction()->toolTip().contains(QStringLiteral("框大小")));

        // 太小：不收、吐司說明，表單的數字回到實際的值。
        panel->sizeH()->setValue(4);
        emit panel->sizeH()->editingFinished();
        QCOMPARE(r.document().historyDepth(), depth + 1);
        QCOMPARE(panel->sizeH()->value(), 32.0);
        QVERIFY(r.hint().contains(QStringLiteral("太小")));

        // 鎖定寬高比：改寬，高跟著變（80 : 32 ＝ 2.5）；送出的是兩個數字。
        panel->lockRatio()->setChecked(true);
        panel->sizeW()->setValue(40);
        QCOMPARE(panel->sizeH()->value(), 16.0);
        emit panel->sizeW()->editingFinished();
        QCOMPARE(r.document().bodySizePx("t1")->w, 40.0);
        QCOMPARE(r.document().bodySizePx("t1")->h, 16.0);

        // 中心 X：移動這一個元件（一個 MoveComponents），其他元件不動。
        const double otherX = r.document().find(2)->pose.x;
        panel->centerX()->setValue(600);
        emit panel->centerX()->editingFinished();
        QCOMPARE(r.document().find(1)->pose.x, 600.0);
        QCOMPARE(r.document().find(1)->pose.y, 376.0);
        QCOMPARE(r.document().find(2)->pose.x, otherX);
        QCOMPARE(r.document().historyDepth(), depth + 3);
        QVERIFY(r.window.undoAction()->toolTip().contains(zh("ui.history.move", {{"count", QStringLiteral("1")}})));
        // 同樣的值再按一次 Enter：沒有改變，不產生 Command。
        emit panel->centerX()->editingFinished();
        QCOMPARE(r.document().historyDepth(), depth + 3);
    }

    void applyToAllSamplesMakesEverySampleTheSameSizeAsOneCommand() {
        Rig r;
        r.open();
        QVERIFY(r.drawSample(QRectF(560, 360, 60, 32)));  // t1
        QVERIFY(r.drawSample(QRectF(200, 150, 40, 24)));  // t2（元件 2，中心 (220, 162)）
        QVERIFY(r.drawSample(QRectF(300, 500, 50, 30)));  // t3
        ObjectsPanel* panel = r.window.objectsPanel();
        r.clickComponent(0);  // X1：60 × 32
        QVERIFY(panel->applyToAll()->isEnabled());
        const std::size_t depth = r.document().historyDepth();
        panel->applyToAll()->click();
        QCOMPARE(r.document().historyDepth(), depth + 1);
        for (const char* t : {"t1", "t2", "t3"}) {
            QCOMPARE(r.document().bodySizePx(t)->w, 60.0);
            QCOMPARE(r.document().bodySizePx(t)->h, 32.0);
        }
        for (const SampleOnBoard& s : r.document().samples()) QCOMPARE(s.rect.w, 60.0);
        QCOMPARE(r.document().find(2)->pose.x, 220.0);  // 中心不動
        QVERIFY(r.window.undoAction()->toolTip().contains(zh("ui.history.resizeTypes", {{"count", QStringLiteral("2")}})));
        QVERIFY(r.hint().contains(QStringLiteral("2 個樣本")));
        // 樣本庫 dock 跟著：三筆都是 60 × 32。
        for (int i = 0; i < 3; ++i) QVERIFY(r.window.samplePanel()->list()->item(i)->text().contains(QStringLiteral("60 × 32 px")));
        // 再按一次：都一樣了，不進堆疊。
        panel->applyToAll()->click();
        QCOMPARE(r.document().historyDepth(), depth + 1);
        QCOMPARE(r.hint(), zh("ui.resize.nothingToApply"));
        r.window.undoAction()->trigger();
        QCOMPARE(r.document().bodySizePx("t2")->w, 40.0);
        QCOMPARE(r.document().bodySizePx("t3")->w, 50.0);
        QVERIFY(r.window.samplePanel()->list()->item(1)->text().contains(QStringLiteral("40 × 24 px")));
    }

    void theSamplePanelListsBoardSamplesWithThumbnailsAndClickingOneSelectsIt() {
        Rig r;
        r.open();
        SamplePanel* panel = r.window.samplePanel();
        QVERIFY(panel->emptyLabel()->isVisibleTo(panel));
        QCOMPARE(panel->emptyLabel()->text(), zh("ui.library.empty"));
        QVERIFY(!panel->list()->isVisibleTo(panel));

        QVERIFY(r.drawSample(QRectF(560, 360, 60, 32)));
        QVERIFY(r.drawSample(QRectF(200, 150, 40, 24)));
        QCOMPARE(panel->list()->count(), 2);
        QVERIFY(!panel->emptyLabel()->isVisibleTo(panel));
        QVERIFY(panel->headerLabel()->text().contains(QStringLiteral("2 個樣本")));
        const SampleListPtr list = r.presenter().lastSamples();
        QVERIFY(list != nullptr);
        QCOMPARE(list->entries.size(), std::size_t{2});
        QCOMPARE(list->entries[0].sceneId, 1);
        QCOMPARE(list->entries[0].name, QStringLiteral("X1"));
        QCOMPARE(list->entries[0].width, 60.0);
        QCOMPARE(list->entries[0].height, 32.0);
        QCOMPARE(list->entries[0].boxCount, std::size_t{1});
        QCOMPARE(list->entries[0].thumbnail.width(), 96);  // 縮到固定寬度；從影像裁下來的
        QVERIFY(list->entries[0].thumbnail.height() > 0);
        QVERIFY(panel->list()->item(0)->text().contains(QStringLiteral("60 × 32 px")));
        QVERIFY(panel->list()->item(1)->text().contains(QStringLiteral("X2")));
        QCOMPARE(panel->selectedIds(), std::vector<int>{2});  // 剛建的樣本是選取的：清單跟著

        // 點第一筆 ＝ 畫布選取 X1（物件樹與表單也跟著）。
        QTest::mouseClick(panel->list()->viewport(), Qt::LeftButton, {}, panel->list()->visualItemRect(panel->list()->item(0)).center());
        QCOMPARE(r.canvas()->selection(), std::vector<int>{1});
        QCOMPARE(r.window.objectsPanel()->sizeW()->value(), 60.0);
        QCOMPARE(panel->selectedIds(), std::vector<int>{1});

        // 改大小、復原：清單即時更新；門檻拖動不重做（同一份）。
        const SampleListPtr before = r.presenter().lastSamples();
        r.window.objectsPanel()->sizeW()->setValue(70);
        emit r.window.objectsPanel()->sizeW()->editingFinished();
        QVERIFY(r.presenter().lastSamples() != before);
        QVERIFY(panel->list()->item(0)->text().contains(QStringLiteral("70 × 32 px")));
        r.window.undoAction()->trigger();
        QVERIFY(panel->list()->item(0)->text().contains(QStringLiteral("60 × 32 px")));
        QVERIFY(r.searchAndWait());
        const SampleListPtr during = r.presenter().lastSamples();
        r.presenter().applyThreshold(0.80);
        QVERIFY(r.presenter().lastSamples() == during);
        QCOMPARE(panel->list()->count(), 2);  // 預覽中清單照舊是板子上的樣本

        // 子框：框數跟著變。
        r.key(Qt::Key_Escape);  // 捨棄提案
        QCOMPARE_STATE(r.state(), S::Idle);
        r.clickComponent(0);
        r.window.toolAction(ToolId::Box)->trigger();
        press(r.vp(), r.at(565, 365));
        move(r.vp(), r.at(575, 372));
        release(r.vp(), r.at(575, 372));
        QCOMPARE(r.presenter().lastSamples()->entries[0].boxCount, std::size_t{2});
        QVERIFY(panel->list()->item(0)->text().contains(QStringLiteral("2 個框")));
        QCOMPARE(r.window.objectsPanel()->boxesValue()->text(), QStringLiteral("2"));

        // 刪掉樣本：清單少一筆；開新影像：清單清空。
        r.key(Qt::Key_Delete);
        QCOMPARE(panel->list()->count(), 1);
        r.open();
        QCOMPARE(panel->list()->count(), 0);
        QVERIFY(panel->emptyLabel()->isVisibleTo(panel));
    }

    void aMultiSelectionDisablesTheFormAndAPreviewObjectCanOnlyBeMoved() {
        Rig r;
        r.open();
        QVERIFY(r.teachSample(QRectF(560, 360, 60, 32)));
        ObjectsPanel* panel = r.window.objectsPanel();
        // 預覽中選一個提案物件（索引 0 是板子上的樣本，預覽時只是背景）：只能改位置。
        r.clickComponent(1);
        QCOMPARE(r.canvas()->selection().size(), std::size_t{1});
        QVERIFY(isProposalId(r.canvas()->selection().front()));
        QVERIFY(panel->centerX()->isEnabled());
        QVERIFY(!panel->sizeW()->isEnabled());
        QVERIFY(!panel->lockRatio()->isEnabled());
        QVERIFY(!panel->applyToAll()->isEnabled());
        QVERIFY(panel->selectionNote()->isVisibleTo(panel));
        QCOMPARE(panel->selectionNote()->text(), zh("ui.selection.previewOnlyMove"));
        QVERIFY(panel->selectionSummary()->text().startsWith(QLatin1Char('#')));
        QCOMPARE(panel->sizeW()->value(), 60.0);  // 類型的主體框
        const double x = panel->centerX()->value();
        panel->centerX()->setValue(x + 5);
        emit panel->centerX()->editingFinished();
        QCOMPARE(r.document().components().size(), std::size_t{1});  // 板子沒動：改的是提案
        QCOMPARE(r.presenter().session().proposal()->editCount(), std::size_t{1});
        QCOMPARE(panel->centerX()->value(), x + 5);
        r.key(Qt::Key_Return);  // 接受

        // 多選：表單停用、只顯示「已選 N 個」；按鈕也停用。
        r.key(Qt::Key_A, Qt::ControlModifier);
        const std::size_t all = r.canvas()->selection().size();
        QVERIFY(all > 1);
        QCOMPARE(panel->selectionSummary()->text(), zh("ui.selection.multi", {{"count", QString::number(all)}}));
        QVERIFY(!panel->centerX()->isEnabled());
        QVERIFY(!panel->sizeW()->isEnabled());
        QVERIFY(!panel->applyToAll()->isEnabled());
        QVERIFY(panel->angleValue()->text().isEmpty());
        const std::size_t depth = r.document().historyDepth();
        panel->applyToAll()->click();  // 停用的按鈕：什麼都不做
        emit panel->sizeW()->editingFinished();
        QCOMPARE(r.document().historyDepth(), depth);
        r.key(Qt::Key_Escape);  // 清除選取
        QCOMPARE(panel->selectionSummary()->text(), zh("ui.selection.none"));
    }
};

QTEST_MAIN(TstPresenter)
#include "tst_presenter.moc"
