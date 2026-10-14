// InspectionDocument：已接受的文件、每個修改一個 Command、一次變更通知、復原後等於原狀（ADR-002）。
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <vector>

#include "ProposalTestSupport.h"
#include "application/document/InspectionDocument.h"
#include "domain/box_expander.h"

using namespace aoi;
using namespace aoi::test;

namespace {

ProposalSession session(const std::vector<double>& scores, std::size_t predicted = 0, double threshold = 0.70) {
    return ProposalSession(makeProposal(scores, predicted), Rect{10, 10, 400, 300}, RegionMode::Sample, threshold);
}

bool samePose(const Pose2D& a, const Pose2D& b) { return a.x == b.x && a.y == b.y && a.angleDeg == b.angleDeg; }

bool sameComponents(const std::vector<ComponentInstance>& a, const std::vector<ComponentInstance>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id || a[i].name != b[i].name || a[i].typeId != b[i].typeId || a[i].state != b[i].state ||
            a[i].origin != b[i].origin || a[i].score != b[i].score || !samePose(a[i].pose, b[i].pose))
            return false;
    }
    return true;
}

std::vector<int> ids(const InspectionDocument& d) {
    std::vector<int> out;
    for (const ComponentInstance& c : d.components()) out.push_back(c.id);
    return out;
}

// 計算變更通知。
struct Counting {
    int total = 0;
    int edits = 0, undos = 0, redos = 0, resets = 0;
    DocumentChange last;
    void attach(InspectionDocument& d) {
        d.setObserver([this](const DocumentChange& c) {
            ++total;
            last = c;
            switch (c.cause) {
                case DocumentChange::Cause::Edit: ++edits; break;
                case DocumentChange::Cause::Undo: ++undos; break;
                case DocumentChange::Cause::Redo: ++redos; break;
                case DocumentChange::Cause::Reset: ++resets; break;
            }
        });
    }
};

// 板子上已經有一個樣本（ADR-025：每個樣本一個類型）：類型 t1（主體 40 × 20 µm，未校正 1 µm ＝ 1 像素）、樣本元件 X1（編號 1，
// 中心 (100, 100)，和 makeProposal 的樣本相同）。接受提案時 Sample 物件不再加進來，所以新元件從編號 2 開始。
// 沒有樣本、歷史是空的（只看歷史與重設的規則時用）。
InspectionDocument bareDocument() {
    InspectionDocument d;
    d.reset("board.png", 1000, 800, 0.0);
    return d;
}

InspectionDocument freshDocument() {
    InspectionDocument d;
    d.reset("board.png", 1000, 800, 0.0);
    const int sample = d.addSample(Rect{80, 90, 40, 20});
    Q_ASSERT(sample == 1);
    Q_UNUSED(sample);
    return d;
}

}  // namespace

class TstDocument : public QObject {
    Q_OBJECT

private slots:
    // ---- 接受 ----
    void acceptTurnsSampleFoundAndInferredIntoComponentsAndDropsRejected() {
        InspectionDocument d = freshDocument();
        const ProposalSession s = session({0.95, 0.75, 0.50}, 1);  // Sample、Found、Found、Rejected、Inferred
        QCOMPARE(d.accept(s), std::size_t{3});  // Sample 已經在板子上（freshDocument 建的），不再加（ADR-025）
        QCOMPARE(d.components().size(), std::size_t{4});
        QVERIFY(d.components()[0].state == ObjectState::Sample);
        QVERIFY(d.components()[1].state == ObjectState::Found);
        QVERIFY(d.components()[3].state == ObjectState::Inferred);
        for (const ComponentInstance& c : d.components()) QVERIFY(c.state != ObjectState::Rejected);
    }

    void acceptAssignsIncreasingIdsAndNamesAndKeepsScoresAndPoses() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95, 0.75}));
        QCOMPARE(ids(d), (std::vector<int>{1, 2, 3}));
        QCOMPARE(d.components()[0].name, std::string("X1"));
        QCOMPARE(d.components()[2].name, std::string("X3"));
        QCOMPARE(*d.components()[1].score, 0.95);
        QVERIFY(samePose(d.components()[1].pose, Pose2D{150, 100, 0}));
        QVERIFY(d.components()[0].origin == ObjectOrigin::Manual);  // 樣本是使用者畫的
        QVERIFY(d.components()[1].origin == ObjectOrigin::Auto);
    }

    // ADR-025：類型在建立樣本時就有了（一個樣本一個類型，只有主體框）；接受不建類型，元件指向各自的類型。
    void addSampleCreatesOneTypeWithABodySizedFromTheBoxAndOneSampleComponent() {
        InspectionDocument d;
        d.reset("board.png", 1000, 800, 0.0);
        QCOMPARE(d.addSample(Rect{80, 90, 40, 20}), 1);
        QCOMPARE(d.board().types.size(), std::size_t{1});
        const ComponentType& t = d.board().types[0];
        QCOMPARE(t.id, std::string("t1"));
        QCOMPARE(t.libId, std::string());  // 只在這塊板子上
        QCOMPARE(t.groups.size(), std::size_t{1});
        QVERIFY(t.groups[0].kind == BoxGroupKind::Body);
        QCOMPARE(t.groups[0].widthUm, 40.0);   // 未校正：暫定 1 µm ＝ 1 像素
        QCOMPARE(t.groups[0].heightUm, 20.0);
        QCOMPARE(d.components().size(), std::size_t{1});
        const ComponentInstance& s = d.components()[0];
        QCOMPARE(s.name, std::string("X1"));
        QVERIFY(s.state == ObjectState::Sample);
        QVERIFY(s.origin == ObjectOrigin::Manual);
        QVERIFY(samePose(s.pose, Pose2D{100, 100, 0}));
        QCOMPARE(s.typeId, t.id);
        QCOMPARE(d.lastTypeId(), t.id);
        QCOMPARE(d.samples().size(), std::size_t{1});
        QCOMPARE(d.samples()[0].rect.x, 80.0);
        QVERIFY(d.undoInfo()->kind == CommandKind::AddSample);
        QVERIFY(d.undo().performed);
        QVERIFY(d.components().empty());
        QVERIFY(d.board().types.empty());
        QCOMPARE(d.lastTypeId(), std::string());
        QCOMPARE(d.addSample(Rect{0, 0, 0, 10}), 0);  // 不合法的框
    }

    void acceptPointsEveryComponentAtTheSampleTypeAndCreatesNoType() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95, 0.75}));
        QCOMPARE(d.board().types.size(), std::size_t{1});
        for (const ComponentInstance& c : d.components()) QCOMPARE(c.typeId, std::string("t1"));
        QCOMPARE(d.lastTypeId(), std::string("t1"));
    }

    void theBodySizeUsesTheImageCalibration() {
        InspectionDocument d;
        d.reset("calibrated.png", 1000, 800, 2.5);  // 2.5 µm／像素
        d.addSample(Rect{80, 90, 40, 20});
        QCOMPARE(d.board().types[0].groups[0].widthUm, 100.0);  // 40 px × 2.5
        QCOMPARE(d.board().types[0].groups[0].heightUm, 50.0);
        QCOMPARE(d.bodySizePx("t1")->w, 40.0);  // 換回像素
        QCOMPARE(d.samples()[0].rect.w, 40.0);
    }

    void acceptingNothingAcceptableDoesNotTouchTheHistory() {
        InspectionDocument d = freshDocument();
        ProposalSession s = session({0.50, 0.40});  // 只有 Sample 以外都 Rejected；先刪掉 Sample
        s.deleteObjects({1});
        QCOMPARE(d.accept(s), std::size_t{0});
        QCOMPARE(d.historyDepth(), std::size_t{1});  // 只有建立樣本那一步
        QCOMPARE(d.components().size(), std::size_t{1});  // 只剩樣本
        QCOMPARE(d.board().types.size(), std::size_t{1});
    }

    void undoOfAcceptLeavesTheDocumentExactlyAsItWas() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95, 0.75}));
        const Board before = d.board();
        const std::string lastBefore = d.lastTypeId();
        d.accept(session({0.90}, 0, 0.70));
        QCOMPARE(d.components().size(), std::size_t{2});  // 上一次自動產生的 2 個換成這一次的 1 個，樣本留著（ADR-025）
        const UndoOutcome out = d.undo();
        QVERIFY(out.performed);
        QVERIFY(sameComponents(d.components(), before.components));
        QCOMPARE(d.board().types.size(), before.types.size());
        QCOMPARE(d.lastTypeId(), lastBefore);
    }

    void acceptUndoAcceptGivesTheSameIdsAndNamesAgain() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95}));
        const std::vector<ComponentInstance> first = d.components();
        d.undo();
        QCOMPARE(d.components().size(), std::size_t{1});  // 只剩樣本
        d.accept(session({0.95}));  // 計數隨復原還原，所以編號與名稱從頭開始
        QVERIFY(sameComponents(d.components(), first));
    }

    void redoOfAcceptRestoresTheSameComponentsAndType() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95, 0.75}));
        const std::vector<ComponentInstance> after = d.components();
        const std::string typeId = d.board().types[0].id;
        d.undo();
        const UndoOutcome out = d.redo();
        QVERIFY(out.performed);
        QVERIFY(out.consumedProposal);
        QVERIFY(sameComponents(d.components(), after));
        QCOMPARE(d.board().types.size(), std::size_t{1});
        QCOMPARE(d.board().types[0].id, typeId);
    }

    void idsAreNeverReusedAfterADeleteSoAnOldSelectionCannotPointAtANewComponent() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95, 0.75}));  // 1 2 3
        d.removeComponents({3});
        d.accept(session({0.90}));  // 自動產生的 2 被換掉
        QCOMPARE(ids(d), (std::vector<int>{1, 4}));  // 3 沒有被重用
    }

    // ---- 捨棄 ----
    void discardLeavesTheBoardAloneButCarriesTheProposal() {
        InspectionDocument d = freshDocument();
        ProposalSession s = session({0.95, 0.75, 0.50});
        s.deleteObjects({2});
        d.discard(s);
        QCOMPARE(d.components().size(), std::size_t{1});  // 只剩樣本
        QVERIFY(d.canUndo());
        QVERIFY(d.undoInfo()->kind == CommandKind::Discard);

        UndoOutcome out = d.undo();
        QVERIFY(out.performed);
        QVERIFY(out.restoredProposal.has_value());
        QVERIFY(out.restoredProposal->proposalPtr() == s.proposalPtr());
        QVERIFY(!out.restoredProposal->contains(2));            // 預覽中刪掉的，還給預覽時還是刪掉的
        QVERIFY(out.restoredProposal->undo());                  // 連提案的堆疊一起還原
        QVERIFY(out.restoredProposal->contains(2));
    }

    void undoOfAcceptReturnsTheProposalWithItsEditStackAndThreshold() {
        InspectionDocument d = freshDocument();
        ProposalSession s = session({0.95, 0.75, 0.50}, 0, 0.60);
        s.deleteObjects({3});
        s.addManual({400, 100, 0}, kTestTypeId, {40, 20});
        d.accept(s);
        UndoOutcome out = d.undo();
        QVERIFY(out.restoredProposal.has_value());
        QCOMPARE(out.restoredProposal->threshold(), 0.60);
        QCOMPARE(out.restoredProposal->editCount(), std::size_t{2});
        QVERIFY(out.restoredProposal->canUndo());
        QVERIFY(!out.restoredProposal->contains(3));
        QCOMPARE(d.components().size(), std::size_t{1});  // 只剩樣本
    }

    void redoOfDiscardConsumesTheProposalAgain() {
        InspectionDocument d = freshDocument();
        d.discard(session({0.95}));
        d.undo();
        const UndoOutcome out = d.redo();
        QVERIFY(out.performed);
        QVERIFY(out.consumedProposal);
        QVERIFY(out.kind == CommandKind::Discard);
    }

    void nextRedoProposalIsOnlySetForAcceptAndDiscard() {
        InspectionDocument d = freshDocument();
        const ProposalSession s = session({0.95});
        d.accept(s);
        d.removeComponents({1});
        d.undo();  // 刪除被復原；下一個重做是刪除
        QVERIFY(d.nextRedoProposal() == nullptr);
        d.undo();  // 接受被復原；下一個重做是接受
        QVERIFY(d.nextRedoProposal() != nullptr);
        QVERIFY(d.nextRedoProposal()->proposalPtr() == s.proposalPtr());
    }

    // ---- 接受＝換掉上一次自動產生的元件（ADR-025）----
    void acceptReplacesTheLastAutomaticResultButKeepsSamplesAndManualParts() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95, 0.75}));                       // 1 樣本、2 自動（2、3）
        const int manual = d.addManualComponent({500, 400, 0}, "t1");  // 4，手動
        d.addSample(Rect{300, 300, 40, 20});                   // t2、樣本 5
        QCOMPARE(d.components().size(), std::size_t{5});
        const std::vector<ComponentInstance> before = d.components();

        QCOMPARE(d.accept(session({0.90, 0.80, 0.70})), std::size_t{3});  // 6、7、8
        QCOMPARE(ids(d), (std::vector<int>{1, 4, 5, 6, 7, 8}));  // 自動的 2、3 換掉；樣本 1、5 與手動的 4 留著
        QVERIFY(d.find(manual)->origin == ObjectOrigin::Manual);
        QCOMPARE(d.undoInfo()->count, std::size_t{3});
        QVERIFY(d.undo().performed);
        QVERIFY(sameComponents(d.components(), before));  // 復原：換掉的回到原來的位置
        QVERIFY(d.redo().performed);
        QCOMPARE(ids(d), (std::vector<int>{1, 4, 5, 6, 7, 8}));
    }

    void acceptSkipsCandidatesWhoseTypeIsNotOnTheBoard() {
        InspectionDocument d = freshDocument();
        QCOMPARE(d.accept(ProposalSession(makeProposal({0.95}, 0, {40, 20}, "nope"), Rect{}, RegionMode::Sample, 0.7)), std::size_t{0});
        QCOMPARE(d.historyDepth(), std::size_t{1});
    }

    // ---- 改主體框的大小（ADR-025 §4）----
    void setBodySizeChangesTheTypeForEveryPartAndMovesOnlyTheAnchor() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95, 0.75}));  // 1 樣本、2、3 都是 t1
        const Pose2D pose3 = d.find(3)->pose;
        QVERIFY(d.setBodySize("t1", {60, 30}, 2, Pose2D{160, 105, 0}));
        QCOMPARE(d.board().types[0].groups[0].widthUm, 60.0);
        QCOMPARE(d.board().types[0].groups[0].heightUm, 30.0);
        QCOMPARE(d.bodySizePx("t1")->h, 30.0);
        QVERIFY(samePose(d.find(2)->pose, Pose2D{160, 105, 0}));
        QVERIFY(samePose(d.find(3)->pose, pose3));  // 其他元件的位姿不動
        QCOMPARE(d.samples()[0].rect.w, 60.0);         // 樣本的框跟著類型變
        QVERIFY(d.undoInfo()->kind == CommandKind::ResizeType);
        QCOMPARE(d.undoInfo()->count, std::size_t{3});  // 同類型的 3 個元件
        QVERIFY(d.undo().performed);
        QCOMPARE(d.board().types[0].groups[0].widthUm, 40.0);
        QVERIFY(samePose(d.find(2)->pose, Pose2D{150, 100, 0}));
        QVERIFY(!d.setBodySize("t1", {0, 30}, 2, Pose2D{}));     // 不合法的大小
        QVERIFY(!d.setBodySize("nope", {60, 30}, 2, Pose2D{}));  // 沒有這個類型
        QVERIFY(!d.setBodySize("t1", {60, 30}, 99, Pose2D{}));   // 沒有這個元件
    }

    // ---- 套用大小到所有樣本（2026-10-14，選取資訊表單）----
    void setBodySizeForTypesIsOneCommandAndUndoRestoresEachType() {
        InspectionDocument d = freshDocument();  // t1：40 × 20
        d.addSample(Rect{200, 200, 60, 30});     // t2（元件 2）
        d.addSample(Rect{300, 300, 50, 25});     // t3（元件 3）：已經是 50 × 25
        d.accept(session({0.95}));               // 元件 4（t1）
        Counting n;
        n.attach(d);
        const Pose2D pose2 = d.find(2)->pose;
        const Pose2D pose4 = d.find(4)->pose;
        // t3 已是這個大小、t2 重複、nope 不存在：只有 t1、t2 改，一個 Command、一次通知。
        QVERIFY(d.setBodySizeForTypes({"t1", "t2", "t3", "t2", "nope"}, {50, 25}));
        QCOMPARE(n.total, 1);
        QVERIFY(d.undoInfo()->kind == CommandKind::ResizeTypes);
        QCOMPARE(d.undoInfo()->count, std::size_t{2});
        for (const char* t : {"t1", "t2", "t3"}) {
            QCOMPARE(d.bodySizePx(t)->w, 50.0);
            QCOMPARE(d.bodySizePx(t)->h, 25.0);
        }
        QVERIFY(samePose(d.find(2)->pose, pose2));  // 位姿都不動（中心不變）
        QVERIFY(samePose(d.find(4)->pose, pose4));
        QCOMPARE(d.samples()[0].rect.w, 50.0);      // 樣本的框跟著類型變
        QVERIFY(d.undo().performed);
        QCOMPARE(d.bodySizePx("t1")->w, 40.0);
        QCOMPARE(d.bodySizePx("t2")->w, 60.0);
        QCOMPARE(d.bodySizePx("t3")->w, 50.0);
        QVERIFY(d.redo().performed);
        QCOMPARE(d.bodySizePx("t2")->h, 25.0);
        QCOMPARE(n.total, 3);
        QVERIFY(!d.setBodySizeForTypes({"t1"}, {0, 25}));                 // 不合法的大小
        QVERIFY(!d.setBodySizeForTypes({"nope"}, {50, 25}));              // 沒有這個類型
        QVERIFY(!d.setBodySizeForTypes({"t1", "t2", "t3"}, {50, 25}));    // 已經都是：不進堆疊
        QCOMPARE(n.total, 3);
    }

    // ---- 子框（ADR-025 §6）----
    void addBoxGroupAddsAPinGroupToTheTypeAndEveryPartExpandsIt() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95}));
        BoxGroup g;
        g.kind = BoxGroupKind::Pin;
        g.widthUm = 10;
        g.heightUm = 6;
        g.centerXUm = -12;
        g.centerYUm = 4;
        g.matrix.rows = 1;
        g.matrix.cols = 1;
        QVERIFY(d.addBoxGroup("t1", g));
        const ComponentType& t = d.board().types[0];
        QCOMPARE(t.groups.size(), std::size_t{2});
        QCOMPARE(t.groups[1].id, 2);  // 文件指派的編號
        QCOMPARE(countBoxes(t), std::size_t{2});
        const std::vector<ExpandedBox> boxes = expandBoxes(t, d.find(2)->pose, 0.0);  // (150, 100)
        QCOMPARE(boxes.size(), std::size_t{2});
        QCOMPARE(boxes[1].rect.pose.x, 138.0);
        QCOMPARE(boxes[1].rect.pose.y, 104.0);
        QVERIFY(d.undoInfo()->kind == CommandKind::AddBoxGroup);
        QCOMPARE(d.undoInfo()->count, std::size_t{2});
        QVERIFY(d.undo().performed);
        QCOMPARE(d.board().types[0].groups.size(), std::size_t{1});
        QVERIFY(!d.addBoxGroup("nope", g));
    }

    void componentsOfTypeListsTheIdsInOrder() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95, 0.75}));
        d.addSample(Rect{300, 300, 40, 20});
        QCOMPARE(d.componentsOfType("t1"), (std::vector<int>{1, 2, 3}));
        QCOMPARE(d.componentsOfType("t2"), (std::vector<int>{4}));
        QVERIFY(d.componentsOfType("nope").empty());
    }

    // ---- 刪除 ----
    void removeAndUndoRestoreTheSamePositionsIdsAndNames() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95, 0.90, 0.85, 0.80, 0.75, 0.72}));  // 1..7
        const std::vector<ComponentInstance> before = d.components();
        QCOMPARE(d.removeComponents({2, 5, 7}), std::size_t{3});
        QCOMPARE(ids(d), (std::vector<int>{1, 3, 4, 6}));
        QVERIFY(d.undo().performed);
        QVERIFY(sameComponents(d.components(), before));  // 位置、編號、名稱、分數、位姿全部相同
        QVERIFY(d.redo().performed);
        QCOMPARE(ids(d), (std::vector<int>{1, 3, 4, 6}));
    }

    void removeIgnoresUnknownAndDuplicateIds() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95}));
        QCOMPARE(d.removeComponents({99, 1, 1}), std::size_t{1});
        QCOMPARE(d.removeComponents({1}), std::size_t{0});  // 已經刪了
        QCOMPARE(d.historyDepth(), std::size_t{3});         // 建立樣本＋接受＋一次有效的刪除
    }

    void findUsesTheIdNotThePosition() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95, 0.90, 0.85}));
        d.removeComponents({1, 2});
        QVERIFY(d.find(1) == nullptr);
        QVERIFY(d.find(3) != nullptr);
        QCOMPARE(d.find(4)->id, 4);
        QVERIFY(d.find(99) == nullptr);
    }

    // ---- 補一個 ----
    void addManualUsesTheGivenTypeAndHasNoScore() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95}));
        const std::string type = d.board().types[0].id;
        const int id = d.addManualComponent({500, 400, 0}, type);
        QCOMPARE(id, 3);
        const ComponentInstance* c = d.find(id);
        QVERIFY(c != nullptr);
        QCOMPARE(c->name, std::string("X3"));
        QVERIFY(c->origin == ObjectOrigin::Manual);
        QVERIFY(c->state == ObjectState::Found);
        QVERIFY(!c->score.has_value());
        QCOMPARE(c->typeId, type);
    }

    void addManualWithAnUnknownTypeDoesNothing() {
        InspectionDocument d = bareDocument();
        QCOMPARE(d.addManualComponent({1, 1, 0}, "nope"), 0);
        QVERIFY(!d.canUndo());
    }

    void undoOfAddManualRemovesItAndFreesTheNameAndId() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95}));
        const std::string type = d.board().types[0].id;
        const int id = d.addManualComponent({500, 400, 0}, type);
        d.undo();
        QVERIFY(d.find(id) == nullptr);
        const int again = d.addManualComponent({10, 10, 0}, type);
        QCOMPARE(again, id);
        QCOMPARE(d.find(again)->name, std::string("X3"));
    }

    // ---- 移動 ----
    void moveAndUndoRestoreTheExactPose() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95}));
        const Pose2D before = d.find(2)->pose;
        QCOMPARE(d.moveComponents({2}, {3.7, -1.3}), std::size_t{1});
        QVERIFY(!samePose(d.find(2)->pose, before));
        d.undo();
        QVERIFY(samePose(d.find(2)->pose, before));  // 精確相等
        d.redo();
        QVERIFY(std::abs(d.find(2)->pose.x - (before.x + 3.7)) < 1e-12);
    }

    void aDragIsOneCommandAndTwoDragsAreTwo() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95}));
        d.moveComponents({2}, {5, 0});
        d.moveComponents({2}, {5, 0});
        QCOMPARE(d.historyDepth(), std::size_t{4});  // 建立樣本＋接受＋兩次拖曳
    }

    void nudgesOfTheSameComponentsMergeWhenAllowed() {
        InspectionDocument d = freshDocument();
        d.accept(session({0.95, 0.90}));
        const Pose2D start = d.find(2)->pose;
        for (int i = 0; i < 5; ++i) d.moveComponents({2, 3}, {1, 0}, true);
        QCOMPARE(d.historyDepth(), std::size_t{3});  // 建立樣本＋接受＋一步微調
        QCOMPARE(d.find(2)->pose.x, start.x + 5);
        d.undo();
        QVERIFY(samePose(d.find(2)->pose, start));
    }

    void movingNothingKnownDoesNothing() {
        InspectionDocument d = bareDocument();
        QCOMPARE(d.moveComponents({1, 2}, {1, 1}), std::size_t{0});
        QVERIFY(!d.canUndo());
    }

    // ---- 通知：每個 Command 一次（ADR-002 規則 11）----
    void everyCommandNotifiesExactlyOnce() {
        InspectionDocument d = freshDocument();
        Counting n;
        n.attach(d);
        d.accept(session({0.95, 0.90, 0.85}));
        QCOMPARE(n.total, 1);
        QVERIFY(n.last.kind == CommandKind::Accept);
        QCOMPARE(n.last.count, std::size_t{3});
        d.removeComponents({1, 2, 3});
        QCOMPARE(n.total, 2);
        d.moveComponents({4}, {1, 1});
        QCOMPARE(n.total, 3);
        d.addManualComponent({1, 1, 0}, d.board().types[0].id);
        QCOMPARE(n.total, 4);
        d.discard(session({0.95}));
        QCOMPARE(n.total, 5);
        d.undo();
        QCOMPARE(n.total, 6);
        QCOMPARE(n.undos, 1);
        d.redo();
        QCOMPARE(n.total, 7);
        QCOMPARE(n.redos, 1);
        QCOMPARE(n.edits, 5);
    }

    void aThousandComponentCommitNotifiesOncePerCommandNotOncePerComponent() {
        InspectionDocument d = freshDocument();
        Counting n;
        n.attach(d);
        std::vector<double> scores(1000, 0.9);
        const ProposalSession big = ProposalSession(makeProposal(scores), Rect{0, 0, 10, 10}, RegionMode::Sample, 0.7);
        QCOMPARE(d.accept(big), std::size_t{1000});
        QCOMPARE(n.total, 1);
        d.undo();
        QCOMPARE(n.total, 2);
        QCOMPARE(d.components().size(), std::size_t{1});  // 只剩樣本
        d.redo();
        QCOMPARE(n.total, 3);
        QCOMPARE(d.components().size(), std::size_t{1001});
    }

    // S11：修訂號和變更通知同步（讀取端用它判斷快取還能不能用），不改板子內容的操作不算。
    void theRevisionCountsEveryChangeAndNothingElse() {
        InspectionDocument d = bareDocument();
        const std::uint64_t start = d.revision();
        d.addSample(Rect{80, 90, 40, 20});
        QCOMPARE(d.revision(), start + 1);
        d.accept(session({0.95, 0.90}));
        QCOMPARE(d.revision(), start + 2);
        d.removeComponents({1});
        QCOMPARE(d.revision(), start + 3);
        d.undo();
        QCOMPARE(d.revision(), start + 4);
        d.redo();
        QCOMPARE(d.revision(), start + 5);
        d.undo();  // 什麼都沒變的復原不通知，也不加
        d.undo();
        d.undo();
        QCOMPARE(d.revision(), start + 8);
        QVERIFY(!d.undo().performed);
        QCOMPARE(d.revision(), start + 8);
        d.markSaved();
        d.dropRedo();
        QVERIFY(d.setSignificantBits(12));
        QCOMPARE(d.revision(), start + 8);
        d.reset("b.png", 10, 10, 1.0);
        QCOMPARE(d.revision(), start + 9);
    }

    void anUndoThatChangesNothingDoesNotNotify() {
        InspectionDocument d = bareDocument();
        Counting n;
        n.attach(d);
        QVERIFY(!d.undo().performed);
        QVERIFY(!d.redo().performed);
        QCOMPARE(n.total, 0);
    }

    // ---- 歷史資訊、重設、儲存狀態 ----
    void undoInfoNamesTheCommandAndItsCount() {
        InspectionDocument d = bareDocument();
        QVERIFY(!d.undoInfo().has_value());
        d.addSample(Rect{80, 90, 40, 20});
        QVERIFY(d.undoInfo()->kind == CommandKind::AddSample);
        d.accept(session({0.95, 0.90}));
        QVERIFY(d.undoInfo()->kind == CommandKind::Accept);
        QCOMPARE(d.undoInfo()->count, std::size_t{2});
        d.removeComponents({1, 2});
        QVERIFY(d.undoInfo()->kind == CommandKind::DeleteComponents);
        QCOMPARE(d.undoInfo()->count, std::size_t{2});
        d.undo();
        QVERIFY(d.redoInfo()->kind == CommandKind::DeleteComponents);
    }

    // S07：影像的有效位元數是相機的設定，換影像沿用（使用者 2026-10-06 決定）；不是 Command，不進復原堆疊。
    void theBitDepthSettingSurvivesAResetAndIsNotACommand() {
        InspectionDocument d = bareDocument();
        Counting n;
        n.attach(d);
        QCOMPARE(d.board().significantBits, 0);
        QVERIFY(d.setSignificantBits(12));
        QCOMPARE(d.board().significantBits, 12);
        QVERIFY(!d.canUndo());
        QVERIFY(!d.isDirty());
        QCOMPARE(n.resets, 0);
        QVERIFY(!d.setSignificantBits(7));   // 只能是 0 或 8..16
        QVERIFY(!d.setSignificantBits(17));
        QCOMPARE(d.board().significantBits, 12);
        d.reset("next.tif", 640, 480, 0.0);
        QCOMPARE(d.board().significantBits, 12);
        QVERIFY(d.setSignificantBits(0));
        QCOMPARE(d.board().significantBits, 0);
    }

    void resetClearsTheBoardTheHistoryAndTheCounters() {
        InspectionDocument d = freshDocument();
        Counting n;
        n.attach(d);
        d.accept(session({0.95}));
        d.reset("other.png", 640, 480, 1.5);
        QCOMPARE(n.resets, 1);
        QVERIFY(d.components().empty());
        QVERIFY(d.board().types.empty());
        QVERIFY(!d.canUndo());
        QVERIFY(!d.canRedo());
        QCOMPARE(d.board().name, std::string("other.png"));
        QCOMPARE(d.board().imageWidth, 640);
        QCOMPARE(d.board().umPerPixel, 1.5);
        QCOMPARE(d.lastTypeId(), std::string());
        QCOMPARE(d.addSample(Rect{80, 90, 40, 20}), 1);  // 編號從頭開始
        d.accept(session({0.95}));
        QCOMPARE(d.components()[0].name, std::string("X1"));
        QCOMPARE(d.components()[1].id, 2);
    }

    void dirtyFollowsTheHistoryPosition() {
        InspectionDocument d = freshDocument();
        QVERIFY(d.isDirty());  // 建立樣本就是修改
        d.markSaved();
        QVERIFY(!d.isDirty());
        d.accept(session({0.95}));
        QVERIFY(d.isDirty());
        d.markSaved();
        QVERIFY(!d.isDirty());
        d.removeComponents({1});
        QVERIFY(d.isDirty());
        d.undo();
        QVERIFY(!d.isDirty());
    }

    void aLargeDocumentKeepsItsIdsSortedThroughDeleteAndUndo() {
        InspectionDocument d = freshDocument();
        const ProposalSession big = ProposalSession(makeBigProposal(5000), Rect{0, 0, 10, 10}, RegionMode::Sample, 0.0);
        d.accept(big);
        const std::vector<ComponentInstance> before = d.components();
        QVERIFY(before.size() > 4000);
        std::vector<int> some;
        for (std::size_t i = 3; i < before.size(); i += 5) some.push_back(before[i].id);
        QCOMPARE(d.removeComponents(some), some.size());
        QVERIFY(std::is_sorted(d.components().begin(), d.components().end(),
                               [](const ComponentInstance& a, const ComponentInstance& b) { return a.id < b.id; }));
        d.undo();
        QVERIFY(sameComponents(d.components(), before));
    }
};

QTEST_APPLESS_MAIN(TstDocument)
#include "tst_document.moc"
