// A/B 比較的畫面（S09）：主視窗的入口、比較視窗、兩個 back end 依序搜尋同一個樣本、疊圖與表格、門檻分開、取消、失敗、語言。
// 走產品的接線（PresenterRig：core → MainWindow → AppPresenters）。Rig 的 registry 有 fake、broken（工廠失敗）、
// other（同一個假世界、自己的探針）、off（不可用）。
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QtTest>

#include "PresenterRig.h"
#include "QtTestPrinters.h"
#include "presentation/compare/CompareOverlayItem.h"
#include "presentation/compare/ComparePresenter.h"
#include "presentation/compare/CompareWindow.h"

using namespace aoi;
using namespace aoi::ui;
using namespace aoi::test;

namespace {

ComparePresenter& compare(Rig& rig) { return rig.presenters->compare(); }
CompareWindow* compareWindow(Rig& rig) { return rig.window.compareWindow(); }

// 在比較視窗的兩個選單選好 A、B，按〔比較〕，等結果。打開視窗時的那一次比較還在跑的話，先等它結束（比較中按鈕與選單停用）。
bool runFromWindow(Rig& rig, const QString& a, const QString& b) {
    CompareWindow* w = compareWindow(rig);
    if (!QTest::qWaitFor([&] { return !compare(rig).running(); }, 8000)) return false;
    if (!w->runButton()->isEnabled()) return false;
    w->backendA()->setCurrentIndex(w->backendA()->findData(a));
    w->backendB()->setCurrentIndex(w->backendB()->findData(b));
    QSignalSpy finished(&compare(rig), &ComparePresenter::compareFinished);
    w->runButton()->click();
    return finished.count() > 0 || finished.wait(8000);
}

int rowOf(QTableWidget* table, const QString& label) {
    for (int r = 0; r < table->rowCount(); ++r) {
        if (table->item(r, 0) && table->item(r, 0)->text() == label) return r;
    }
    return -1;
}

}  // namespace

class TstCompare : public QObject {
    Q_OBJECT

private slots:
    void init() { Translations::install(QStringLiteral("zh_TW")); }

    // 入口：有樣本（預覽中）才能按；不能按時，提示說怎麼做。
    void theEntryNeedsASample() {
        Rig rig;
        QAction* action = rig.window.compareAction();
        QVERIFY(!action->isEnabled());
        QCOMPARE(action->toolTip(), uiText("ui.compare.entry.needSample"));
        rig.open();
        QVERIFY(!action->isEnabled());
        QVERIFY(rig.teachSample());
        QVERIFY(action->isEnabled());
        QCOMPARE(action->toolTip(), uiText("ui.compare.entry.tip"));
        rig.presenter().discard();  // 回到 Idle：沒有樣本了
        QTRY_VERIFY(!action->isEnabled());
    }

    // 預設的 A、B：註冊順序由簡單到完整，主視窗用的是最簡單的（fake）而且還有兩個以上時，比後面兩個（broken、other）。
    // broken 的工廠失敗（像授權）：比較視窗說是哪一個、為什麼，不當掉、主視窗不受影響。
    void openingUsesTheDefaultPairAndReportsAFailedBackEnd() {
        Rig rig;
        rig.open();
        QVERIFY(rig.teachSample());
        QCOMPARE(compare(rig).defaultPair(), (std::pair<std::string, std::string>{"broken", "other"}));
        QSignalSpy finished(&compare(rig), &ComparePresenter::compareFinished);
        rig.window.compareAction()->trigger();
        QVERIFY(compareWindow(rig)->isVisible());
        QVERIFY(finished.wait(8000));
        QCOMPARE(finished.last().at(0).toBool(), false);
        const QString status = compareWindow(rig)->statusLabel()->text();
        QVERIFY2(status.contains(QStringLiteral("Broken (test)")) && status.contains(QStringLiteral("no license (test)")), qPrintable(status));
        QVERIFY(compareWindow(rig)->runButton()->isEnabled());
        QCOMPARE_STATE(rig.state(), InteractionState::Preview);  // 主視窗照常
    }

    // 同一個樣本、兩個 back end（同一個假世界）：全部都是「兩邊都找到」，表格有各階段的耗時與數量，疊圖每個元件一個標記。
    void twoBackEndsOnTheSameSample() {
        Rig rig;
        rig.open();
        QVERIFY(rig.teachSample());
        rig.window.compareAction()->trigger();
        QVERIFY(runFromWindow(rig, QStringLiteral("fake"), QStringLiteral("other")));
        QCOMPARE(compare(rig).idA(), std::string("fake"));
        QCOMPARE(compare(rig).idB(), std::string("other"));
        QVERIFY(compare(rig).comparison().has_value());
        const CompareSummary& s = compare(rig).comparison()->summary;
        QVERIFY(s.both > 0);
        QCOMPARE(s.onlyA, std::size_t{0});
        QCOMPARE(s.onlyB, std::size_t{0});
        QCOMPARE(s.both, s.a.found);
        QVERIFY(rig.otherProbe->finds.load() > 0);  // B 真的搜尋了

        CompareWindow* w = compareWindow(rig);
        QVERIFY(w->windowTitle().contains(QStringLiteral("Fake (test)")) && w->windowTitle().contains(QStringLiteral("Other (test)")));
        QCOMPARE(w->overlayItem()->overlay()->marks.size(), s.both);
        QCOMPARE(w->summaryTable()->rowCount(), 11);
        const int found = rowOf(w->summaryTable(), uiText("ui.compare.row.found"));
        QVERIFY(found >= 0);
        QCOMPARE(w->summaryTable()->item(found, 1)->text(), QString::number(s.a.found));
        QCOMPARE(w->summaryTable()->item(found, 3)->text(), QStringLiteral("0"));
        QCOMPARE(w->differencesTable()->rowCount(), 0);
        QCOMPARE(w->legendCheck(CompareKind::Both)->text(), uiText("ui.compare.kind.both", {{"n", QString::number(s.both)}}));
        QVERIFY(!w->busy());
        QCOMPARE_STATE(rig.state(), InteractionState::Preview);  // 比較不動主視窗的提案
    }

    // 門檻兩邊各一個：把 B 的門檻拉高，原本兩邊都有的變成「只有 A」，原因是 B 的分數低於它的門檻。只重新比較，不重新搜尋。
    void eachSideHasItsOwnThresholdWithoutANewSearch() {
        Rig rig;
        rig.open();
        QVERIFY(rig.teachSample());
        rig.window.compareAction()->trigger();
        QVERIFY(runFromWindow(rig, QStringLiteral("fake"), QStringLiteral("other")));
        const int finds = rig.otherProbe->finds.load();
        const int findsA = rig.probe->finds.load();
        CompareWindow* w = compareWindow(rig);
        w->thresholdB()->setValue(0.95);
        QVERIFY(compare(rig).comparison().has_value());
        const CompareSummary& s = compare(rig).comparison()->summary;
        QVERIFY(s.onlyA > 0);
        QCOMPARE(s.onlyA, s.onlyABelowThreshold);
        QCOMPARE(rig.otherProbe->finds.load(), finds);
        QCOMPARE(rig.probe->finds.load(), findsA);
        QCOMPARE(w->differencesTable()->rowCount(), static_cast<int>(s.onlyA));
        const QString reason = w->differencesTable()->item(0, 4)->text();
        QVERIFY2(reason.contains(QStringLiteral("Other (test)")) && reason.contains(QStringLiteral("0.95")), qPrintable(reason));
        QCOMPARE(w->overlayItem()->overlay()->count(CompareKind::OnlyA), s.onlyA);
        // 選一個差異：疊圖強調它，畫布移過去。
        w->differencesTable()->setCurrentCell(0, 0);
        QVERIFY(w->overlayItem()->highlight() >= 0);
        QVERIFY(w->overlayItem()->overlay()->marks[static_cast<std::size_t>(w->overlayItem()->highlight())].kind == CompareKind::OnlyA);
    }

    // 圖例的勾選控制疊圖：不勾的那一類不畫。
    void theLegendFiltersTheOverlay() {
        Rig rig;
        rig.open();
        QVERIFY(rig.teachSample());
        rig.window.compareAction()->trigger();
        QVERIFY(runFromWindow(rig, QStringLiteral("fake"), QStringLiteral("other")));
        CompareWindow* w = compareWindow(rig);
        w->thresholdB()->setValue(0.95);  // 有兩種標記
        w->canvas()->viewport()->repaint();
        const int all = w->overlayItem()->lastPaintStats().marksDrawn;
        QCOMPARE(static_cast<std::size_t>(all), w->overlayItem()->overlay()->marks.size());
        w->legendCheck(CompareKind::Both)->setChecked(false);
        QVERIFY(!w->overlayItem()->isKindVisible(CompareKind::Both));
        w->canvas()->viewport()->repaint();
        QCOMPARE(static_cast<std::size_t>(w->overlayItem()->lastPaintStats().marksDrawn), w->overlayItem()->overlay()->count(CompareKind::OnlyA));
    }

    // 取消：B 的視窗驗證卡住時按〔取消〕，馬上回到可以再比較（latest-wins：不等它停下），晚到的結果丟掉。
    void cancelStopsTheComparisonAtOnce() {
        Rig rig;
        rig.open();
        QVERIFY(rig.teachSample());
        rig.window.compareAction()->trigger();
        QSignalSpy finished(&compare(rig), &ComparePresenter::compareFinished);
        QVERIFY(finished.count() > 0 || finished.wait(8000));  // 預設的那一次（broken）先結束
        rig.otherProbe->gateWindows = true;
        CompareWindow* w = compareWindow(rig);
        w->backendA()->setCurrentIndex(w->backendA()->findData(QStringLiteral("fake")));
        w->backendB()->setCurrentIndex(w->backendB()->findData(QStringLiteral("other")));
        w->runButton()->click();
        QVERIFY(QTest::qWaitFor([&] { return rig.otherProbe->windowEntered.load() > 0; }, 8000));
        QVERIFY(compare(rig).running());
        QVERIFY(w->busy() && w->cancelButton()->isEnabled() && !w->runButton()->isEnabled());
        finished.clear();
        w->cancelButton()->click();
        QVERIFY(!compare(rig).running());
        QCOMPARE(finished.count(), 1);
        QCOMPARE(w->statusLabel()->text(), uiText("ui.compare.cancelled"));
        QVERIFY(w->runButton()->isEnabled());
        rig.otherProbe->windowReleased = 1 << 20;  // 卡住的搜尋放行：它的結果回來時被丟掉
        QTest::qWait(200);
        QCOMPARE(finished.count(), 1);
        QVERIFY(!compare(rig).comparison().has_value());
    }

    // A 與 B 選同一個：不比較，說要選不同的。
    void theSameBackEndTwiceIsRefused() {
        Rig rig;
        rig.open();
        QVERIFY(rig.teachSample());
        rig.window.compareAction()->trigger();
        QVERIFY(runFromWindow(rig, QStringLiteral("other"), QStringLiteral("other")));
        QCOMPARE(compareWindow(rig)->statusLabel()->text(), uiText("ui.compare.sameBackend"));
    }

    // 給人看的截圖（測試的工作資料夾的 compare-window.png；test.ps1 是 <建置>/test-work）：有兩種標記、選了一個差異。斷言只檢查截得到東西。
    void aScreenshotOfTheWindow() {
        Rig rig;
        rig.open();
        QVERIFY(rig.teachSample());
        rig.window.compareAction()->trigger();
        QVERIFY(runFromWindow(rig, QStringLiteral("fake"), QStringLiteral("other")));
        CompareWindow* w = compareWindow(rig);
        w->resize(1400, 820);
        w->thresholdB()->setValue(0.95);
        w->differencesTable()->setCurrentCell(0, 0);
        QCoreApplication::processEvents();
        const QImage shot = w->grab().toImage();
        QVERIFY(!shot.isNull());
        QVERIFY(shot.save(QDir::current().filePath(QStringLiteral("compare-window.png"))));  // test.ps1：<建置>/test-work
    }

    // 語言切換：比較視窗的固定文字與結果的文字都換。
    void theWindowFollowsTheLanguage() {
        Rig rig;
        rig.open();
        QVERIFY(rig.teachSample());
        rig.window.compareAction()->trigger();
        QVERIFY(runFromWindow(rig, QStringLiteral("fake"), QStringLiteral("other")));
        Translations::install(QStringLiteral("en"));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        CompareWindow* w = compareWindow(rig);
        QCOMPARE(w->runButton()->text(), QStringLiteral("Compare"));
        QVERIFY2(w->legendCheck(CompareKind::Both)->text().startsWith(QStringLiteral("● Found by both")), qPrintable(w->legendCheck(CompareKind::Both)->text()));
        QVERIFY(rowOf(w->summaryTable(), QStringLiteral("Found")) >= 0);
        QCOMPARE(rig.window.compareAction()->text(), QStringLiteral("Compare back ends (A/B)…"));
        Translations::install(QStringLiteral("zh_TW"));
    }
};

QTEST_MAIN(TstCompare)
#include "tst_compare.moc"
