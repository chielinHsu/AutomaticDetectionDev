// 互動狀態機：docs/ux/ux-spec.md 第 6 節的轉移表，逐格驗證。
//
// 下面的表是照 ux-spec 第 6 節逐行抄的獨立副本（不是從 InteractionStateMachine.cpp 產生），
// 所以實作和規格不一致時會被抓到。兩個測試：
//   1. 表裡每一個轉移各是一個測試資料列，轉移後的狀態必須相符；
//   2. 8 個狀態 × 19 個事件的其餘組合，一律不合法：fire 回 false、狀態不變。
#include <QtTest>
#include <set>
#include <string>
#include <tuple>

#include "QtTestPrinters.h"
#include "application/interaction/InteractionStateMachine.h"

using namespace aoi;
using S = InteractionState;
using E = InteractionEvent;

namespace {

struct Row {
    const char* name;
    S from;
    E event;
    S to;
    // 轉移條件
    bool hasImage = true;
    bool foundAny = false;
    // 只有 DrawingRegion 的 ReleaseTooSmall／Esc 用：進入 DrawingRegion 之前的狀態
    S before = S::Idle;
};

// ux-spec 第 6 節。
const std::vector<Row>& table() {
    static const std::vector<Row> rows = {
        // Empty
        {"Empty ImageOpened", S::Empty, E::ImageOpened, S::Idle},
        {"Empty ImageFailed", S::Empty, E::ImageFailed, S::Error},
        // Idle
        {"Idle PressRegion", S::Idle, E::PressRegion, S::DrawingRegion},
        {"Idle SearchRequested", S::Idle, E::SearchRequested, S::Searching},
        {"Idle EditType", S::Idle, E::EditType, S::EditingType},
        {"Idle UndoToPreview", S::Idle, E::UndoToPreview, S::Preview},
        {"Idle ImageOpened", S::Idle, E::ImageOpened, S::Idle},
        {"Idle ImageFailed", S::Idle, E::ImageFailed, S::Error},
        // DrawingRegion
        {"DrawingRegion Release", S::DrawingRegion, E::Release, S::Searching},
        {"DrawingRegion ReleaseSample (from Idle)", S::DrawingRegion, E::ReleaseSample, S::Idle, true, false, S::Idle},
        {"DrawingRegion ReleaseSample (from Preview)", S::DrawingRegion, E::ReleaseSample, S::Idle, true, false, S::Preview},
        {"DrawingRegion ReleaseSample (from NoMatch)", S::DrawingRegion, E::ReleaseSample, S::Idle, true, false, S::NoMatch},
        {"DrawingRegion ReleaseTooSmall (from Idle)", S::DrawingRegion, E::ReleaseTooSmall, S::Idle, true, false, S::Idle},
        {"DrawingRegion ReleaseTooSmall (from Preview)", S::DrawingRegion, E::ReleaseTooSmall, S::Preview, true, false, S::Preview},
        {"DrawingRegion ReleaseTooSmall (from NoMatch)", S::DrawingRegion, E::ReleaseTooSmall, S::NoMatch, true, false, S::NoMatch},
        {"DrawingRegion Esc (from Idle)", S::DrawingRegion, E::Esc, S::Idle, true, false, S::Idle},
        {"DrawingRegion Esc (from Preview)", S::DrawingRegion, E::Esc, S::Preview, true, false, S::Preview},
        {"DrawingRegion Esc (from NoMatch)", S::DrawingRegion, E::Esc, S::NoMatch, true, false, S::NoMatch},
        // Searching
        {"Searching ResultFound", S::Searching, E::ResultFound, S::Preview},
        {"Searching ResultEmpty", S::Searching, E::ResultEmpty, S::NoMatch},
        {"Searching Failure", S::Searching, E::Failure, S::Error},
        {"Searching Cancel", S::Searching, E::Cancel, S::Idle},
        {"Searching ParamsChanged", S::Searching, E::ParamsChanged, S::Searching},
        {"Searching SearchRequested", S::Searching, E::SearchRequested, S::Searching},
        {"Searching ImageOpened", S::Searching, E::ImageOpened, S::Idle},
        // Preview
        {"Preview ThresholdChanged", S::Preview, E::ThresholdChanged, S::Preview},
        {"Preview SearchRequested", S::Preview, E::SearchRequested, S::Searching},
        {"Preview ParamsChanged", S::Preview, E::ParamsChanged, S::Searching},
        {"Preview Accept", S::Preview, E::Accept, S::Idle},
        {"Preview Discard", S::Preview, E::Discard, S::Idle},
        {"Preview PressRegion", S::Preview, E::PressRegion, S::DrawingRegion},
        {"Preview ImageOpened", S::Preview, E::ImageOpened, S::Idle},
        {"Preview ImageFailed", S::Preview, E::ImageFailed, S::Error},
        // NoMatch
        {"NoMatch ThresholdChanged (found one)", S::NoMatch, E::ThresholdChanged, S::Preview, true, true},
        {"NoMatch ThresholdChanged (found none)", S::NoMatch, E::ThresholdChanged, S::NoMatch, true, false},
        {"NoMatch SearchRequested", S::NoMatch, E::SearchRequested, S::Searching},
        {"NoMatch ParamsChanged", S::NoMatch, E::ParamsChanged, S::Searching},
        {"NoMatch Dismiss", S::NoMatch, E::Dismiss, S::Idle},
        {"NoMatch PressRegion", S::NoMatch, E::PressRegion, S::DrawingRegion},
        {"NoMatch ImageOpened", S::NoMatch, E::ImageOpened, S::Idle},
        {"NoMatch ImageFailed", S::NoMatch, E::ImageFailed, S::Error},
        // Error
        {"Error Dismiss (has an image)", S::Error, E::Dismiss, S::Idle, true},
        {"Error Dismiss (no image)", S::Error, E::Dismiss, S::Empty, false},
        {"Error Retry", S::Error, E::Retry, S::Searching},
        {"Error ImageOpened", S::Error, E::ImageOpened, S::Idle},
        // EditingType
        {"EditingType Save", S::EditingType, E::Save, S::Idle},
        {"EditingType Cancel", S::EditingType, E::Cancel, S::Idle},
    };
    return rows;
}

// 讓機器處於 row.from（DrawingRegion 要經過 PressRegion 才記得來源）。
InteractionStateMachine machineAt(const Row& row) {
    InteractionStateMachine m;
    if (row.from == S::DrawingRegion) {
        m.reset(row.before);
        const bool pressed = m.fire(E::PressRegion);
        Q_ASSERT(pressed);
        Q_UNUSED(pressed);
    } else {
        m.reset(row.from);
    }
    return m;
}

TransitionContext ctxOf(const Row& row) {
    TransitionContext ctx;
    ctx.hasImage = row.hasImage;
    ctx.foundAny = row.foundAny;
    return ctx;
}

}  // namespace

Q_DECLARE_METATYPE(InteractionState)
Q_DECLARE_METATYPE(InteractionEvent)

class TstStateMachine : public QObject {
    Q_OBJECT

private slots:
    void aNewMachineStartsEmpty() {
        InteractionStateMachine m;
        QCOMPARE_STATE(m.state(), S::Empty);
    }

    void theTableHasEveryRowOfTheSpec() {
        // 計數：ux-spec §6 的 8 個狀態共 2+6+4+7+8+7+3+2 ＝ 39 個事件格（2026-10-14 加了 Idle／Preview／NoMatch 的
        // SearchRequested 與 DrawingRegion 的 ReleaseSample，ADR-025）；DrawingRegion 的三格各依來源展開成 3 列，
        // NoMatch 的 ThresholdChanged 與 Error 的 Dismiss 各依條件展開成 2 列。
        QCOMPARE(static_cast<int>(table().size()), 47);
    }

    void everyTransitionInTheSpec_data() {
        QTest::addColumn<int>("row");
        for (std::size_t i = 0; i < table().size(); ++i) QTest::newRow(table()[i].name) << static_cast<int>(i);
    }
    void everyTransitionInTheSpec() {
        QFETCH(int, row);
        const Row& r = table()[static_cast<std::size_t>(row)];
        InteractionStateMachine m = machineAt(r);
        QCOMPARE_STATE(m.state(), r.from);

        const std::optional<InteractionState> peeked = m.peek(r.event, ctxOf(r));
        QVERIFY2(peeked.has_value(), "peek must accept a transition that is in the spec");
        QCOMPARE_STATE(*peeked, r.to);
        QCOMPARE_STATE(m.state(), r.from);  // peek 不動

        QVERIFY(m.fire(r.event, ctxOf(r)));
        QCOMPARE_STATE(m.state(), r.to);
    }

    void everyOtherCombinationIsRefusedAndLeavesTheStateAlone() {
        int checked = 0;
        for (int s = 0; s < kInteractionStateCount; ++s) {
            for (int e = 0; e < kInteractionEventCount; ++e) {
                for (bool hasImage : {true, false}) {
                    for (bool foundAny : {true, false}) {
                        // 這四個條件組合裡，表只對「有條件的那幾格」列了需要的值；其餘格子與條件無關，
                        // 所以只要 (狀態, 事件) 出現在表裡，不論條件都是合法的。
                        bool inSpec = false;
                        for (const Row& r : table()) {
                            inSpec = inSpec || (static_cast<int>(r.from) == s && static_cast<int>(r.event) == e);
                        }
                        if (inSpec) continue;
                        InteractionStateMachine m;
                        m.reset(static_cast<S>(s));
                        TransitionContext ctx;
                        ctx.hasImage = hasImage;
                        ctx.foundAny = foundAny;
                        const std::string what = std::string(toString(static_cast<S>(s))) + " + " + std::string(toString(static_cast<E>(e)));
                        QVERIFY2(!m.peek(static_cast<E>(e), ctx).has_value(), what.c_str());
                        QVERIFY2(!m.fire(static_cast<E>(e), ctx), what.c_str());
                        QVERIFY2(m.state() == static_cast<S>(s), what.c_str());
                        ++checked;
                    }
                }
            }
        }
        // 8 × 21 ＝ 168 組 (狀態, 事件)，其中 39 組在表裡；其餘 129 組各 4 種條件。
        QCOMPARE(checked, 129 * 4);
    }

    void aRefusedEventKeepsTheDrawingOrigin() {
        InteractionStateMachine m;
        m.reset(S::Preview);
        QVERIFY(m.fire(E::PressRegion));
        QCOMPARE_STATE(m.stateBeforeDrawing(), S::Preview);
        QVERIFY(!m.fire(E::Accept));   // 畫到一半不能接受
        QCOMPARE_STATE(m.state(), S::DrawingRegion);
        QCOMPARE_STATE(m.stateBeforeDrawing(), S::Preview);
        QVERIFY(m.fire(E::Esc));
        QCOMPARE_STATE(m.state(), S::Preview);
    }

    void theDrawingOriginIsRememberedPerGesture() {
        InteractionStateMachine m;
        m.reset(S::Idle);
        QVERIFY(m.fire(E::PressRegion));
        QVERIFY(m.fire(E::Release));      // → Searching
        QVERIFY(m.fire(E::ResultEmpty));  // → NoMatch
        QVERIFY(m.fire(E::PressRegion));
        QCOMPARE_STATE(m.stateBeforeDrawing(), S::NoMatch);  // 這一次從 NoMatch 開始，不是上一次的 Idle
        QVERIFY(m.fire(E::ReleaseTooSmall));
        QCOMPARE_STATE(m.state(), S::NoMatch);
    }

    void aSearchRoundTripCoversTheWholeFlow() {
        InteractionStateMachine m;
        QVERIFY(m.fire(E::ImageOpened));
        QVERIFY(m.fire(E::PressRegion));
        QVERIFY(m.fire(E::Release));
        QCOMPARE_STATE(m.state(), S::Searching);
        QVERIFY(m.fire(E::ResultFound));
        QCOMPARE_STATE(m.state(), S::Preview);
        QVERIFY(m.fire(E::Accept));
        QCOMPARE_STATE(m.state(), S::Idle);
        QVERIFY(m.fire(E::UndoToPreview));
        QCOMPARE_STATE(m.state(), S::Preview);
        QVERIFY(m.fire(E::Discard));
        QCOMPARE_STATE(m.state(), S::Idle);
    }

    void namesAreDistinctAndNeverUnknown() {
        std::set<std::string> states;
        for (int s = 0; s < kInteractionStateCount; ++s) {
            const std::string name(toString(static_cast<S>(s)));
            QVERIFY(name != "Unknown");
            states.insert(name);
        }
        QCOMPARE(static_cast<int>(states.size()), kInteractionStateCount);
        std::set<std::string> events;
        for (int e = 0; e < kInteractionEventCount; ++e) {
            const std::string name(toString(static_cast<E>(e)));
            QVERIFY(name != "Unknown");
            events.insert(name);
        }
        QCOMPARE(static_cast<int>(events.size()), kInteractionEventCount);
    }
};

QTEST_APPLESS_MAIN(TstStateMachine)
#include "tst_state_machine.moc"
