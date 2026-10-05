#include <QApplication>
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
    void registryHasTheFourDefaultTools() {
        ToolRegistry r = ToolRegistry::makeDefault();
        QCOMPARE(static_cast<int>(r.ids().size()), 4);
        for (ToolId id : {ToolId::Select, ToolId::Sample, ToolId::Region, ToolId::Add}) {
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
