// MatchAdvice（R10、ADR-042）：「哪些樣本該提醒用遮罩當比對區」的規則從 InspectionSession 與 back end 的特性推導；
// 沒有 Qt 的畫面，每一條決定性地驗證。
#include <QtTest>

#include "CoreTestSupport.h"
#include "ProposalTestSupport.h"
#include "QtTestPrinters.h"
#include "application/interaction/MatchAdvice.h"

using namespace aoi;
using namespace aoi::test;

namespace {

const Rect kBig{20, 20, 60, 32};
const Rect kSecond{200, 20, 60, 32};

BackendTraits masking() {
    BackendTraits t;
    t.suggestsMatchMask = true;
    return t;
}

struct Fixture {
    InspectionSession session;
    MatchAdvice advice;
    Fixture() { session.imageOpened("board.png", 1000, 800, 0.0); }
    int addSample(Rect region = kBig) {
        if (!session.beginRegion(RegionMode::Sample)) return 0;
        if (session.endRegion(region, RegionMode::Sample) != InspectionSession::Release::SampleAdded) return 0;
        return session.selection().empty() ? 0 : session.selection().front();
    }
    std::vector<std::string> pendingTypes(const BackendTraits& traits) {
        std::vector<std::string> out;
        for (const SampleOnBoard& s : advice.pending(session, traits)) out.push_back(s.typeId);
        return out;
    }
};

}  // namespace

class TstMatchAdvice : public QObject {
    Q_OBJECT

private slots:
    void aBackendWithoutTheTraitNeverAdvises() {
        Fixture f;
        QVERIFY(f.addSample() > 0);
        QVERIFY(f.advice.pending(f.session, BackendTraits{}).empty());  // OpenCV、假 back end
    }

    void withoutASampleThereIsNothingToAdvise() {
        Fixture f;
        QVERIFY(f.advice.pending(f.session, masking()).empty());
        InspectionSession empty;  // 沒有影像
        QVERIFY(f.advice.pending(empty, masking()).empty());
    }

    void aSampleWithTheDefaultMatchSettingsIsAdvisedInIdle() {
        Fixture f;
        const int id = f.addSample();
        QVERIFY(id > 0);
        QCOMPARE_STATE(f.session.state(), InteractionState::Idle);
        const std::vector<SampleOnBoard> pending = f.advice.pending(f.session, masking());
        QCOMPARE(pending.size(), std::size_t{1});
        QCOMPARE(pending.front().componentId, id);
        QVERIFY(pending.front().match.isDefault());
    }

    void onlyIdleAdvises() {
        Fixture f;
        QVERIFY(f.addSample() > 0);
        QVERIFY(f.session.startSearch());
        QCOMPARE_STATE(f.session.state(), InteractionState::Searching);
        QVERIFY(f.advice.pending(f.session, masking()).empty());
        QVERIFY(f.session.searchSucceeded(makeProposal({0.95, 0.75, 0.50})));
        QCOMPARE_STATE(f.session.state(), InteractionState::Preview);
        QVERIFY(f.advice.pending(f.session, masking()).empty());  // 預覽由預覽的卡片說話
        QVERIFY(f.session.discard());
        QCOMPARE_STATE(f.session.state(), InteractionState::Idle);
        QCOMPARE(f.pendingTypes(masking()).size(), std::size_t{1});  // 回到 Idle、樣本還是整框 → 再提醒
    }

    void acceptingKeepsAdvisingUntilTheRegionIsSet() {
        Fixture f;
        QVERIFY(f.addSample() > 0);
        QVERIFY(f.session.startSearch());
        QVERIFY(f.session.searchSucceeded(makeProposal({0.95, 0.75, 0.50})));
        QVERIFY(f.session.accept().value_or(0) > 0);
        QCOMPARE_STATE(f.session.state(), InteractionState::Idle);
        QCOMPARE(f.pendingTypes(masking()).size(), std::size_t{1});
    }

    void aSampleWithAMatchRegionIsNotAdvised() {
        Fixture f;
        const int id = f.addSample();
        QVERIFY(id > 0);
        QVERIFY(f.session.setMatchRect(id, Rect{30, 25, 30, 20}));
        QVERIFY(f.advice.pending(f.session, masking()).empty());
        // 清掉比對區：又是整框 → 再提醒（使用者沒有按〔關閉〕）
        QVERIFY(f.session.clearMatchRegion(id));
        QCOMPARE(f.pendingTypes(masking()).size(), std::size_t{1});
    }

    void changingOnlyTheFeatureCountsAsHandled() {
        Fixture f;
        const int id = f.addSample();
        QVERIFY(id > 0);
        QVERIFY(f.session.setMatchMode(id, MatchMode::Edges));
        QVERIFY(f.advice.pending(f.session, masking()).empty());  // 使用者動過比對區的設定
    }

    void dismissingASampleSilencesItsTypeButNotANewSample() {
        Fixture f;
        QVERIFY(f.addSample() > 0);
        const std::vector<SampleOnBoard> first = f.advice.pending(f.session, masking());
        QCOMPARE(first.size(), std::size_t{1});
        f.advice.dismiss(first);
        QVERIFY(f.advice.dismissed(first.front().typeId));
        QVERIFY(f.advice.pending(f.session, masking()).empty());
        // 再框一個樣本：新的類型，只提醒它
        QVERIFY(f.addSample(kSecond) > 0);
        const std::vector<std::string> types = f.pendingTypes(masking());
        QCOMPARE(types.size(), std::size_t{1});
        QVERIFY(types.front() != first.front().typeId);
        // 關掉的記錄跟著板子：換影像之後重來
        f.advice.reset();
        QCOMPARE(f.pendingTypes(masking()).size(), std::size_t{2});
    }

    void samplesAreListedInBoardOrder() {
        Fixture f;
        const int a = f.addSample(kBig);
        const int b = f.addSample(kSecond);
        QVERIFY(a > 0 && b > a);
        const std::vector<SampleOnBoard> pending = f.advice.pending(f.session, masking());
        QCOMPARE(pending.size(), std::size_t{2});
        QCOMPARE(pending[0].componentId, a);
        QCOMPARE(pending[1].componentId, b);
    }
};

QTEST_GUILESS_MAIN(TstMatchAdvice)
#include "tst_match_advice.moc"
