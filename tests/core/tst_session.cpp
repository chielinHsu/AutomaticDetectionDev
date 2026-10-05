// InspectionSession：狀態機＋文件＋提案＋選取的規則（沒有 Qt 的畫面，所以每一條都能決定性地驗證）。
#include <QtTest>

#include "ProposalTestSupport.h"
#include "CoreTestSupport.h"
#include "QtTestPrinters.h"
#include "application/interaction/InspectionSession.h"
#include "application/usecases/GenerateObjectsFromSample.h"

using namespace aoi;
using namespace aoi::test;
using S = InteractionState;

namespace {

const Rect kBig{20, 20, 60, 32};

// 一個走到 Idle 的工作階段。
struct Fixture {
    InspectionSession session;
    Fixture() { session.imageOpened("board.png", 1000, 800, 0.0); }

    // 畫一個區域並放開（進入 Searching）。
    void search(RegionMode mode = RegionMode::Sample, Rect region = kBig) {
        QVERIFY(session.beginRegion(mode));
        QVERIFY(session.endRegion(region, mode) == InspectionSession::Release::Search);
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
    void releasingABigRegionStartsASearchAndRemembersIt() {
        Fixture f;
        QVERIFY(f.session.beginRegion(RegionMode::Sample));
        QCOMPARE_STATE(f.session.state(), S::DrawingRegion);
        QVERIFY(f.session.endRegion(kBig, RegionMode::Sample) == InspectionSession::Release::Search);
        QCOMPARE_STATE(f.session.state(), S::Searching);
        QVERIFY(f.session.lastSearch().has_value());
        QCOMPARE(f.session.lastSearch()->region.w, 60.0);
        QVERIFY(f.session.lastSearch()->mode == RegionMode::Sample);
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
        QVERIFY(f.session.endRegion(Rect{0, 0, 8, 8}, RegionMode::Sample) == InspectionSession::Release::Search);
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

    void releasingANewRegionDropsTheOldProposal() {
        Fixture f;
        f.toPreview();
        QVERIFY(f.session.beginRegion(RegionMode::Sample));
        f.session.endRegion(kBig, RegionMode::Sample);
        QCOMPARE_STATE(f.session.state(), S::Searching);
        QVERIFY(f.session.proposal() == nullptr);
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
        auto p = std::make_shared<SampleProposal>();
        p->sampleProblem = NoMatchInfo{NoMatchReason::SampleTooSmall, 0, 0, 0};
        QVERIFY(f.session.searchSucceeded(p));
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
        QCOMPARE(*count, std::size_t{4});
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
        QVERIFY(!f.session.document().canUndo());
    }

    void discardGoesToIdleWithoutTouchingTheBoard() {
        Fixture f;
        f.toPreview();
        QVERIFY(f.session.discard());
        QCOMPARE_STATE(f.session.state(), S::Idle);
        QVERIFY(f.session.document().components().empty());
    }

    void undoAfterAcceptReturnsToPreviewWithTheSameProposal() {
        Fixture f;
        f.toPreview({0.95, 0.75});
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
        QVERIFY(f.session.document().components().empty());
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
        QVERIFY(f.session.document().components().empty());
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
        QVERIFY(f.session.document().components().empty());
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
        QVERIFY(f.session.beginRegion(RegionMode::Sample));
        f.session.endRegion(kBig, RegionMode::Sample);
        QVERIFY(f.session.searchSucceeded(makeProposal({0.90, 0.85})));  // 新的提案 B
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QVERIFY(!f.session.redo());                 // 這個 Ctrl+Y 不能把 A 接受進來：預覽中的是 B
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QVERIFY(f.session.document().components().empty());
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
        QVERIFY(f.session.document().components().empty());
        QVERIFY(!f.session.document().canUndo());
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
        f.toPreview({0.95});
        f.session.accept();
        f.toPreview({0.90});  // 第二份提案，產生第二個類型
        f.session.accept();
        QCOMPARE(f.session.document().board().types.size(), std::size_t{2});
        f.session.select({1});  // 第一個類型的元件
        f.session.addManual({400, 300, 0});
        const int id = f.session.selection().front();
        QCOMPARE(f.session.document().find(id)->typeId, f.session.document().find(1)->typeId);
    }

    void addManualInPreviewAddsAProposalObject() {
        Fixture f;
        f.toPreview({0.95});
        QCOMPARE(f.session.addManual({400, 300, 0}), InspectionSession::AddResult::Added);
        QCOMPARE(f.session.proposal()->objects().manual, std::size_t{1});
        QVERIFY(f.session.document().components().empty());
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
        QVERIFY(f.session.document().components().empty());
    }

    // ---- 用真的 use case 與假 back end 走一遍 ----
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
        QVERIFY(f.session.searchSucceeded(std::make_shared<const SampleProposal>(result.value())));
        QCOMPARE_STATE(f.session.state(), S::Preview);
        const ProposalObjects objects = f.session.proposal()->objects();
        QVERIFY(objects.found >= 2);
        const auto count = f.session.accept();
        QVERIFY(count.has_value());
        QVERIFY(*count >= 3);
        QCOMPARE(f.session.document().components().size(), *count);
        QVERIFY(f.session.undo());
        QCOMPARE_STATE(f.session.state(), S::Preview);
        QVERIFY(f.session.document().components().empty());
    }
};

QTEST_APPLESS_MAIN(TstSession)
#include "tst_session.moc"
