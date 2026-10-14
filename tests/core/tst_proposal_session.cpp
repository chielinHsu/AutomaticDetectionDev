// ProposalSession：一份提案、分數門檻（重新過濾，不進堆疊）、使用者在預覽中的修改（提案堆疊）。
#include <QtTest>
#include <cmath>

#include "ProposalTestSupport.h"
#include "application/document/ProposalSession.h"

using namespace aoi;
using namespace aoi::test;

namespace {

ProposalSession make(const std::vector<double>& scores, std::size_t predicted = 0, double threshold = 0.70) {
    return ProposalSession(makeProposal(scores, predicted), Rect{10, 10, 400, 300}, RegionMode::Sample, threshold);
}

const InspectionObject* byId(const ProposalObjects& o, int id) {
    for (const InspectionObject& x : o.objects) {
        if (x.id == id) return &x;
    }
    return nullptr;
}

}  // namespace

class TstProposalSession : public QObject {
    Q_OBJECT

private slots:
    // ---- 門檻 ----
    void objectsAreClassifiedByTheThreshold() {
        ProposalSession s = make({0.95, 0.75, 0.50}, 1);
        const ProposalObjects o = s.objects();
        QCOMPARE(o.objects.size(), std::size_t{1 + 3 + 1});
        QCOMPARE(o.sample, std::size_t{1});
        QCOMPARE(o.found, std::size_t{2});     // 0.95、0.75 ≥ 0.70
        QCOMPARE(o.rejected, std::size_t{1});  // 0.50
        QCOMPARE(o.inferred, std::size_t{1});
        QVERIFY(!o.noMatch.has_value());
    }

    void changingTheThresholdRefiltersWithoutChangingIds() {
        ProposalSession s = make({0.95, 0.75, 0.50});
        const ProposalObjects before = s.objects();
        s.setThreshold(0.90);
        const ProposalObjects after = s.objects();
        QCOMPARE(after.found, std::size_t{1});
        QCOMPARE(after.rejected, std::size_t{2});
        QCOMPARE(before.objects.size(), after.objects.size());
        for (std::size_t i = 0; i < before.objects.size(); ++i) QCOMPARE(before.objects[i].id, after.objects[i].id);
        QVERIFY(byId(after, 3)->state == ObjectState::Rejected);  // 0.75 在 0.90 下被拒絕
        QVERIFY(byId(before, 3)->state == ObjectState::Found);
    }

    void theThresholdIsNotAnEditAndDoesNotEnterTheStack() {
        ProposalSession s = make({0.95, 0.75});
        s.setThreshold(0.90);
        s.setThreshold(0.40);
        QVERIFY(!s.canUndo());
        QCOMPARE(s.editCount(), std::size_t{0});
    }

    void aThresholdAboveEveryScoreReportsTheSuggestion() {
        ProposalSession s = make({0.60, 0.55}, 0, 0.90);
        const ProposalObjects o = s.objects();
        QVERIFY(o.noMatch.has_value());
        QVERIFY(o.noMatch->reason == NoMatchReason::ThresholdTooHigh);
        QVERIFY(std::abs(o.noMatch->highestScore - 0.60) < 1e-12);
        // 建議值向下取整到小數第二位：不高於最高分（才過得了新門檻），而且差不到 0.01。
        QVERIFY(o.noMatch->suggestedThreshold <= 0.60 + 1e-12);
        QVERIFY(o.noMatch->suggestedThreshold >= 0.59 - 1e-12);
    }

    // ---- 刪除 ----
    void deleteRemovesTheObjectsAndUndoBringsThemBack() {
        ProposalSession s = make({0.95, 0.75, 0.50});
        const std::size_t before = s.objects().objects.size();
        QCOMPARE(s.deleteObjects({3, 4}), std::size_t{2});
        const ProposalObjects after = s.objects();
        QCOMPARE(after.objects.size(), before - 2);
        QVERIFY(byId(after, 3) == nullptr);
        QVERIFY(!s.contains(3));
        QVERIFY(s.contains(2));

        QVERIFY(s.undo());
        QCOMPARE(s.objects().objects.size(), before);
        QVERIFY(s.contains(3));
        QVERIFY(s.redo());
        QCOMPARE(s.objects().objects.size(), before - 2);
    }

    void deleteIgnoresUnknownAndAlreadyDeletedAndDuplicateIds() {
        ProposalSession s = make({0.95, 0.75});
        QCOMPARE(s.deleteObjects({99, 0, -1}), std::size_t{0});
        QVERIFY(!s.canUndo());  // 沒有改變，就不進堆疊
        QCOMPARE(s.deleteObjects({2, 2, 2}), std::size_t{1});
        QCOMPARE(s.deleteObjects({2}), std::size_t{0});
        QCOMPARE(s.editCount(), std::size_t{1});
    }

    void aDeletedObjectStaysDeletedWhenTheThresholdMoves() {
        ProposalSession s = make({0.95, 0.75});
        s.deleteObjects({2});
        s.setThreshold(0.30);
        QVERIFY(byId(s.objects(), 2) == nullptr);
    }

    void deleteCountsAreReflectedInTheSummary() {
        ProposalSession s = make({0.95, 0.75, 0.50}, 1);
        s.deleteObjects({2, 4, 5});  // 一個 Found（0.95）、一個 Rejected（0.50）、一個 Inferred
        const ProposalObjects o = s.objects();
        QCOMPARE(o.found, std::size_t{1});
        QCOMPARE(o.rejected, std::size_t{0});
        QCOMPARE(o.inferred, std::size_t{0});
    }

    // ---- 補一個 ----
    void addManualCreatesAFoundObjectWithoutAScore() {
        ProposalSession s = make({0.95});
        const int id = s.addManual({300, 120, 0}, kTestTypeId, {40, 20});
        QCOMPARE(id, 3);  // 候選是 1..2，手動補的接在後面
        const ProposalObjects o = s.objects();
        const InspectionObject* m = byId(o, id);
        QVERIFY(m != nullptr);
        QVERIFY(m->state == ObjectState::Found);
        QVERIFY(m->origin == ObjectOrigin::Manual);
        QVERIFY(!m->score.has_value());  // 不偽造分數（ux-spec §12.1 第 5 項）
        QCOMPARE(m->rect.width, 40.0);
        QCOMPARE(m->rect.height, 20.0);
        QCOMPARE(o.manual, std::size_t{1});
        QCOMPARE(o.found, std::size_t{2});
    }

    void manualIdsDoNotCollideAndSurviveThresholdChanges() {
        ProposalSession s = make({0.95, 0.75});
        const int a = s.addManual({300, 120, 0}, kTestTypeId, {40, 20});
        const int b = s.addManual({360, 120, 0}, kTestTypeId, {40, 20});
        QVERIFY(a != b);
        s.setThreshold(0.99);
        QVERIFY(byId(s.objects(), a) != nullptr);  // 手動補的不受門檻影響
        QVERIFY(byId(s.objects(), b) != nullptr);
    }

    void undoOfAddManualRemovesItAndRedoRestoresTheSameId() {
        ProposalSession s = make({0.95});
        const int id = s.addManual({300, 120, 0}, kTestTypeId, {40, 20});
        QVERIFY(s.undo());
        QVERIFY(byId(s.objects(), id) == nullptr);
        QVERIFY(s.redo());
        QVERIFY(byId(s.objects(), id) != nullptr);
        const int again = s.addManual({10, 10, 0}, kTestTypeId, {40, 20});
        QVERIFY(again != id);  // 重做之後再補，編號不重複
    }

    void deletingAManualObjectLeavesNoTrace() {
        ProposalSession s = make({0.95});
        const int id = s.addManual({300, 120, 0}, kTestTypeId, {40, 20});
        QCOMPARE(s.deleteObjects({id}), std::size_t{1});
        QVERIFY(byId(s.objects(), id) == nullptr);
        QVERIFY(s.undo());
        QVERIFY(byId(s.objects(), id) != nullptr);
    }

    // ---- 移動 ----
    void moveShiftsTheObjectsAndUndoRestoresTheExactPose() {
        ProposalSession s = make({0.95, 0.75});
        const double x0 = byId(s.objects(), 2)->rect.pose.x;
        QCOMPARE(s.moveObjects({2}, {7.3, -2.9}), std::size_t{1});
        const ProposalObjects after = s.objects();  // 指標要指向活著的物件：不能指進暫存的 objects()
        const InspectionObject* moved = byId(after, 2);
        QVERIFY(moved != nullptr);
        QVERIFY(std::abs(moved->rect.pose.x - (x0 + 7.3)) < 1e-12);
        QVERIFY(std::abs(moved->rect.pose.y - (100 - 2.9)) < 1e-12);
        QVERIFY(s.undo());
        QCOMPARE(byId(s.objects(), 2)->rect.pose.x, x0);  // 精確相等，不是近似
        QCOMPARE(byId(s.objects(), 2)->rect.pose.y, 100.0);
    }

    void movingAManualObjectMovesItsPose() {
        ProposalSession s = make({0.95});
        const int id = s.addManual({300, 120, 0}, kTestTypeId, {40, 20});
        s.moveObjects({id}, {10, 5});
        QCOMPARE(byId(s.objects(), id)->rect.pose.x, 310.0);
        QCOMPARE(byId(s.objects(), id)->rect.pose.y, 125.0);
        s.undo();
        QCOMPARE(byId(s.objects(), id)->rect.pose.x, 300.0);
    }

    void consecutiveNudgesOfTheSameObjectsMergeIntoOneStepWhenAllowed() {
        ProposalSession s = make({0.95, 0.75});
        s.moveObjects({2, 3}, {1, 0}, true);
        s.moveObjects({2, 3}, {1, 0}, true);
        s.moveObjects({2, 3}, {1, 0}, true);
        QCOMPARE(s.editCount(), std::size_t{1});
        QCOMPARE(byId(s.objects(), 2)->rect.pose.x, 153.0);
        QVERIFY(s.undo());
        QCOMPARE(byId(s.objects(), 2)->rect.pose.x, 150.0);  // 一次回到開始前
    }

    void nudgesOfDifferentObjectsDoNotMerge() {
        ProposalSession s = make({0.95, 0.75});
        s.moveObjects({2}, {1, 0}, true);
        s.moveObjects({3}, {1, 0}, true);
        QCOMPARE(s.editCount(), std::size_t{2});
    }

    void movesWithoutPermissionNeverMerge() {
        ProposalSession s = make({0.95});
        s.moveObjects({2}, {1, 0});
        s.moveObjects({2}, {1, 0});  // 兩次拖曳放開 ＝ 兩步
        QCOMPARE(s.editCount(), std::size_t{2});
    }

    void movingAnUnknownObjectDoesNothing() {
        ProposalSession s = make({0.95});
        QCOMPARE(s.moveObjects({77}, {1, 1}), std::size_t{0});
        QVERIFY(!s.canUndo());
    }

    // ---- 堆疊 ----
    void undoInfoTellsWhatWillBeUndone() {
        ProposalSession s = make({0.95, 0.75});
        QVERIFY(!s.undoInfo().has_value());
        s.deleteObjects({2, 3});
        QVERIFY(s.undoInfo()->kind == CommandKind::DeleteComponents);
        QCOMPARE(s.undoInfo()->count, std::size_t{2});
        s.undo();
        QVERIFY(!s.undoInfo().has_value());
        QVERIFY(s.redoInfo()->kind == CommandKind::DeleteComponents);
    }

    void aNewEditDiscardsTheRedoTail() {
        ProposalSession s = make({0.95, 0.75});
        s.deleteObjects({2});
        s.undo();
        QVERIFY(s.canRedo());
        s.addManual({10, 10, 0}, kTestTypeId, {40, 20});
        QVERIFY(!s.canRedo());
    }

    void dropRedoClearsOnlyTheRedoPart() {
        ProposalSession s = make({0.95, 0.75});
        s.deleteObjects({2});
        s.addManual({10, 10, 0}, kTestTypeId, {40, 20});
        s.undo();
        s.dropRedo();
        QVERIFY(!s.canRedo());
        QVERIFY(s.canUndo());
    }

    void aCopyCarriesTheWholeProposalAndItsStackIndependently() {
        ProposalSession s = make({0.95, 0.75});
        s.deleteObjects({2});
        ProposalSession copy = s;  // 接受／捨棄會帶走一份
        s.addManual({10, 10, 0}, kTestTypeId, {40, 20});
        s.undo();
        s.undo();
        QCOMPARE(s.objects().objects.size(), std::size_t{3});  // 原本的回到最初
        QCOMPARE(copy.objects().objects.size(), std::size_t{2});  // 複本還是刪過一個的狀態
        QVERIFY(copy.undo());
        QCOMPARE(copy.objects().objects.size(), std::size_t{3});
        QVERIFY(copy.proposalPtr() == s.proposalPtr());  // 提案本身是共用的不可變資料
    }

    void regionAndModeArePartOfTheSession() {
        const ProposalSession s = make({0.95});
        QCOMPARE(s.region().w, 400.0);
        QVERIFY(s.mode() == RegionMode::Sample);
    }

    void aProposalWithASampleProblemHasNoObjectsAndExplainsWhy() {
        ProposalSession s(makeProblemProposal(NoMatchReason::SampleTooSmall), Rect{0, 0, 5, 5}, RegionMode::Sample, 0.7);
        const ProposalObjects o = s.objects();
        QVERIFY(o.objects.empty());
        QVERIFY(o.noMatch.has_value());
        QVERIFY(o.noMatch->reason == NoMatchReason::SampleTooSmall);
        QVERIFY(!s.contains(1));
    }
};

QTEST_APPLESS_MAIN(TstProposalSession)
#include "tst_proposal_session.moc"
