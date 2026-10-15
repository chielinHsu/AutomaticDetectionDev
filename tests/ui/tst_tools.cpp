#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <cmath>
#include <memory>

#include "TestSupport.h"
#include "presentation/canvas/ImageCanvas.h"
#include "presentation/dev/FakeScene.h"
#include "presentation/theme/ThemeTokens.h"
#include "presentation/tools/ToolRegistry.h"
#include "presentation/tools/Tools.h"

using namespace aoi::ui;
using namespace aoi::test;

namespace {

const ThemeTokens& tokens() { return ThemeTokens::builtin(); }

constexpr int kBoardW = 1200;
constexpr int kBoardH = 800;

// 板子上有兩個元件：A（編號 1）與 B（編號 2）。
SnapshotPtr twoComponents() {
    auto s = std::make_shared<ComponentsSnapshot>();
    auto make = [](int id, QRectF r) {
        ComponentView c;
        c.id = id;
        c.outline = r;
        c.boxes.push_back({r, BoxKind::Body});
        return c;
    };
    s->components = {make(1, QRectF(100, 100, 200, 100)), make(2, QRectF(500, 300, 150, 80))};
    return s;
}

struct Fixture {
    ImageCanvas canvas{tokens()};
    RecordingController rec;
    aoi::dev::FakeBoard board;

    Fixture() {
        canvas.resize(1000, 700);
        canvas.show();
        Q_UNUSED(QTest::qWaitForWindowExposed(&canvas));
        board = aoi::dev::makeFakeBoard(kBoardW, kBoardH);
        canvas.setImage(board.toQImage());
        canvas.showComponents(twoComponents());
        canvas.setController(&rec);
        canvas.setSelectionController(&rec);
    }
    QWidget* vp() { return canvas.viewport(); }
    QPointF at(double x, double y) { return toView(canvas, QPointF(x, y)); }
};

bool near(const QRectF& a, const QRectF& b, double tol = 0.6) {
    return std::abs(a.left() - b.left()) <= tol && std::abs(a.top() - b.top()) <= tol &&
           std::abs(a.width() - b.width()) <= tol && std::abs(a.height() - b.height()) <= tol;
}

}  // namespace

class TstTools : public QObject {
    Q_OBJECT
private slots:
    // ---- 註冊表 ----
    void registryHasTheSixDefaultTools() {
        ToolRegistry r = ToolRegistry::makeDefault();
        QCOMPARE(static_cast<int>(r.ids().size()), 6);  // ADR-026 起多 M（比對區）
        for (ToolId id : {ToolId::Select, ToolId::Sample, ToolId::Region, ToolId::Add, ToolId::Box, ToolId::Match}) {
            QVERIFY(r.find(id));
            QCOMPARE(r.find(id)->id(), id);
        }
        QCOMPARE(r.find(ToolId::Select)->cursor(), Qt::ArrowCursor);
        QCOMPARE(r.find(ToolId::Sample)->cursor(), Qt::CrossCursor);
    }

    void registeringTheSameIdReplacesTheTool() {
        ToolRegistry r;
        QVERIFY(!r.find(ToolId::Sample));
        r.registerTool(std::make_unique<SampleRoiTool>());
        ITool* first = r.find(ToolId::Sample);
        r.registerTool(std::make_unique<SampleRoiTool>());
        QVERIFY(r.find(ToolId::Sample));
        QCOMPARE(static_cast<int>(r.ids().size()), 1);
        Q_UNUSED(first);
    }

    // ---- S：教樣本 ----
    void sampleToolReportsTheNormalizedRectInImagePixels() {
        Fixture f;
        f.canvas.setInteraction(ToolId::Sample);
        // 反向拖曳（右下 → 左上）：矩形要正規化。
        press(f.vp(), f.at(300, 200));
        move(f.vp(), f.at(270, 180));
        QVERIFY(f.canvas.draftItem()->isVisible());  // 拖曳中即時顯示
        move(f.vp(), f.at(240, 150));
        release(f.vp(), f.at(240, 150));
        QCOMPARE(static_cast<int>(f.rec.regions.size()), 1);
        QCOMPARE(f.rec.regions[0].mode, DraftMode::Sample);
        QVERIFY2(near(f.rec.regions[0].rect, QRectF(240, 150, 60, 50)),
                 qPrintable(QStringLiteral("%1,%2 %3x%4").arg(f.rec.regions[0].rect.x()).arg(f.rec.regions[0].rect.y())
                                .arg(f.rec.regions[0].rect.width()).arg(f.rec.regions[0].rect.height())));
        QCOMPARE(f.rec.aborted, 0);
        QVERIFY(f.canvas.draftItem()->isVisible());  // 放開後框留著：搜尋中與預覽時要讓人看到
        QVERIFY(near(f.canvas.draftItem()->rect(), f.rec.regions[0].rect));
    }

    void pressShowsFeedbackImmediatelyAsACross() {
        Fixture f;
        f.canvas.setInteraction(ToolId::Sample);
        press(f.vp(), f.at(300, 200));
        QVERIFY(f.canvas.draftItem()->isVisible());
        QCOMPARE(f.canvas.draftItem()->rect().width(), 0.0);  // 退化矩形：畫成十字記號
        release(f.vp(), f.at(300, 200));
    }

    void regionToolUsesTheRegionMode() {
        Fixture f;
        f.canvas.setInteraction(ToolId::Region);
        press(f.vp(), f.at(100, 100));
        move(f.vp(), f.at(400, 300));
        release(f.vp(), f.at(400, 300));
        QCOMPARE(static_cast<int>(f.rec.regions.size()), 1);
        QCOMPARE(f.rec.regions[0].mode, DraftMode::Region);
        QVERIFY(near(f.rec.regions[0].rect, QRectF(100, 100, 300, 200)));
        QCOMPARE(f.canvas.draftItem()->look(), RoiGraphicsItem::Look::RegionDraft);
    }

    void dragPastTheImageEdgeStopsAtTheEdge() {
        Fixture f;
        f.canvas.setInteraction(ToolId::Sample);
        press(f.vp(), f.at(1100, 700));
        move(f.vp(), f.at(1500, 1200));
        release(f.vp(), f.at(1500, 1200));
        QCOMPARE(static_cast<int>(f.rec.regions.size()), 1);
        QVERIFY(near(f.rec.regions[0].rect, QRectF(1100, 700, 100, 100)));  // 右下停在 (1200, 800)
    }

    void pressOutsideTheImageDoesNotStartADrag() {
        Fixture f;
        f.canvas.setInteraction(ToolId::Sample);
        press(f.vp(), QPointF(3, 3));  // 適配視窗的邊界裡，影像外
        move(f.vp(), QPointF(200, 200));
        release(f.vp(), QPointF(200, 200));
        QVERIFY(f.rec.regions.empty());
        QVERIFY(!f.canvas.draftItem()->isVisible());
    }

    void escapeDuringTheDragAbortsAndHidesTheDraft() {
        Fixture f;
        f.canvas.setInteraction(ToolId::Sample);
        press(f.vp(), f.at(100, 100));
        move(f.vp(), f.at(200, 200));
        sendKey(&f.canvas, Qt::Key_Escape);
        QCOMPARE(f.rec.aborted, 1);
        QVERIFY(!f.canvas.draftItem()->isVisible());
        release(f.vp(), f.at(200, 200));  // 之後放開不再回報
        QVERIFY(f.rec.regions.empty());
    }

    void switchingToolsMidDragAborts() {
        Fixture f;
        f.canvas.setInteraction(ToolId::Sample);
        press(f.vp(), f.at(100, 100));
        move(f.vp(), f.at(200, 200));
        f.canvas.setInteraction(ToolId::Region);
        QCOMPARE(f.rec.aborted, 1);
        QVERIFY(!f.canvas.draftItem()->isVisible());
    }

    void aTinyDragIsStillReportedSoThePresenterCanSayTooSmall() {
        Fixture f;
        f.canvas.setInteraction(ToolId::Sample);
        press(f.vp(), f.at(100, 100));
        release(f.vp(), f.at(102, 101));
        QCOMPARE(static_cast<int>(f.rec.regions.size()), 1);
        QVERIFY(f.rec.regions[0].rect.width() < tokens().interaction.minSampleSidePx);
    }

    void dragFrameKeepsTheViewUntouched() {
        Fixture f;
        f.canvas.setInteraction(ToolId::Sample);
        const double scale = f.canvas.viewScale();
        const QPointF origin = f.at(0, 0);
        press(f.vp(), f.at(100, 100));
        move(f.vp(), f.at(500, 500));
        release(f.vp(), f.at(500, 500));
        QCOMPARE(f.canvas.viewScale(), scale);
        QCOMPARE(f.at(0, 0), origin);
    }

    // ---- A：補一個 ----
    void addToolReportsTheClickedImagePoint() {
        Fixture f;
        f.canvas.setInteraction(ToolId::Add);
        press(f.vp(), f.at(321, 123));
        release(f.vp(), f.at(321, 123));
        QCOMPARE(static_cast<int>(f.rec.adds.size()), 1);
        QVERIFY(std::abs(f.rec.adds[0].x() - 321) < 0.01 && std::abs(f.rec.adds[0].y() - 123) < 0.01);
        press(f.vp(), QPointF(3, 3));
        release(f.vp(), QPointF(3, 3));
        QCOMPARE(static_cast<int>(f.rec.adds.size()), 1);  // 影像外不算
    }

    // ---- V：選取 ----
    void selectToolSelectsTheWholeComponent() {
        Fixture f;
        press(f.vp(), f.at(150, 120));  // A 的內部
        release(f.vp(), f.at(150, 120));
        QCOMPARE(static_cast<int>(f.rec.selections.size()), 1);
        QCOMPARE(f.rec.selections.back(), (std::vector<int>{1}));
    }

    void shiftClickAddsAndRemoves() {
        Fixture f;
        f.canvas.setSelection({1});  // Presenter 把結果回送給畫布
        press(f.vp(), f.at(550, 330), Qt::LeftButton, Qt::ShiftModifier);
        release(f.vp(), f.at(550, 330));
        QCOMPARE(f.rec.selections.back(), (std::vector<int>{1, 2}));
        f.canvas.setSelection({1, 2});
        press(f.vp(), f.at(150, 120), Qt::LeftButton, Qt::ShiftModifier);
        release(f.vp(), f.at(150, 120));
        QCOMPARE(f.rec.selections.back(), (std::vector<int>{2}));
    }

    void clickingEmptySpaceClearsTheSelectionOnlyWhenSomethingIsSelected() {
        Fixture f;
        press(f.vp(), f.at(900, 700));
        release(f.vp(), f.at(900, 700));
        QVERIFY(f.rec.selections.empty());  // 本來就沒選：不呼叫
        f.canvas.setSelection({2});
        press(f.vp(), f.at(900, 700));
        release(f.vp(), f.at(900, 700));
        QCOMPARE(static_cast<int>(f.rec.selections.size()), 1);
        QVERIFY(f.rec.selections.back().empty());
        // Shift＋點空白處不動：避免手滑丟掉多選。
        f.canvas.setSelection({2});
        press(f.vp(), f.at(900, 700), Qt::LeftButton, Qt::ShiftModifier);
        release(f.vp(), f.at(900, 700));
        QCOMPARE(static_cast<int>(f.rec.selections.size()), 1);
    }

    void doubleClickOpensTheTypeEditorOfThatComponent() {
        Fixture f;
        doubleClick(f.vp(), f.at(560, 340));
        QCOMPARE(static_cast<int>(f.rec.editors.size()), 1);
        QCOMPARE(f.rec.editors[0].first, 2);
        doubleClick(f.vp(), f.at(900, 700));  // 空白處不開
        QCOMPARE(static_cast<int>(f.rec.editors.size()), 1);
    }

    void escapeClearsTheSelectionAndDeleteRemovesIt() {
        Fixture f;
        sendKey(&f.canvas, Qt::Key_Delete);  // 沒選：什麼都不做
        QCOMPARE(f.rec.deletes, 0);
        f.canvas.setSelection({1});
        sendKey(&f.canvas, Qt::Key_Escape);
        QCOMPARE(static_cast<int>(f.rec.selections.size()), 1);
        QVERIFY(f.rec.selections.back().empty());
        f.canvas.setSelection({1});
        sendKey(&f.canvas, Qt::Key_Delete);
        QCOMPARE(f.rec.deletes, 1);
        sendKey(&f.canvas, Qt::Key_Backspace);
        QCOMPARE(f.rec.deletes, 2);
    }

    // ---- V：拖曳移動（S04）----
    void draggingAComponentMovesItByTheDeltaInImagePixels() {
        Fixture f;
        press(f.vp(), f.at(150, 120));  // A（編號 1）還沒選取：先選取再拖
        QCOMPARE(f.rec.selections.back(), (std::vector<int>{1}));
        f.canvas.setSelection({1});     // Presenter 把選取回送給畫布
        move(f.vp(), f.at(180, 140));
        move(f.vp(), f.at(250, 160));
        release(f.vp(), f.at(250, 160));
        QCOMPARE(static_cast<int>(f.rec.moves.size()), 1);          // 一次拖曳 ＝ 一次回報（一個 Command）
        QCOMPARE(f.rec.moves[0].first, (std::vector<int>{1}));
        QVERIFY2(std::abs(f.rec.moves[0].second.x() - 100) <= 0.6 && std::abs(f.rec.moves[0].second.y() - 40) <= 0.6,
                 qPrintable(QStringLiteral("%1,%2").arg(f.rec.moves[0].second.x()).arg(f.rec.moves[0].second.y())));
        QVERIFY(!f.canvas.draftItem()->isVisible());                 // 放開後外框收起來
    }

    void aGhostOutlineShowsWhereTheComponentsWillLand() {
        Fixture f;
        press(f.vp(), f.at(150, 120));
        f.canvas.setSelection({1});
        move(f.vp(), f.at(200, 170));
        QVERIFY(f.canvas.draftItem()->isVisible());
        QVERIFY(near(f.canvas.draftItem()->rect(), QRectF(150, 150, 200, 100)));  // A 的外框 (100,100,200,100) 右下各移 50
        QCOMPARE(f.canvas.draftItem()->look(), RoiGraphicsItem::Look::RegionDraft);
        release(f.vp(), f.at(200, 170));
    }

    void draggingASelectedComponentMovesTheWholeSelection() {
        Fixture f;
        f.canvas.setSelection({1, 2});
        press(f.vp(), f.at(150, 120));  // 按在已選取的 A 上：整個選取一起走，選取不變
        QVERIFY(f.rec.selections.empty());
        move(f.vp(), f.at(170, 130));
        release(f.vp(), f.at(170, 130));
        QCOMPARE(static_cast<int>(f.rec.moves.size()), 1);
        QCOMPARE(f.rec.moves[0].first, (std::vector<int>{1, 2}));
    }

    void aSmallWobbleIsAClickNotADrag() {
        Fixture f;
        press(f.vp(), f.at(150, 120));
        move(f.vp(), f.at(151, 121));  // 約 1 個螢幕像素：手抖
        release(f.vp(), f.at(151, 121));
        QVERIFY(f.rec.moves.empty());
        QCOMPARE(f.rec.selections.back(), (std::vector<int>{1}));
        QVERIFY(!f.canvas.draftItem()->isVisible());
    }

    void shiftClickNeverStartsADrag() {
        Fixture f;
        f.canvas.setSelection({1});
        press(f.vp(), f.at(550, 330), Qt::LeftButton, Qt::ShiftModifier);
        move(f.vp(), f.at(700, 400));
        release(f.vp(), f.at(700, 400));
        QVERIFY(f.rec.moves.empty());
    }

    void escapeDuringADragCancelsItAndReportsNoMovement() {
        Fixture f;
        press(f.vp(), f.at(150, 120));
        f.canvas.setSelection({1});
        move(f.vp(), f.at(250, 160));
        QVERIFY(f.canvas.draftItem()->isVisible());
        sendKey(&f.canvas, Qt::Key_Escape);
        QVERIFY(!f.canvas.draftItem()->isVisible());
        QCOMPARE(static_cast<int>(f.rec.moves.size()), 1);
        QVERIFY(f.rec.moves[0].second.isNull());  // 位移 0：Presenter 還原被蓋掉的畫面，不產生 Command
        release(f.vp(), f.at(250, 160));
        QCOMPARE(static_cast<int>(f.rec.moves.size()), 1);  // 放開不再回報
        QVERIFY(f.rec.selections.size() == 1);              // Esc 也沒有清掉選取（工具用掉了這個鍵）
    }

    void switchingToolsMidDragCancelsTheMove() {
        Fixture f;
        press(f.vp(), f.at(150, 120));
        f.canvas.setSelection({1});
        move(f.vp(), f.at(250, 160));
        f.canvas.setInteraction(ToolId::Region);
        QVERIFY(!f.canvas.draftItem()->isVisible());
        QCOMPARE(static_cast<int>(f.rec.moves.size()), 1);
        QVERIFY(f.rec.moves[0].second.isNull());
    }

    // ---- ADR-025：把手改大小、框選、B 工具 ----
    void draggingAHandleOfTheSingleSelectedComponentReportsTheNewRect() {
        Fixture f;
        QVERIFY(!f.canvas.handleItem()->isVisible());
        f.canvas.setSelection({1});  // A：(100,100,200,100)，螢幕上 ≥ 72 px → 8 個把手在邊上
        QVERIFY(f.canvas.handleItem()->isVisible());
        QCOMPARE(f.canvas.handleItem()->rect(), QRectF(100, 100, 200, 100));
        press(f.vp(), f.at(300, 200));  // 右下角的把手
        QVERIFY(f.rec.selections.empty());  // 不是點擊元件：選取不變
        move(f.vp(), f.at(320, 210));
        QVERIFY(f.canvas.draftItem()->isVisible());  // 拖曳中畫新的框
        QCOMPARE(f.canvas.draftItem()->look(), RoiGraphicsItem::Look::SampleDraft);
        move(f.vp(), f.at(340, 220));
        release(f.vp(), f.at(340, 220));
        QCOMPARE(static_cast<int>(f.rec.resizes.size()), 1);
        QCOMPARE(f.rec.resizes[0].first, 1);
        QVERIFY2(near(f.rec.resizes[0].second, QRectF(100, 100, 240, 120)), qPrintable(QString::number(f.rec.resizes[0].second.width())));
        QVERIFY(f.rec.moves.empty());
        QVERIFY(!f.canvas.draftItem()->isVisible());

        // 拖左邊的把手：只有左邊動；翻過去也不會是負的寬。
        press(f.vp(), f.at(100, 150));
        move(f.vp(), f.at(150, 150));
        release(f.vp(), f.at(150, 150));
        QCOMPARE(static_cast<int>(f.rec.resizes.size()), 2);
        QVERIFY(near(f.rec.resizes[1].second, QRectF(150, 100, 150, 100)));

        f.canvas.setSelection({1, 2});  // 多選：沒有把手
        QVERIFY(!f.canvas.handleItem()->isVisible());
        f.canvas.setSelection({});
        QVERIFY(!f.canvas.handleItem()->isVisible());
    }

    void draggingOnEmptySpaceSelectsTheComponentsInTheBox() {
        Fixture f;
        press(f.vp(), f.at(400, 50));  // 空白處
        move(f.vp(), f.at(550, 200));
        QVERIFY(f.canvas.draftItem()->isVisible());  // 框選中的框
        QCOMPARE(f.canvas.draftItem()->look(), RoiGraphicsItem::Look::RegionDraft);
        move(f.vp(), f.at(700, 400));
        release(f.vp(), f.at(700, 400));
        QCOMPARE(f.rec.selections.back(), (std::vector<int>{2}));  // 只有 B 和框相交
        QVERIFY(!f.canvas.draftItem()->isVisible());
        QVERIFY(f.rec.moves.empty());

        // Shift＋框選：加入既有的選取。
        f.canvas.setSelection({1});
        press(f.vp(), f.at(400, 50), Qt::LeftButton, Qt::ShiftModifier);
        move(f.vp(), f.at(700, 400));
        release(f.vp(), f.at(700, 400));
        QCOMPARE(f.rec.selections.back(), (std::vector<int>{2, 1}));

        // 框到什麼都沒有：選取清空。
        press(f.vp(), f.at(700, 50));
        move(f.vp(), f.at(900, 90));
        release(f.vp(), f.at(900, 90));
        QVERIFY(f.rec.selections.back().empty());
        QVERIFY(f.canvas.componentsIn(QRectF(0, 0, 1200, 800)).size() == 2);
    }

    void theBoxToolReportsTheRectWithTheBoxMode() {
        Fixture f;
        f.canvas.setInteraction(ToolId::Box);
        QCOMPARE(f.canvas.activeTool(), ToolId::Box);
        press(f.vp(), f.at(120, 120));
        move(f.vp(), f.at(160, 150));
        QVERIFY(f.canvas.draftItem()->isVisible());
        release(f.vp(), f.at(160, 150));
        QCOMPARE(static_cast<int>(f.rec.regions.size()), 1);
        QVERIFY(f.rec.regions[0].mode == DraftMode::Box);
        QVERIFY(near(f.rec.regions[0].rect, QRectF(120, 120, 40, 30)));
        QVERIFY(f.rec.selections.empty());  // 不是選取
    }

    void outlineOfUnitesTheOutlinesAndIgnoresUnknownIds() {
        Fixture f;
        QCOMPARE(f.canvas.outlineOf({1, 2}), QRectF(100, 100, 200, 100).united(QRectF(500, 300, 150, 80)));
        QCOMPARE(f.canvas.outlineOf({2}), QRectF(500, 300, 150, 80));
        QVERIFY(f.canvas.outlineOf({99}).isNull());
        QVERIFY(f.canvas.outlineOf({}).isNull());
    }

    // ---- 畫布的輸入閘門與鍵盤（S04）----
    void pausedCanvasIgnoresThePointerButStillZooms() {
        Fixture f;
        f.canvas.setInteraction(ToolId::Sample);
        f.canvas.setToolsEnabled(false);
        press(f.vp(), f.at(300, 200));
        move(f.vp(), f.at(400, 300));
        release(f.vp(), f.at(400, 300));
        QVERIFY(f.rec.regions.empty());
        QVERIFY(!f.canvas.draftItem()->isVisible());
        const double before = f.canvas.zoomPercent();
        sendWheel(f.vp(), QPointF(f.vp()->width() / 2.0, f.vp()->height() / 2.0), 2);
        QVERIFY(f.canvas.zoomPercent() > before);  // 縮放與平移是檢視狀態，不受閘門影響
        f.canvas.setToolsEnabled(true);
        press(f.vp(), f.at(300, 200));
        release(f.vp(), f.at(400, 300));
        QCOMPARE(static_cast<int>(f.rec.regions.size()), 1);
    }

    void pausingTheCanvasAbandonsADragInProgress() {
        Fixture f;
        f.canvas.setInteraction(ToolId::Sample);
        press(f.vp(), f.at(300, 200));
        move(f.vp(), f.at(350, 230));
        QVERIFY(f.canvas.draftItem()->isVisible());
        f.canvas.setToolsEnabled(false);
        QVERIFY(!f.canvas.draftItem()->isVisible());
        QCOMPARE(f.rec.aborted, 1);
        release(f.vp(), f.at(350, 230));
        QVERIFY(f.rec.regions.empty());  // 放開時已經沒有可回報的框
    }

    void theBusyCursorOverridesTheToolCursorAndComesBack() {
        Fixture f;
        QCOMPARE(f.vp()->cursor().shape(), Qt::ArrowCursor);
        f.canvas.setBusyCursor(true);
        QCOMPARE(f.vp()->cursor().shape(), Qt::BusyCursor);
        f.canvas.setBusyCursor(false);
        QCOMPARE(f.vp()->cursor().shape(), Qt::ArrowCursor);
    }

    void unhandledKeysGoToTheKeyboardControllerFirst() {
        struct Keys final : IKeyboardController {
            std::vector<std::pair<int, Qt::KeyboardModifiers>> keys;
            bool consume = true;
            bool onKeyPressed(int key, Qt::KeyboardModifiers m) override {
                keys.push_back({key, m});
                return consume;
            }
        } keys;
        Fixture f;
        f.canvas.setKeyboardController(&keys);
        f.canvas.setSelection({1});
        sendKey(&f.canvas, Qt::Key_Return);
        sendKey(&f.canvas, Qt::Key_Right, QEvent::KeyPress, Qt::ShiftModifier);
        QCOMPARE(static_cast<int>(keys.keys.size()), 2);
        QCOMPARE(keys.keys[0].first, static_cast<int>(Qt::Key_Return));
        QVERIFY(keys.keys[1].second.testFlag(Qt::ShiftModifier));

        // 有鍵盤 controller 時，Esc 與 Del 歸它，畫布自己的備援（清除選取、刪除）不再動作。
        sendKey(&f.canvas, Qt::Key_Escape);
        sendKey(&f.canvas, Qt::Key_Delete);
        QVERIFY(f.rec.selections.empty());
        QCOMPARE(f.rec.deletes, 0);
        f.canvas.setKeyboardController(nullptr);
    }

    void aKeyTheKeyboardControllerDeclinesFallsBackToTheCanvasDefaults() {
        struct Decline final : IKeyboardController {
            bool onKeyPressed(int, Qt::KeyboardModifiers) override { return false; }
        } decline;
        Fixture f;
        f.canvas.setKeyboardController(&decline);
        f.canvas.setSelection({1});
        sendKey(&f.canvas, Qt::Key_Escape);
        // 有鍵盤 controller（即使它不處理）時，備援不動作：責任在 Presenter，畫布不替它猜。
        QVERIFY(f.rec.selections.empty());
        f.canvas.setKeyboardController(nullptr);
        sendKey(&f.canvas, Qt::Key_Escape);
        QCOMPARE(static_cast<int>(f.rec.selections.size()), 1);
    }

    void pausedCanvasStillForwardsEscToTheKeyboardController() {
        struct Keys final : IKeyboardController {
            int calls = 0;
            bool onKeyPressed(int key, Qt::KeyboardModifiers) override {
                if (key == Qt::Key_Escape) ++calls;
                return key == Qt::Key_Escape;
            }
        } keys;
        Fixture f;
        f.canvas.setKeyboardController(&keys);
        f.canvas.setToolsEnabled(false);
        sendKey(&f.canvas, Qt::Key_Escape);  // 搜尋中：Esc 取消搜尋，要送得到 Presenter
        QCOMPARE(keys.calls, 1);
        f.canvas.setKeyboardController(nullptr);
    }

    void activeToolChangesAreAnnounced() {
        Fixture f;
        QSignalSpy spy(&f.canvas, &ImageCanvas::activeToolChanged);
        f.canvas.setInteraction(ToolId::Region);
        f.canvas.setInteraction(ToolId::Region);  // 同一個：不重複通知
        f.canvas.setInteraction(ToolId::Select);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).value<ToolId>(), ToolId::Region);
    }

    void missingControllersAreTolerated() {
        Fixture f;
        f.canvas.setController(nullptr);
        f.canvas.setSelectionController(nullptr);
        f.canvas.setInteraction(ToolId::Sample);
        press(f.vp(), f.at(100, 100));
        release(f.vp(), f.at(200, 200));
        f.canvas.setInteraction(ToolId::Select);
        press(f.vp(), f.at(150, 120));
        doubleClick(f.vp(), f.at(150, 120));
        sendKey(&f.canvas, Qt::Key_Escape);
        sendKey(&f.canvas, Qt::Key_Delete);
        QVERIFY(true);  // 沒有當機
    }
};

QTEST_MAIN(TstTools)
#include "tst_tools.moc"
