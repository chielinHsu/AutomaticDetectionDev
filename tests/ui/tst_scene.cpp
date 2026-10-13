// SceneBuilder（工作階段 → 畫布的框與物件樹的列）。S04 的 ImageViewAdapter（QImage → ImageView）在 S06 移到
// infrastructure/image_qt，它的測試在 tests/infra/tst_image_qt。
#include <QtTest>

#include "ProposalTestSupport.h"
#include "presentation/presenters/SceneBuilder.h"

using namespace aoi;
using namespace aoi::ui;
using namespace aoi::test;

namespace {

SceneLabels labels() {
    SceneLabels l;
    l.sample = QStringLiteral("樣本");
    l.inferred = QStringLiteral("推算");
    l.manual = QStringLiteral("手動");
    l.board = QStringLiteral("板子 board.png");
    return l;
}

// 走到 Preview 的工作階段。
InspectionSession previewOf(const std::vector<double>& scores, std::size_t predicted = 0, double threshold = 0.70) {
    InspectionSession s;
    s.imageOpened("board.png", 1000, 800, 0.0);
    s.setThreshold(threshold);
    s.beginRegion(RegionMode::Sample);
    s.endRegion(Rect{20, 20, 60, 32}, RegionMode::Sample);
    s.searchSucceeded(makeProposal(scores, predicted));
    return s;
}

const ComponentView* byId(const Scene& scene, int id) {
    for (const ComponentView& c : scene.snapshot->components) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

}  // namespace

class TstScene : public QObject {
    Q_OBJECT
private slots:
    // ---- SceneBuilder：空的、文件、提案 ----
    void anEmptySessionGivesAnEmptyScene() {
        InspectionSession s;
        s.imageOpened("board.png", 1000, 800, 0.0);
        const Scene scene = buildScene(s, labels());
        QVERIFY(scene.snapshot->components.empty());
        QVERIFY(scene.rows->rows.empty());
        QVERIFY(!scene.rows->proposal);
        QCOMPARE(scene.counts.components, std::size_t{0});
        QCOMPARE(scene.rows->boardTitle, QStringLiteral("板子 board.png"));
    }

    void aProposalGetsOffsetIdsOneBodyBoxPerObjectAndItsStates() {
        InspectionSession s = previewOf({0.95, 0.75, 0.50}, 1);  // Sample、Found、Found、Rejected、Inferred
        const Scene scene = buildScene(s, labels());
        QCOMPARE(scene.snapshot->components.size(), std::size_t{5});
        QVERIFY(scene.rows->proposal);
        QCOMPARE(scene.snapshot->components.front().id, kProposalIdBase + 1);
        QVERIFY(isProposalId(scene.snapshot->components.front().id));
        QVERIFY(byId(scene, kProposalIdBase + 1)->state == OverlayState::Sample);
        QVERIFY(byId(scene, kProposalIdBase + 2)->state == OverlayState::Found);
        QVERIFY(byId(scene, kProposalIdBase + 4)->state == OverlayState::Rejected);
        QVERIFY(byId(scene, kProposalIdBase + 5)->state == OverlayState::Inferred);
        for (const ComponentView& c : scene.snapshot->components) {
            QCOMPARE(c.boxes.size(), std::size_t{1});
            QVERIFY(c.boxes[0].kind == BoxKind::Body);
            QCOMPARE(c.boxes[0].rect, c.outline);
        }
        // 候選 0.95 在 (150, 100)，範本 40 × 20：外框 (130, 90, 40, 20)
        QCOMPARE(byId(scene, kProposalIdBase + 2)->outline, QRectF(130, 90, 40, 20));
    }

    void proposalLabelsShowTheScoreOrWhatTheObjectIs() {
        InspectionSession s = previewOf({0.95, 0.50}, 1);
        QCOMPARE(s.addManual({400, 300, 0}), InspectionSession::AddResult::Added);
        const Scene scene = buildScene(s, labels());
        QCOMPARE(byId(scene, kProposalIdBase + 1)->label(), QStringLiteral("樣本"));
        QCOMPARE(byId(scene, kProposalIdBase + 2)->label(), QStringLiteral("0.95"));
        QCOMPARE(byId(scene, kProposalIdBase + 3)->label(), QStringLiteral("0.50"));  // 拒絕的也標示分數
        QCOMPARE(byId(scene, kProposalIdBase + 4)->label(), QStringLiteral("推算"));
        QCOMPARE(byId(scene, kProposalIdBase + 5)->label(), QStringLiteral("手動"));
    }

    void proposalRowsAreNamedByIdAndCarryTheScore() {
        InspectionSession s = previewOf({0.95}, 1);
        const Scene scene = buildScene(s, labels());
        QCOMPARE(scene.rows->rows.size(), std::size_t{3});
        QVERIFY(scene.rows->rows[1].name.isEmpty());  // 提案物件沒有名稱；顯示時才組「#編號」（S11）
        QCOMPARE(displayName(scene.rows->rows[1]), QStringLiteral("#2"));
        QCOMPARE(scene.rows->rows[1].score, 0.95);
        QCOMPARE(scene.rows->rows[2].score, -1.0);  // 推算的沒有分數
        QVERIFY(scene.rows->rows[1].id >= kProposalIdBase);
        QCOMPARE(scene.rows->types.size(), std::size_t{1});
        QCOMPARE(scene.rows->types[0].groups.size(), std::size_t{1});
        QCOMPARE(scene.rows->types[0].groups[0].boxCount, 1);
    }

    void countsFollowTheThresholdAndTheSampleCountsAsFound() {
        InspectionSession s = previewOf({0.95, 0.75, 0.50}, 1, 0.70);
        Scene scene = buildScene(s, labels());
        QCOMPARE(scene.counts.found, std::size_t{3});     // Sample ＋ 0.95 ＋ 0.75
        QCOMPARE(scene.counts.rejected, std::size_t{1});
        QCOMPARE(scene.counts.inferred, std::size_t{1});
        QCOMPARE(scene.counts.boxes, std::size_t{5});
        QVERIFY(scene.counts.fromProposal);
        QCOMPARE(scene.rows->stateCounts[static_cast<std::size_t>(stateIndex(OverlayState::Found))], 2);
        s.setThreshold(0.90);
        scene = buildScene(s, labels());
        QCOMPARE(scene.counts.found, std::size_t{2});     // Sample ＋ 0.95
        QCOMPARE(scene.counts.rejected, std::size_t{2});
    }

    void lowConfidenceCountsInferredAndFoundCloseToTheThreshold() {
        // 門檻 0.70，邊界 0.10：0.75 與 0.78 在 0.80 之內（算偏低）；0.85 與 0.95 不是；推算的一律算。
        InspectionSession s = previewOf({0.95, 0.85, 0.75, 0.78}, 1, 0.70);
        QCOMPARE(buildScene(s, labels()).counts.lowConfidence, std::size_t{3});
        // 手動補的沒有分數，不算。
        s.addManual({400, 300, 0});
        QCOMPARE(buildScene(s, labels()).counts.lowConfidence, std::size_t{3});
    }

    void editsInThePreviewShowUpInTheScene() {
        InspectionSession s = previewOf({0.95, 0.75});
        s.select({2});
        s.deleteSelection();
        s.select({3});
        s.moveSelection({10, 5});
        const Scene scene = buildScene(s, labels());
        QVERIFY(byId(scene, kProposalIdBase + 2) == nullptr);
        QCOMPARE(byId(scene, kProposalIdBase + 3)->outline.topLeft(), QPointF(200 - 20 + 10, 100 - 10 + 5));
    }

    // S11：文件那一部分的快取——文件沒變就共用上一次的結果，變了（Command、復原、換語言）就重算。
    void theDocumentPartIsReusedUntilTheDocumentOrTheLabelsChange() {
        InspectionSession s = previewOf({0.95, 0.75}, 1);
        s.accept();
        DocumentSceneCache cache;
        const Scene first = buildScene(s, labels(), &cache);
        QVERIFY(cache.valid);
        QCOMPARE(cache.revision, s.document().revision());
        QCOMPARE(first.snapshot->components.size(), std::size_t{4});

        const Scene again = buildScene(s, labels(), &cache);
        QVERIFY(again.rows == first.rows);          // 同一份：沒有重建
        QVERIFY(again.snapshot == first.snapshot);  // 沒有提案時連快照都共用
        QCOMPARE(again.counts.components, std::size_t{4});

        // 文件變了：刪掉一個 → 重算，結果反映新的文件
        s.select({2});
        QCOMPARE(s.deleteSelection(), std::size_t{1});
        const Scene afterDelete = buildScene(s, labels(), &cache);
        QVERIFY(afterDelete.rows != first.rows);
        QCOMPARE(afterDelete.snapshot->components.size(), std::size_t{3});
        QVERIFY(byId(afterDelete, 2) == nullptr);
        QCOMPARE(cache.revision, s.document().revision());

        // 復原：又是一次變更 → 重算，四個都回來
        QVERIFY(s.undo());
        const Scene afterUndo = buildScene(s, labels(), &cache);
        QCOMPARE(afterUndo.snapshot->components.size(), std::size_t{4});
        QVERIFY(byId(afterUndo, 2) != nullptr);

        // 標籤變了（換語言）：板子的標題跟著變
        SceneLabels english = labels();
        english.board = QStringLiteral("Board board.png");
        const Scene translated = buildScene(s, english, &cache);
        QCOMPARE(translated.rows->boardTitle, QStringLiteral("Board board.png"));
        QVERIFY(translated.rows != afterUndo.rows);

        // 有提案時：文件的元件在前（共用快取）、提案的在後；快照是新的一份
        s.beginRegion(RegionMode::Sample);
        s.endRegion(Rect{300, 300, 60, 32}, RegionMode::Sample);
        s.searchSucceeded(makeProposal({0.9}, 0));
        const Scene withProposal = buildScene(s, english, &cache);
        QVERIFY(withProposal.snapshot != cache.snapshot);
        QCOMPARE(withProposal.snapshot->components.size(), std::size_t{4 + 2});
        QVERIFY(!isProposalId(withProposal.snapshot->components[3].id));
        QVERIFY(isProposalId(withProposal.snapshot->components[4].id));
        QVERIFY(withProposal.rows->proposal);
        QCOMPARE(cache.revision, s.document().revision());

        // 沒有快取的呼叫和有快取的結果一樣
        const Scene plain = buildScene(s, english);
        QCOMPARE(plain.snapshot->components.size(), withProposal.snapshot->components.size());
        for (std::size_t i = 0; i < plain.snapshot->components.size(); ++i) {
            QCOMPARE(plain.snapshot->components[i].id, withProposal.snapshot->components[i].id);
            QCOMPARE(plain.snapshot->components[i].outline, withProposal.snapshot->components[i].outline);
            QCOMPARE(plain.snapshot->components[i].label(), withProposal.snapshot->components[i].label());
        }
    }

    void boardComponentsKeepTheirIdsAndNamesAndAnnounceTheirType() {
        InspectionSession s = previewOf({0.95, 0.75}, 1);
        s.accept();
        const Scene scene = buildScene(s, labels());
        QVERIFY(!scene.rows->proposal);
        QCOMPARE(scene.rows->rows.size(), std::size_t{4});
        QCOMPARE(scene.rows->rows[0].id, 1);
        QCOMPARE(scene.rows->rows[0].name, QStringLiteral("X1"));
        QCOMPARE(scene.snapshot->components[1].label(), QStringLiteral("X2 0.95"));
        QCOMPARE(scene.snapshot->components[3].label(), QStringLiteral("X4"));  // 推算的沒有分數
        for (const ComponentView& c : scene.snapshot->components) QVERIFY(!isProposalId(c.id));
        QCOMPARE(scene.rows->types.size(), std::size_t{1});
        QCOMPARE(scene.rows->rows[0].typeIndex, 0);
        QCOMPARE(scene.counts.components, std::size_t{4});
    }

    void aSecondProposalIsDrawnOnTopOfTheBoardButTheTreeListsTheProposal() {
        InspectionSession s = previewOf({0.95});
        s.accept();
        s.beginRegion(RegionMode::Sample);
        s.endRegion(Rect{300, 300, 60, 32}, RegionMode::Sample);
        s.searchSucceeded(makeProposal({0.90, 0.85}));
        const Scene scene = buildScene(s, labels());
        QCOMPARE(scene.snapshot->components.size(), std::size_t{2 + 3});  // 板子 2 ＋ 提案 3
        QVERIFY(!isProposalId(scene.snapshot->components.front().id));
        QVERIFY(isProposalId(scene.snapshot->components.back().id));
        QCOMPARE(scene.rows->rows.size(), std::size_t{3});                 // 樹列的是提案
        QVERIFY(scene.rows->proposal);
        QVERIFY(scene.counts.fromProposal);
    }

    void manualBoardComponentsAreLabelledAsManual() {
        InspectionSession s = previewOf({0.95});
        s.accept();
        s.addManual({500, 400, 0});
        const Scene scene = buildScene(s, labels());
        QVERIFY(scene.snapshot->components.back().label().contains(QStringLiteral("手動")));
        QVERIFY(scene.rows->rows.back().manual);
    }

    void aBigProposalBuildsAllTheRowsAndBoxes() {
        InspectionSession s;
        s.imageOpened("board.png", 2592, 1944, 0.0);
        s.beginRegion(RegionMode::Sample);
        s.endRegion(Rect{0, 0, 60, 40}, RegionMode::Sample);
        s.searchSucceeded(makeBigProposal(5000));
        const Scene scene = buildScene(s, labels());
        QCOMPARE(scene.snapshot->components.size(), std::size_t{5001});
        QCOMPARE(scene.rows->rows.size(), std::size_t{5001});
        QCOMPARE(scene.counts.boxes, std::size_t{5001});
        int total = 0;
        for (int n : scene.rows->stateCounts) total += n;
        QCOMPARE(total, 5001);
    }
};

QTEST_MAIN(TstScene)
#include "tst_scene.moc"
