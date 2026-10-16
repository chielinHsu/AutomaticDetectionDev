// StepGuide：操作步驟的規則（現在做到哪一步、哪些做完了）從 InspectionSession 推導；沒有 Qt 的畫面，每一條決定性地驗證。
#include <QtTest>

#include "CoreTestSupport.h"
#include "ProposalTestSupport.h"
#include "QtTestPrinters.h"
#include "application/interaction/StepGuide.h"

using namespace aoi;
using namespace aoi::test;
using G = GuideStep;

// QCOMPARE 用 ADL 找 toString()：失敗訊息印步驟的名稱，不是數字（和 QtTestPrinters.h 的狀態一樣的做法）。
namespace aoi {
inline char* toString(GuideStep s) { return QTest::toString(std::string(guideStepName(s)).c_str()); }
}  // namespace aoi

namespace {

const Rect kBig{20, 20, 60, 32};

struct Fixture {
    InspectionSession session;
    Fixture() { session.imageOpened("board.png", 1000, 800, 0.0); }
    int addSample(Rect region = kBig) {
        if (!session.beginRegion(RegionMode::Sample)) return 0;
        if (session.endRegion(region, RegionMode::Sample) != InspectionSession::Release::SampleAdded) return 0;
        return session.selection().empty() ? 0 : session.selection().front();
    }
    void toPreview() {
        if (!session.hasSamples()) QVERIFY(addSample() > 0);
        QVERIFY(session.startSearch());
        QVERIFY(session.searchSucceeded(makeProposal({0.95, 0.75, 0.50})));
        QCOMPARE_STATE(session.state(), InteractionState::Preview);
    }
    void toAccepted() {
        toPreview();
        QVERIFY(session.accept().value_or(0) > 0);
        QCOMPARE_STATE(session.state(), InteractionState::Idle);
    }
};

std::array<bool, kGuideStepCount> doneUpTo(int count) {
    std::array<bool, kGuideStepCount> d{};
    for (int i = 0; i < count; ++i) d[static_cast<std::size_t>(i)] = true;
    return d;
}

}  // namespace

class TstStepGuide : public QObject {
    Q_OBJECT

private slots:
    void aNewSessionStartsAtOpenImageWithNothingDone() {
        InspectionSession s;
        const GuideStatus g = guideStatus(s);
        QCOMPARE(g.current, G::OpenImage);
        QCOMPARE(g.done, doneUpTo(0));
        QVERIFY(!g.busy);
        QVERIFY(!g.attention);
    }

    void openingAnImageMovesToDrawSample() {
        Fixture f;
        const GuideStatus g = guideStatus(f.session);
        QCOMPARE(g.current, G::DrawSample);
        QCOMPARE(g.done, doneUpTo(1));
    }

    void draggingASampleIsDrawSampleInProgress() {
        Fixture f;
        QVERIFY(f.session.beginRegion(RegionMode::Sample));
        const GuideStatus g = guideStatus(f.session);
        QCOMPARE(g.current, G::DrawSample);
        QVERIFY(g.busy);
        QVERIFY(f.session.abortRegion());
        QVERIFY(!guideStatus(f.session).busy);
    }

    void aSampleOnTheBoardMovesToSearch() {
        Fixture f;
        QVERIFY(f.addSample() > 0);
        const GuideStatus g = guideStatus(f.session);
        QCOMPARE(g.current, G::Search);
        QCOMPARE(g.done, doneUpTo(2));
        QVERIFY(!g.busy);
    }

    void searchingIsSearchInProgress() {
        Fixture f;
        QVERIFY(f.addSample() > 0);
        QVERIFY(f.session.startSearch());
        const GuideStatus g = guideStatus(f.session);
        QCOMPARE(g.current, G::Search);
        QVERIFY(g.busy);
        QCOMPARE(g.done, doneUpTo(2));
    }

    void aPreviewMovesToAcceptWithSearchDone() {
        Fixture f;
        f.toPreview();
        const GuideStatus g = guideStatus(f.session);
        QCOMPARE(g.current, G::Accept);
        QCOMPARE(g.done, doneUpTo(3));
    }

    void changingTheThresholdInPreviewDoesNotChangeTheStatus() {
        Fixture f;
        f.toPreview();
        const GuideStatus before = guideStatus(f.session);
        f.session.setThreshold(0.40);
        QVERIFY(guideStatus(f.session) == before);
        f.session.setThreshold(0.90);
        QVERIFY(guideStatus(f.session) == before);
    }

    void acceptingMovesToRefineWithFourStepsDone() {
        Fixture f;
        f.toAccepted();
        const GuideStatus g = guideStatus(f.session);
        QCOMPARE(g.current, G::Refine);
        QCOMPARE(g.done, doneUpTo(4));
        QVERIFY(!g.attention);
    }

    void aNewSampleAfterAcceptingGoesBackToSearch() {
        Fixture f;
        f.toAccepted();
        QVERIFY(f.addSample(Rect{300, 300, 60, 32}) > 0);
        const GuideStatus g = guideStatus(f.session);
        QCOMPARE(g.current, G::Search);
        QCOMPARE(g.done, doneUpTo(4));  // 做過的不會變成沒做
    }

    void discardingGoesBackToSearch() {
        Fixture f;
        f.toPreview();
        QVERIFY(f.session.discard());
        const GuideStatus g = guideStatus(f.session);
        QCOMPARE(g.current, G::Search);
        QCOMPARE(g.done, doneUpTo(2));
    }

    void cancellingASearchGoesBackToSearch() {
        Fixture f;
        QVERIFY(f.addSample() > 0);
        QVERIFY(f.session.startSearch());
        QVERIFY(f.session.cancelSearch());
        QCOMPARE(guideStatus(f.session).current, G::Search);
        QVERIFY(!guideStatus(f.session).busy);
    }

    void noMatchAsksForAttentionOnSearch() {
        Fixture f;
        QVERIFY(f.addSample() > 0);
        QVERIFY(f.session.startSearch());
        QVERIFY(f.session.searchSucceeded(makeProblemProposal(NoMatchReason::SampleTooSmall)));
        QCOMPARE_STATE(f.session.state(), InteractionState::NoMatch);
        const GuideStatus g = guideStatus(f.session);
        QCOMPARE(g.current, G::Search);
        QVERIFY(g.attention);
        QVERIFY(!g.busy);
    }

    void aFailedSearchAsksForAttentionOnSearch() {
        Fixture f;
        QVERIFY(f.addSample() > 0);
        QVERIFY(f.session.startSearch());
        QVERIFY(f.session.searchFailed(makeError(ErrorCode::BackendFailed, "x")));
        const GuideStatus g = guideStatus(f.session);
        QCOMPARE(g.current, G::Search);
        QVERIFY(g.attention);
    }

    void aFailedImageAsksForAttentionOnOpenImage() {
        InspectionSession s;
        QVERIFY(s.imageFailed(makeError(ErrorCode::NotFound, "missing")));
        const GuideStatus g = guideStatus(s);
        QCOMPARE(g.current, G::OpenImage);
        QVERIFY(g.attention);
        QCOMPARE(g.done, doneUpTo(0));
    }

    void undoingTheAcceptGoesBackToAccept() {
        Fixture f;
        f.toAccepted();
        QVERIFY(f.session.undo());
        QCOMPARE_STATE(f.session.state(), InteractionState::Preview);
        QCOMPARE(guideStatus(f.session).current, G::Accept);
    }

    void openingAnotherImageStartsOver() {
        Fixture f;
        f.toAccepted();
        f.session.imageOpened("other.png", 800, 600, 0.0);
        const GuideStatus g = guideStatus(f.session);
        QCOMPARE(g.current, G::DrawSample);
        QCOMPARE(g.done, doneUpTo(1));
    }
};

QTEST_APPLESS_MAIN(TstStepGuide)
#include "tst_step_guide.moc"
