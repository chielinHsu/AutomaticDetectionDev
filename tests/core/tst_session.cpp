// InspectionSession：狀態機＋文件＋提案＋選取的規則（沒有 Qt 的畫面，所以每一條都能決定性地驗證）。
#include <QtTest>

#include "ProposalTestSupport.h"
#include "CoreTestSupport.h"
#include "QtTestPrinters.h"
#include "application/interaction/InspectionSession.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "domain/box_expander.h"

using namespace aoi;
using namespace aoi::test;
using S = InteractionState;

namespace {

const Rect kBig{20, 20, 60, 32};

// 一個走到 Idle 的工作階段。
struct Fixture {
    InspectionSession session;
    Fixture() { session.imageOpened("board.png", 1000, 800, 0.0); }

    // 建立一個樣本（S 工具放開，ADR-025）：回到 Idle，板上多一個樣本元件（第一個的類型是 t1，和 makeProposal 的相同）。
    // 回傳樣本元件的編號；失敗回 0。
    int addSample(Rect region = kBig) {
        if (!session.beginRegion(RegionMode::Sample)) return 0;
        if (session.endRegion(region, RegionMode::Sample) != InspectionSession::Release::SampleAdded) return 0;
        return session.selection().empty() ? 0 : session.selection().front();
    }
    // 進入 Searching。Sample ＝ 板上還沒有樣本就先建一個，再按〔搜尋〕；Region ＝ 畫一個區域放開。
    void search(RegionMode mode = RegionMode::Sample, Rect region = kBig) {
        if (mode == RegionMode::Sample) {
            if (!session.hasSamples()) QVERIFY(addSample(region) > 0);
            QVERIFY(session.startSearch());
        } else {
            QVERIFY(session.beginRegion(mode));
            QVERIFY(session.endRegion(region, mode) == InspectionSession::Release::Search);
        }
    }
    // 走到 Preview。
    void toPreview(const std::vector<double>& scores = {0.95, 0.75, 0.50}, std::size_t predicted = 0) {
        search();
        QVERIFY(session.searchSucceeded(makeProposal(scores, predicted)));
        QCOMPARE_STATE(session.state(), S::Preview);
    }
};

}  // namespace

class TstSession : public QObject {
    Q_OBJECT

private slots:
    // ---- 影像 ----
    void aNewSessionIsEmptyAndOpeningAnImageMakesItIdle() {
        InspectionSession s;
        QCOMPARE_STATE(s.state(), S::Empty);
        QVERIFY(!s.hasImage());
        QCOMPARE(s.threshold(), 0.70);
        s.imageOpened("a.png", 640, 480, 0.0);
        QCOMPARE_STATE(s.state(), S::Idle);
        QVERIFY(s.hasImage());
        QCOMPARE(s.document().board().imageWidth, 640);
    }

    void openingAnotherImageResetsEverythingFromAnyState() {
        for (int target = 0; target < 5; ++target) {
            Fixture f;
            if (target == 1) f.search();                                 // Searching
            if (target == 2) f.toPreview();                              // Preview
            if (target == 3) { f.search(); f.session.searchEmpty({NoMatchReason::LibraryEmpty, 0, 0, 0}); }  // NoMatch
            if (target == 4) { QVERIFY(f.session.beginRegion(RegionMode::Sample)); }  // DrawingRegion
            f.session.imageOpened("b.png", 100, 100, 0.0);
            QCOMPARE_STATE(f.session.state(), S::Idle);
            QVERIFY(f.session.proposal() == nullptr);
            QVERIFY(f.session.selection().empty());
            QVERIFY(!f.session.lastSearch().has_value());
            QVERIFY(!f.session.document().canUndo());
            QCOMPARE(f.session.document().board().name, std::string("b.png"));
        }
    }

    void anImageFailureIsAnErrorThatDismissesToIdleOrEmpty() {
        Fixture f;
        QVERIFY(f.session.imageFailed(makeError(ErrorCode::FormatInvalid, "bad file")));
        QCOMPARE_STATE(f.session.state(), S::Error);
        QVERIFY(f.session.errorSource() == ErrorSource::Image);
        QVERIFY(f.session.dismiss());
        QCOMPARE_STATE(f.session.state(), S::Idle);  // 舊影像還在

        InspectionSession fresh;  // 一張影像都沒開過
        QVERIFY(fresh.imageFailed(makeError(ErrorCode::NotFound, "missing")));
        QVERIFY(fresh.dismiss());
        QCOMPARE_STATE(fresh.state(), S::Empty);
    }

    void anImageFailureWhileSearchingIsRefusedAndTheSearchContinues() {
        Fixture f;
        f.search();
        QVERIFY(!f.session.imageFailed(makeError(ErrorCode::FormatInvalid, "bad file")));
        QCOMPARE_STATE(f.session.state(), S::Searching);
    }

    // ---- 區域 ----
    // ADR-025：S 放開只建立樣本（一個類型＋一個樣本元件的 Command），回到 Idle；搜尋另外按。
    void releasingASampleCreatesItAndGoesBackToIdle() {
        Fixture f;
        QVERIFY(f.session.beginRegion(RegionMode::Sample));
        QCOMPARE_STATE(f.session.state(), S::DrawingRegion);
        QVERIFY(f.session.endRegion(kBig, RegionMode::Sample) == InspectionSession::Release::SampleAdded);
        QCOMPARE_STATE(f.session.state(), S::Idle);
        QVERIFY(!f.session.lastSearch().has_value());
        QVERIFY(f.session.hasSamples());
        const std::vector<SampleOnBoard> samples = f.session.document().samples();
        QCOMPARE(samples.size(), std::size_t{1});
        QCOMPARE(samples[0].typeId, std::string("t1"));
        QCOMPARE(samples[0].rect.w, 60.0);
        QCOMPARE(samples[0].rect.x, 20.0);
        const ComponentInstance* c = f.session.document().find(samples[0].componentId);
        QVERIFY(c != nullptr);
        QVERIFY(c->state == ObjectState::Sample);
        QVERIFY(c->origin == ObjectOrigin::Manual);
        QVERIFY(!c->score.has_value());
        QCOMPARE(f.session.selection(), std::vector<int>{c->id});  // 建完就是選取的：接著可以改大小、畫子框
        QVERIFY(f.session.undoInfo().has_value());
        QVERIFY(f.session.undoInfo()->kind == CommandKind::AddSample);
        QVERIFY(f.session.undo());
        QVERIFY(!f.session.hasSamples());
        QVERIFY(f.session.document().board().types.empty());
    }

    void releasingARegionStartsAScanAndRemembersIt() {
        Fixture f;
        QVERIFY(f.session.beginRegion(RegionMode::Region));
        QVERIFY(f.session.endRegion(kBig, RegionMode::Region) == InspectionSession::Release::Search);
        QCOMPARE_STATE(f.session.state(), S::Searching);
        QVERIFY(f.session.lastSearch().has_value());
        QCOMPARE(f.session.lastSearch()->region.w, 60.0);
        QVERIFY(f.session.lastSearch()->mode == RegionMode::Region);
        QVERIFY(f.session.lastSearch()->samples.empty());
    }

    void searchNeedsASampleOnTheBoard() {
        Fixture f;
        QVERIFY(!f.session.startSearch());
        QCOMPARE_STATE(f.session.state(), S::Idle);
        QVERIFY(f.addSample() > 0);
        QVERIFY(f.session.startSearch());
        QCOMPARE_STATE(f.session.state(), S::Searching);
        QVERIFY(f.session.selection().empty());
        QVERIFY(f.session.startSearch());  // 搜尋中再按：重新搜尋，最新的優先
    }

    void searchUsesEverySampleOnTheBoardAndRemembersTheFirstAsTheRegion() {
        Fixture f;
        QVERIFY(f.addSample(Rect{20, 20, 60, 32}) > 0);
        QVERIFY(f.addSample(Rect{300, 300, 40, 20}) > 0);
        QVERIFY(f.session.startSearch());
        const SearchTarget& t = *f.session.lastSearch();
        QVERIFY(t.mode == RegionMode::Sample);
        QCOMPARE(t.samples.size(), std::size_t{2});
        QCOMPARE(t.samples[0].typeId, std::string("t1"));
        QCOMPARE(t.samples[1].typeId, std::string("t2"));
        QCOMPARE(t.region.w, 60.0);
    }

    void aMovedSampleIsSearchedAtItsNewPlace() {
        Fixture f;
        const int id = f.addSample();
        f.session.select({id});
        QCOMPARE(f.session.moveSelection({10, 5}), std::size_t{1});
        QVERIFY(f.session.startSearch());
        QCOMPARE(f.session.lastSearch()->samples[0].rect.x, 30.0);
        QCOMPARE(f.session.lastSearch()->samples[0].rect.y, 25.0);
    }

    void aDeletedSampleIsNotSearchedButItsTypeStays() {
        Fixture f;
        const int id = f.addSample();
        f.session.select({id});
        QCOMPARE(f.session.deleteSelection(), std::size_t{1});
        QVERIFY(!f.session.hasSamples());
        QVERIFY(!f.session.startSearch());
        QCOMPARE(f.session.document().board().types.size(), std::size_t{1});
    }

    void aTooSmallRegionGoesBackToTheStateItStartedFrom() {
        Fixture f;
        QVERIFY(f.session.beginRegion(RegionMode::Sample));
        QVERIFY(f.session.endRegion(Rect{20, 20, 3, 2}, RegionMode::Sample) == InspectionSession::Release::TooSmall);
        QCOMPARE_STATE(f.session.state(), S::Idle);
        QVERIFY(!f.session.lastSearch().has_value());
    }

    void theMinimumIsTheLongestSideThatStillCountsAsTooSmall() {
        Fixture f;
        QVERIFY(f.session.beginRegion(RegionMode::Sample));
        // 8 × 8 剛好夠；7.9 × 50 不夠（兩邊都要 ≥ 8）。
        QVERIFY(f.session.endRegion(Rect{0, 0, 7.9, 50}, RegionMode::Sample) == InspectionSession::Release::TooSmall);
        QVERIFY(f.session.beginRegion(RegionMode::Sample));
        QVERIFY(f.session.endRegion(Rect{0, 0, 8, 8}, RegionMode::Sample) == InspectionSession::Release::SampleAdded);
    }

    void escWhileDrawingFromPreviewKeepsTheProposal() {
        Fixture f;
        f.toPreview();
        QVERIFY(f.session.beginRegion(RegionMode::Sample));
        QCOMPARE_STATE(f.session.state(), S::DrawingRegion);
        QVERIFY(f.session.proposal() != nullptr);  // 還沒放開：舊提案還在
        QVERIFY(f.session.abortRegion());
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QVERIFY(f.session.proposal() != nullptr);
    }

    void aTooSmallReleaseFromNoMatchGoesBackToNoMatch() {
        Fixture f;
        f.search();
        f.session.searchEmpty({NoMatchReason::LibraryEmpty, 0, 0, 0});
        QVERIFY(f.session.beginRegion(RegionMode::Region));
        f.session.endRegion(Rect{0, 0, 2, 2}, RegionMode::Region);
        QCOMPARE_STATE(f.session.state(), S::NoMatch);
        QVERIFY(f.session.noMatch().has_value());
    }

    void creatingASampleInPreviewDropsTheProposalAndGoesIdle() {
        Fixture f;
        f.toPreview();
        QVERIFY(f.session.beginRegion(RegionMode::Sample));
        QVERIFY(f.session.endRegion(Rect{300, 300, 40, 20}, RegionMode::Sample) == InspectionSession::Release::SampleAdded);
        QCOMPARE_STATE(f.session.state(), S::Idle);
        QVERIFY(f.session.proposal() == nullptr);  // 舊提案是用舊的樣本集合搜尋的，不再有意義
        QCOMPARE(f.session.document().samples().size(), std::size_t{2});
        QVERIFY(!f.session.lastSearch().has_value());
    }

    void drawingIsRefusedWhileSearchingOrInAnError() {
        Fixture f;
        f.search();
        QVERIFY(!f.session.beginRegion(RegionMode::Sample));
        QVERIFY(f.session.searchFailed(makeError(ErrorCode::BackendFailed, "x")));
        QVERIFY(!f.session.beginRegion(RegionMode::Sample));
    }

    // ---- 搜尋結果 ----
    void aResultWithFoundObjectsGoesToPreview() {
        Fixture f;
        f.toPreview({0.95, 0.75});
        QVERIFY(f.session.proposal() != nullptr);
        QVERIFY(!f.session.noMatch().has_value());
        QCOMPARE(f.session.proposal()->objects().found, std::size_t{2});
    }

    void aSampleProblemGoesToNoMatchWithTheReason() {
        Fixture f;
        f.search();
        QVERIFY(f.session.searchSucceeded(makeProblemProposal(NoMatchReason::SampleTooSmall)));
        QCOMPARE_STATE(f.session.state(), S::NoMatch);
        QVERIFY(f.session.noMatch()->reason == NoMatchReason::SampleTooSmall);
    }

    void aThresholdAboveEveryScoreGoesToNoMatchAndLoweringItReachesPreview() {
        Fixture f;
        f.session.setThreshold(0.90);
        f.search();
        QVERIFY(f.session.searchSucceeded(makeProposal({0.60, 0.55})));
        QCOMPARE_STATE(f.session.state(), S::NoMatch);
        QVERIFY(f.session.noMatch()->reason == NoMatchReason::ThresholdTooHigh);
        const double suggested = f.session.noMatch()->suggestedThreshold;
        f.session.setThreshold(suggested);  // 「降低門檻到 {suggested}」
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QVERIFY(!f.session.noMatch().has_value());
        QVERIFY(f.session.proposal()->objects().found >= 1);
    }

    // 轉移表：Preview 的 ThresholdChanged 一律留在 Preview（只有 NoMatch 的 ThresholdChanged 會看有沒有找到）。
    void raisingTheThresholdInPreviewStaysInPreviewEvenWithNothingFound() {
        Fixture f;
        f.toPreview({0.95, 0.75});
        f.session.setThreshold(0.99);
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QCOMPARE(f.session.proposal()->objects().found, std::size_t{0});
        QVERIFY(!f.session.noMatch().has_value());  // noMatch 只在 NoMatch 狀態有意義
        f.session.setThreshold(0.50);
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QCOMPARE(f.session.proposal()->objects().found, std::size_t{2});
    }

    void aStaleResultIsDropped() {
        Fixture f;
        f.search();
        QVERIFY(f.session.cancelSearch());
        QVERIFY(!f.session.searchSucceeded(makeProposal({0.95})));
        QVERIFY(!f.session.searchFailed(makeError(ErrorCode::BackendFailed, "late")));
        QVERIFY(!f.session.searchEmpty({NoMatchReason::LibraryEmpty, 0, 0, 0}));
        QCOMPARE_STATE(f.session.state(), S::Idle);
        QVERIFY(f.session.proposal() == nullptr);
    }

    void cancelGoesToIdleAndForgetsTheRegion() {
        Fixture f;
        f.search();
        QVERIFY(f.session.cancelSearch());
        QCOMPARE_STATE(f.session.state(), S::Idle);
        QVERIFY(!f.session.lastSearch().has_value());
        QVERIFY(!f.session.cancelSearch());  // 沒有進行中的搜尋
    }

    void aFailureIsAnErrorThatRetryRunsAgain() {
        Fixture f;
        f.search();
        QVERIFY(f.session.searchFailed(makeError(ErrorCode::BackendFailed, "boom")));
        QCOMPARE_STATE(f.session.state(), S::Error);
        QVERIFY(f.session.errorSource() == ErrorSource::Search);
        QCOMPARE(f.session.error()->message, std::string("boom"));
        QVERIFY(f.session.lastSearch().has_value());  // 區域留著，才能再試
        QVERIFY(f.session.retrySearch());
        QCOMPARE_STATE(f.session.state(), S::Searching);
        QVERIFY(!f.session.error().has_value());
    }

    void retryIsOnlyForSearchErrors() {
        Fixture f;
        f.session.imageFailed(makeError(ErrorCode::FormatInvalid, "x"));
        QVERIFY(!f.session.retrySearch());
        QCOMPARE_STATE(f.session.state(), S::Error);
    }

    void dismissingNoMatchGoesToIdleAndClearsTheRegion() {
        Fixture f;
        f.search();
        f.session.searchEmpty({NoMatchReason::LibraryEmpty, 0, 0, 0});
        QVERIFY(f.session.dismiss());
        QCOMPARE_STATE(f.session.state(), S::Idle);
        QVERIFY(!f.session.lastSearch().has_value());
        QVERIFY(!f.session.noMatch().has_value());
    }

    void restartingTheSearchKeepsTheOldProposalUntilTheNewResultArrives() {
        Fixture f;
        f.toPreview();
        QVERIFY(f.session.restartSearch());
        QCOMPARE_STATE(f.session.state(), S::Searching);
        QVERIFY(f.session.proposal() != nullptr);
        QVERIFY(f.session.searchSucceeded(makeProposal({0.99})));
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QCOMPARE(f.session.proposal()->proposal().candidates.size(), std::size_t{2});  // 換成新的
    }

    void restartingNeedsARegion() {
        Fixture f;
        QVERIFY(!f.session.restartSearch());
        QCOMPARE_STATE(f.session.state(), S::Idle);
    }

    // ---- 門檻 ----
    void thresholdIsClampedAndRemembered() {
        Fixture f;
        f.session.setThreshold(5.0);
        QCOMPARE(f.session.threshold(), 0.99);
        f.session.setThreshold(-1.0);
        QCOMPARE(f.session.threshold(), 0.30);
        f.session.setThreshold(0.85);
        f.toPreview();
        QCOMPARE(f.session.proposal()->threshold(), 0.85);  // 新提案用目前的門檻
    }

    void thresholdChangesAreNotUndoable() {
        Fixture f;
        f.toPreview();
        f.session.setThreshold(0.90);
        f.session.setThreshold(0.40);
        QVERIFY(!f.session.undoInfo().has_value());
    }

    // ---- 接受、捨棄 ----
    void acceptMovesTheProposalIntoTheDocument() {
        Fixture f;
        f.toPreview({0.95, 0.75, 0.50}, 1);  // Sample、2 個 Found、1 個 Rejected、1 個 Inferred
        const auto count = f.session.accept();
        QVERIFY(count.has_value());
        QCOMPARE(*count, std::size_t{3});  // Sample 已經在板子上，不再加（ADR-025）
        QCOMPARE_STATE(f.session.state(), S::Idle);
        QVERIFY(f.session.proposal() == nullptr);
        QCOMPARE(f.session.document().components().size(), std::size_t{4});
        QVERIFY(f.session.selection().empty());
    }

    void acceptOutsideThePreviewDoesNothing() {
        Fixture f;
        QVERIFY(!f.session.accept().has_value());
        f.search();
        QVERIFY(!f.session.accept().has_value());
        QVERIFY(!f.session.discard());
    }

    void acceptingAnEmptiedProposalStaysInPreview() {
        Fixture f;
        f.toPreview({0.95}, 0);
        f.session.select({1, 2});
        f.session.deleteSelection();
        const auto count = f.session.accept();
        QVERIFY(count.has_value());
        QCOMPARE(*count, std::size_t{0});
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QCOMPARE(f.session.document().historyDepth(), std::size_t{1});  // 只有建立樣本那一步
    }

    void discardGoesToIdleWithoutTouchingTheBoard() {
        Fixture f;
        f.toPreview();
        QVERIFY(f.session.discard());
        QCOMPARE_STATE(f.session.state(), S::Idle);
        QCOMPARE(f.session.document().components().size(), std::size_t{1});  // 只剩樣本
    }

    void undoAfterAcceptReturnsToPreviewWithTheSameProposal() {
        Fixture f;
        f.toPreview({0.95, 0.85, 0.75});  // 刪掉 2 之後還有 0.85 過門檻，接受才不會是 0 個
        f.session.setThreshold(0.80);
        f.session.select({2});
        f.session.deleteSelection();                     // 預覽中刪掉 id 2
        const auto proposal = f.session.proposal()->proposalPtr();
        f.session.accept();
        QVERIFY(f.session.undo());
        QCOMPARE_STATE(f.session.state(), S::Preview);         // UndoToPreview
        QVERIFY(f.session.proposal()->proposalPtr() == proposal);
        QCOMPARE(f.session.threshold(), 0.80);
        QVERIFY(!f.session.proposal()->contains(2));     // 刪掉的還是刪掉的
        QCOMPARE(f.session.document().components().size(), std::size_t{1});  // 只剩樣本
        QVERIFY(f.session.lastSearch().has_value());     // 區域也回來了
        ProposalSession copy = *f.session.proposal();
        QVERIFY(copy.undo());                            // 提案堆疊也回來了
        QVERIFY(copy.contains(2));
    }

    void undoAfterDiscardAlsoReturnsToPreview() {
        Fixture f;
        f.toPreview();
        f.session.discard();
        QVERIFY(f.session.undo());
        QCOMPARE_STATE(f.session.state(), S::Preview);
    }

    void redoAfterUndoToPreviewAcceptsAgain() {
        Fixture f;
        f.toPreview({0.95, 0.75});
        f.session.accept();
        f.session.undo();
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QVERIFY(f.session.redo());
        QCOMPARE_STATE(f.session.state(), S::Idle);
        QCOMPARE(f.session.document().components().size(), std::size_t{3});
        QVERIFY(f.session.proposal() == nullptr);
    }

    void redoAfterUndoOfDiscardDiscardsAgain() {
        Fixture f;
        f.toPreview();
        f.session.discard();
        f.session.undo();
        QVERIFY(f.session.redo());
        QCOMPARE_STATE(f.session.state(), S::Idle);
        QCOMPARE(f.session.document().components().size(), std::size_t{1});  // 只剩樣本
    }

    // ADR-002 規則 4：接受時先清掉提案堆疊的可重做部分，否則復原回預覽後 Ctrl+Y 會先重做殘留的舊動作。
    void acceptingClearsTheProposalRedoTailSoRedoReAccepts() {
        Fixture f;
        f.toPreview({0.95, 0.75});
        f.session.select({3});
        f.session.deleteSelection();
        QVERIFY(f.session.undo());              // 提案堆疊：刪除被復原，可重做
        QVERIFY(f.session.proposal()->canRedo());
        f.session.accept();                     // 3 個元件（Sample、0.95、0.75）
        QCOMPARE(f.session.document().components().size(), std::size_t{3});
        f.session.undo();                       // 回預覽
        QVERIFY(!f.session.proposal()->canRedo());  // 殘留的「重做刪除」已經清掉
        QVERIFY(f.session.redo());
        QCOMPARE_STATE(f.session.state(), S::Idle);   // Ctrl+Y ＝ 重新接受，不是重做刪除
        QCOMPARE(f.session.document().components().size(), std::size_t{3});
    }

    void editingTheProposalAfterUndoToPreviewMakesTheOldAcceptUnredoable() {
        Fixture f;
        f.toPreview({0.95, 0.75});
        f.session.accept();
        f.session.undo();                      // 回預覽
        QVERIFY(f.session.document().nextRedoProposal() != nullptr);  // 這時 Ctrl+Y 會重新接受
        f.session.select({2});
        f.session.deleteSelection();           // 使用者改了提案
        QVERIFY(f.session.document().nextRedoProposal() == nullptr);  // 舊的「接受」帶的是改之前的提案，不能再重做
        QVERIFY(!f.session.document().canRedo());
        QVERIFY(f.session.undo());             // Ctrl+Z 復原剛才的刪除
        QVERIFY(f.session.proposal()->contains(2));
        QVERIFY(f.session.redo());             // Ctrl+Y 重做那個刪除，而不是重新接受
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QVERIFY(!f.session.proposal()->contains(2));
        QCOMPARE(f.session.document().components().size(), std::size_t{1});  // 只剩樣本
    }

    void changingTheThresholdAfterUndoToPreviewMakesTheOldAcceptUnredoable() {
        Fixture f;
        f.toPreview({0.95, 0.75});
        f.session.accept();
        f.session.undo();
        f.session.setThreshold(0.99);
        QVERIFY(!f.session.redo());
        QCOMPARE_STATE(f.session.state(), S::Preview);
    }

    void redoNeverReAcceptsAnOlderProposalAfterANewSearch() {
        Fixture f;
        f.toPreview({0.95, 0.75});
        f.session.accept();
        f.session.undo();                           // 回預覽（提案 A）；文件堆疊有一個可重做的「接受 A」
        QVERIFY(f.session.document().nextRedoProposal() != nullptr);
        QVERIFY(f.session.startSearch());
        QVERIFY(f.session.searchSucceeded(makeProposal({0.90, 0.85})));  // 新的提案 B
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QVERIFY(!f.session.redo());                 // 這個 Ctrl+Y 不能把 A 接受進來：預覽中的是 B
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QCOMPARE(f.session.document().components().size(), std::size_t{1});  // 只剩樣本
        QVERIFY(!f.session.redoInfo().has_value());
    }

    // ---- 復原的路由（ADR-002 規則 5）----
    void inPreviewUndoOnlyTouchesTheProposalStack() {
        Fixture f;
        f.toPreview({0.95, 0.75});
        f.session.accept();
        f.session.undo();                                   // 回預覽
        const std::size_t docDepth = f.session.document().historyDepth();
        QVERIFY(!f.session.undo());                         // 提案堆疊是空的：多餘的復原忽略
        QCOMPARE(f.session.document().historyDepth(), docDepth);
        QCOMPARE_STATE(f.session.state(), S::Preview);
    }

    void anExtraUndoInPreviewNeverUndoesAnEarlierAcceptedBatch() {
        Fixture f;
        f.toPreview({0.95, 0.75});
        f.session.accept();                                   // 文件堆疊：接受 A
        const std::size_t boardSize = f.session.document().components().size();
        f.toPreview({0.90, 0.85});                            // 第二份提案 B，預覽中，提案堆疊是空的
        QVERIFY(!f.session.undo());                           // 多餘的復原：忽略
        QCOMPARE(f.session.document().components().size(), boardSize);  // 板上 A 的元件還在
        QVERIFY(f.session.document().canUndo());              // A 的「接受」還在文件堆疊上
        QCOMPARE_STATE(f.session.state(), S::Preview);
    }

    void inPreviewUndoRedoWorkOnProposalEdits() {
        Fixture f;
        f.toPreview({0.95, 0.75});
        f.session.select({2});
        f.session.deleteSelection();
        QVERIFY(f.session.undoInfo().has_value());
        QVERIFY(f.session.undoInfo()->kind == CommandKind::DeleteComponents);
        QVERIFY(f.session.undo());
        QVERIFY(f.session.proposal()->contains(2));
        QVERIFY(f.session.redoInfo().has_value());
        QVERIFY(f.session.redo());
        QVERIFY(!f.session.proposal()->contains(2));
    }

    void undoRedoAreIgnoredWhileSearchingDrawingOrInAnError() {
        Fixture f;
        f.toPreview();
        f.session.accept();                                 // 文件堆疊有東西可以復原
        f.search();                                         // Searching
        QVERIFY(!f.session.undo());
        QVERIFY(!f.session.redo());
        QVERIFY(!f.session.undoInfo().has_value());
        f.session.cancelSearch();
        QVERIFY(f.session.beginRegion(RegionMode::Sample)); // DrawingRegion
        QVERIFY(!f.session.undo());
        f.session.abortRegion();
        QVERIFY(f.session.undo());                          // 回到 Idle 就可以了
    }

    // ---- 選取與編輯 ----
    void deleteInIdleRemovesSelectedComponentsAndUndoBringsThemBack() {
        Fixture f;
        f.toPreview({0.95, 0.75});
        f.session.accept();
        f.session.select({1, 3});
        QCOMPARE(f.session.deleteSelection(), std::size_t{2});
        QCOMPARE(f.session.document().components().size(), std::size_t{1});
        QVERIFY(f.session.selection().empty());
        QVERIFY(f.session.undo());
        QCOMPARE(f.session.document().components().size(), std::size_t{3});
        QCOMPARE_STATE(f.session.state(), S::Idle);  // 復原的是刪除，不是接受
    }

    void deleteInPreviewTouchesOnlyTheProposal() {
        Fixture f;
        f.toPreview({0.95, 0.75});
        f.session.select({2});
        QCOMPARE(f.session.deleteSelection(), std::size_t{1});
        QCOMPARE(f.session.document().components().size(), std::size_t{1});  // 只剩樣本
        QCOMPARE(f.session.document().historyDepth(), std::size_t{1});  // 文件只有建立樣本那一步
    }

    void deleteDoesNothingWithoutASelectionOrInOtherStates() {
        Fixture f;
        QCOMPARE(f.session.deleteSelection(), std::size_t{0});
        f.search();
        f.session.select({1});
        QCOMPARE(f.session.deleteSelection(), std::size_t{0});
    }

    void selectionKeepsOnlyIdsThatExist() {
        Fixture f;
        f.toPreview({0.95});
        f.session.select({1, 2, 77});
        QCOMPARE(f.session.selection(), (std::vector<int>{1, 2}));
        f.session.accept();
        f.session.select({1, 2, 77});
        QCOMPARE(f.session.selection(), (std::vector<int>{1, 2}));  // 文件裡的元件編號
    }

    void undoPrunesAnySelectionThatNoLongerExists() {
        Fixture f;
        f.toPreview({0.95});
        f.session.accept();
        QCOMPARE(f.session.addManual({400, 300, 0}), InspectionSession::AddResult::Added);
        QCOMPARE(f.session.selection().size(), std::size_t{1});
        f.session.undo();  // 補的那個被復原
        QVERIFY(f.session.selection().empty());
    }

    void addManualInIdleNeedsATypeFirst() {
        Fixture f;
        QCOMPARE(f.session.addManual({100, 100, 0}), InspectionSession::AddResult::NeedType);
        f.toPreview({0.95});
        f.session.accept();
        QCOMPARE(f.session.addManual({400, 300, 0}), InspectionSession::AddResult::Added);
        const int id = f.session.selection().front();
        QVERIFY(f.session.document().find(id)->origin == ObjectOrigin::Manual);
    }

    void addManualUsesTheTypeOfTheSelectedComponent() {
        Fixture f;
        f.addSample();                                            // t1
        const int second = f.addSample(Rect{300, 300, 40, 20});  // t2（每個樣本一個類型，ADR-025）
        f.toPreview({0.95});  // 找到的都是 t1
        f.session.accept();
        QCOMPARE(f.session.document().board().types.size(), std::size_t{2});
        f.session.select({second});  // 第二個類型的樣本
        f.session.addManual({400, 300, 0});
        const int id = f.session.selection().front();
        QCOMPARE(f.session.document().find(id)->typeId, std::string("t2"));
        f.session.select({});
        f.session.addManual({450, 300, 0});  // 沒有選取：最近一次用的類型（剛補的 t2）
        QCOMPARE(f.session.document().find(f.session.selection().front())->typeId, std::string("t2"));
    }

    void addManualInPreviewAddsAProposalObject() {
        Fixture f;
        f.toPreview({0.95});
        QCOMPARE(f.session.addManual({400, 300, 0}), InspectionSession::AddResult::Added);
        QCOMPARE(f.session.proposal()->objects().manual, std::size_t{1});
        QCOMPARE(f.session.document().components().size(), std::size_t{1});  // 只剩樣本
    }

    void addManualIsNotAllowedWhileSearching() {
        Fixture f;
        f.search();
        QCOMPARE(f.session.addManual({1, 1, 0}), InspectionSession::AddResult::NotAllowed);
    }

    void movingTheSelectionInIdleIsOneCommandAndNudgesMerge() {
        Fixture f;
        f.toPreview({0.95});
        f.session.accept();
        f.session.select({2});
        const std::size_t depth = f.session.document().historyDepth();
        for (int i = 0; i < 4; ++i) QCOMPARE(f.session.moveSelection({1, 0}, true), std::size_t{1});
        QCOMPARE(f.session.document().historyDepth(), depth + 1);
        QCOMPARE(f.session.moveSelection({5, 0}), std::size_t{1});  // 一次拖曳放開 ＝ 新的一步
        QCOMPARE(f.session.document().historyDepth(), depth + 2);
    }

    void movingTheSelectionInPreviewMovesTheProposalObjects() {
        Fixture f;
        f.toPreview({0.95});
        f.session.select({2});
        f.session.moveSelection({10, 0});
        const ProposalObjects o = f.session.proposal()->objects();
        bool found = false;
        for (const InspectionObject& x : o.objects) {
            if (x.id == 2) {
                QCOMPARE(x.rect.pose.x, 160.0);
                found = true;
            }
        }
        QVERIFY(found);
        QCOMPARE(f.session.document().components().size(), std::size_t{1});  // 只剩樣本
    }

    // ---- 用真的 use case 與假 back end 走一遍 ----
    // ---- 接受＝換掉上一次自動產生的（ADR-025）----
    void acceptingASecondSearchReplacesTheFirstResultSoNoPositionIsListedTwice() {
        Fixture f;
        f.toPreview({0.95, 0.75});
        f.session.accept();
        QCOMPARE(f.session.document().components().size(), std::size_t{3});
        f.toPreview({0.90, 0.80, 0.70});  // 再搜尋一次（同一批位置）
        QCOMPARE(*f.session.accept(), std::size_t{3});
        QCOMPARE(f.session.document().components().size(), std::size_t{4});  // 樣本 ＋ 這一次的 3 個；上一次的 2 個換掉
        QVERIFY(f.session.undo());
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QCOMPARE(f.session.document().components().size(), std::size_t{3});
    }

    // ---- 改大小（ADR-025 §4）----
    void resizingAPartChangesItsTypeForEveryPartOfThatTypeOnlyInIdle() {
        Fixture f;
        const int sample = f.addSample();  // kBig：中心 (50, 36)、60 × 32
        f.toPreview({0.95});
        QVERIFY(!f.session.resizeBody(sample, Rect{0, 0, 80, 40}));  // 預覽中不改
        f.session.accept();                                           // 樣本 ＋ 1 個找到的（編號 2）
        QVERIFY(f.session.resizeBody(sample, Rect{10, 10, 80, 40}));
        QCOMPARE(f.session.document().bodySizePx("t1")->w, 80.0);
        QCOMPARE(f.session.document().find(sample)->pose.x, 50.0);  // 拖的那一個中心跟著框
        QCOMPARE(f.session.document().find(sample)->pose.y, 30.0);
        QCOMPARE(f.session.document().find(2)->pose.x, 150.0);                         // 其他的不動
        QCOMPARE(f.session.document().samples()[0].rect.w, 80.0);                      // 下次搜尋用新的框
        QVERIFY(!f.session.resizeBody(sample, Rect{0, 0, 5, 40}));  // 太小
        QVERIFY(!f.session.resizeBody(99, Rect{0, 0, 80, 40}));
        QVERIFY(f.session.undoInfo()->kind == CommandKind::ResizeType);
        QVERIFY(f.session.undo());
        QCOMPARE(f.session.document().bodySizePx("t1")->w, 60.0);
    }

    // ---- 選取資訊與套用大小到所有樣本（2026-10-14）----
    void applyingASizeToAllSamplesResizesEverySampleTypeInOneStepOnlyInIdle() {
        Fixture f;
        f.addSample();                        // t1：60 × 32（元件 1）
        f.addSample(Rect{300, 300, 40, 20});  // t2（元件 2，中心 (320, 310)）
        f.addSample(Rect{500, 300, 60, 32});  // t3：已經是 60 × 32
        f.session.select({});
        const std::size_t depth = f.session.document().historyDepth();
        QCOMPARE(f.session.applyBodySizeToSamples({60, 32}), std::size_t{1});  // 只有 t2 要改
        QCOMPARE(f.session.document().historyDepth(), depth + 1);
        QCOMPARE(f.session.document().bodySizePx("t2")->w, 60.0);
        QCOMPARE(f.session.document().find(2)->pose.x, 320.0);  // 中心不動
        QVERIFY(f.session.undoInfo()->kind == CommandKind::ResizeTypes);
        QCOMPARE(f.session.undoInfo()->count, std::size_t{1});
        QCOMPARE(f.session.applyBodySizeToSamples({60, 32}), std::size_t{0});  // 全部一樣：不進堆疊
        QCOMPARE(f.session.applyBodySizeToSamples({5, 32}), std::size_t{0});   // 太小
        QCOMPARE(f.session.document().historyDepth(), depth + 1);
        QCOMPARE(f.session.applyBodySizeToSamples({80, 40}), std::size_t{3});
        for (const SampleOnBoard& s : f.session.document().samples()) QCOMPARE(s.rect.w, 80.0);
        QVERIFY(f.session.undo());
        QCOMPARE(f.session.document().bodySizePx("t2")->w, 60.0);
        QCOMPARE(f.session.document().bodySizePx("t3")->w, 60.0);
        f.toPreview({0.95});
        QCOMPARE(f.session.applyBodySizeToSamples({90, 40}), std::size_t{0});  // 預覽中不改
    }

    void selectionInfoDescribesTheSingleSelectionAndWhatCanBeEdited() {
        Fixture f;
        const int sample = f.addSample();  // kBig：中心 (50, 36)、60 × 32；建完就是選取的
        SelectionInfo info = f.session.selectionInfo();
        QCOMPARE(info.count, std::size_t{1});
        QCOMPARE(info.id, sample);
        QVERIFY(!info.proposal);
        QCOMPARE(info.name, std::string("X1"));
        QCOMPARE(info.typeId, std::string("t1"));
        QCOMPARE(info.pose.x, 50.0);
        QCOMPARE(info.pose.y, 36.0);
        QCOMPARE(info.bodyPx.w, 60.0);
        QCOMPARE(info.bodyPx.h, 32.0);
        QCOMPARE(info.boxCount, std::size_t{1});
        QVERIFY(info.canMove);
        QVERIFY(info.canResize);
        // 表單輸入寬高：中心不動
        QVERIFY(f.session.resizeBodyKeepingCenter(sample, {80, 40}));
        info = f.session.selectionInfo();
        QCOMPARE(info.bodyPx.w, 80.0);
        QCOMPARE(info.pose.x, 50.0);
        QCOMPARE(info.pose.y, 36.0);
        QVERIFY(!f.session.resizeBodyKeepingCenter(sample, {5, 40}));  // 太小
        QVERIFY(!f.session.resizeBodyKeepingCenter(99, {80, 40}));
        f.session.select({});
        QCOMPARE(f.session.selectionInfo().count, std::size_t{0});
        // 預覽：多選只有數量；單一的提案物件只能改位置
        f.toPreview({0.95, 0.75});
        f.session.selectAll();
        QCOMPARE(f.session.selectionInfo().count, std::size_t{3});
        f.session.select({2});
        f.session.moveSelection({10, 0});
        info = f.session.selectionInfo();
        QCOMPARE(info.count, std::size_t{1});
        QVERIFY(info.proposal);
        QCOMPARE(info.id, 2);
        QCOMPARE(info.pose.x, 160.0);  // 候選的位姿加上移動
        QCOMPARE(info.typeId, std::string("t1"));
        QCOMPARE(info.bodyPx.w, 80.0);  // 類型現在的主體框
        QVERIFY(info.canMove);
        QVERIFY(!info.canResize);  // 預覽只能改位置（ADR-025 §4）
        f.search();
        QCOMPARE(f.session.selectionInfo().count, std::size_t{0});
    }

    // ---- 子框（ADR-025 §6）----
    void drawingABoxOnAPartAddsAPinGroupInTypeCoordinates() {
        Fixture f;
        const int sample = f.addSample(Rect{80, 90, 40, 20});  // 中心 (100, 100)
        f.session.select({sample});
        // 樣本的角度是 0：類型座標 ＝ 影像座標減去元件中心（未校正：1 µm ＝ 1 像素）。
        QVERIFY(f.session.addBoxGroup(sample, Rect{105, 95, 10, 6}));  // 中心 (110, 98)：相對 (10, −2)
        const ComponentType* t = f.session.document().findType("t1");
        QCOMPARE(t->groups.size(), std::size_t{2});
        QVERIFY(t->groups[1].kind == BoxGroupKind::Pin);
        QCOMPARE(t->groups[1].matrix.rows, 1);
        QCOMPARE(t->groups[1].matrix.cols, 1);
        QCOMPARE(t->groups[1].centerXUm, 10.0);
        QCOMPARE(t->groups[1].centerYUm, -2.0);
        QCOMPARE(t->groups[1].widthUm, 10.0);
        QCOMPARE(t->groups[1].heightUm, 6.0);
        QVERIFY(!f.session.addBoxGroup(sample, Rect{0, 0, 0, 6}));
        QVERIFY(!f.session.addBoxGroup(99, Rect{0, 0, 5, 6}));
        f.toPreview({0.95});
        QVERIFY(!f.session.addBoxGroup(sample, Rect{105, 95, 10, 6}));  // 只在 Idle
        // 預覽的物件也帶著類型：接受之後每個元件都有兩個框。
        f.session.accept();
        QCOMPARE(countBoxes(*f.session.document().findType("t1")), std::size_t{2});
    }

    // ---- 全選、選取同類型（ADR-025 §5）----
    void selectAllAndSelectSameTypeWorkOnTheBoardAndOnTheProposal() {
        Fixture f;
        f.addSample();                                            // t1，編號 1
        const int second = f.addSample(Rect{300, 300, 40, 20});  // t2，編號 2
        f.toPreview({0.95, 0.75});
        QCOMPARE(f.session.selectAll(), std::size_t{3});  // 預覽：提案的 3 個物件
        f.session.select({2});
        QCOMPARE(f.session.selectSameType(), std::size_t{3});  // 都是 t1
        f.session.accept();                                    // 板上：1、2、3、4（3、4 是 t1）
        QCOMPARE(f.session.selectAll(), std::size_t{4});
        f.session.select({second});
        QCOMPARE(f.session.selectSameType(), std::size_t{1});
        f.session.select({3});
        QCOMPARE(f.session.selectSameType(), std::size_t{3});
        QCOMPARE(f.session.selection(), (std::vector<int>{1, 3, 4}));
        f.session.select({});
        QCOMPARE(f.session.selectSameType(), std::size_t{0});
        f.search();
        QCOMPARE(f.session.selectAll(), std::size_t{0});  // 搜尋中不選
    }

    void theFullFlowWorksWithTheRealUseCaseAndTheFakeBackend() {
        TestImage image(400, 300);
        fake::FakeSpec spec;
        spec.halfCols = 3;
        spec.halfRows = 1;
        fake::FakeTemplateBuilder builder(spec);
        fake::FakeSearcher searcher(spec);
        GenerateObjectsFromSample useCase(builder, searcher);

        Fixture f;
        f.search(RegionMode::Sample, Rect{100, 100, 40, 20});
        const Result<SampleProposal> result =
            useCase.run(makeRequest(image, f.session.lastSearch()->region), CancelToken());
        QVERIFY(result.ok());
        QVERIFY(f.session.searchSucceeded(std::make_shared<const BoardProposal>(boardProposalFrom(result.value(), kTestTypeId))));
        QCOMPARE_STATE(f.session.state(), S::Preview);
        const ProposalObjects objects = f.session.proposal()->objects();
        QVERIFY(objects.found >= 2);
        const auto count = f.session.accept();
        QVERIFY(count.has_value());
        QVERIFY(*count >= 2);
        QCOMPARE(f.session.document().components().size(), *count + 1);  // 加上樣本
        QVERIFY(f.session.undo());
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QCOMPARE(f.session.document().components().size(), std::size_t{1});  // 只剩樣本
    }
};

QTEST_APPLESS_MAIN(TstSession)
#include "tst_session.moc"
