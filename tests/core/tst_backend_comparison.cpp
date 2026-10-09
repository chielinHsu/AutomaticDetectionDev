// A/B 比較的規則（S09，application/compare/BackendComparison.h）：兩份提案怎麼配對、怎麼分成兩邊都有／只有 A／只有 B，
// 以及「另一邊為什麼沒有」。手寫的小提案（位置與分數都知道），再加上「依序跑兩個 back end」的流程（假 back end）。
#include <QtTest>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

#include "CoreTestSupport.h"
#include "QtTestPrinters.h"
#include "application/compare/BackendComparison.h"

using namespace aoi;
using namespace aoi::test;

namespace {

Candidate seed(double x, double y, double score, double angle = 0.0) { return {{x, y, angle}, score, CandidateSource::Seed}; }
Candidate sample(double x, double y) { return {{x, y, 0.0}, 1.0, CandidateSource::Sample}; }
Candidate predicted(double x, double y) { return {{x, y, 0.0}, 0.0, CandidateSource::Predicted}; }

SampleProposal proposal(std::vector<Candidate> candidates, Size2D part = {40.0, 20.0}) {
    SampleProposal p;
    p.candidates = std::move(candidates);
    p.partSize = part;
    return p;
}

const ComparedItem* itemAt(const BackendComparison& c, double x, double y) {
    for (const ComparedItem& item : c.items) {
        if (std::abs(item.pose().x - x) < 1.0 && std::abs(item.pose().y - y) < 1.0) return &item;
    }
    return nullptr;
}

class RecordingProgress final : public IProgressSink {
public:
    void report(const ProgressEvent& e) noexcept override {
        maxFraction = std::max(maxFraction.load(), e.fraction);
        if (e.fraction >= 0.5) ++secondHalf;
        else ++firstHalf;
    }
    std::atomic<double> maxFraction{0.0};
    std::atomic<int> firstHalf{0};
    std::atomic<int> secondHalf{0};
};

}  // namespace

class TstBackendComparison : public QObject {
    Q_OBJECT

private slots:
    // 每一種情況各一個位置：兩邊都找到、只有 A（B 分數低於門檻、B 只是推算、B 附近什麼都沒有）、只有 B。
    void everyKindAndGapIsClassified() {
        const SampleProposal a = proposal({sample(100, 100), seed(200, 100, 0.95), seed(300, 100, 0.90), seed(400, 100, 0.88),
                                           seed(500, 100, 0.85), seed(600, 100, 0.40)});
        const SampleProposal b = proposal({sample(100.2, 100), seed(200.3, 100.1, 0.80, 0.4), seed(300, 100, 0.50), predicted(400, 100),
                                           seed(600, 100, 0.91), seed(700, 100, 0.93)});
        const BackendComparison c = compareProposals(a, 0.70, b, 0.70);

        const ComparedItem* both = itemAt(c, 200, 100);
        QVERIFY(both && both->kind == CompareKind::Both && both->gap == CompareGap::None);
        QVERIFY(approxEqual(both->distancePx, std::hypot(0.3, 0.1)));
        QVERIFY(approxEqual(both->angleDiffDeg, 0.4));
        QVERIFY(both->a.score == 0.95 && both->b.score == 0.80);

        const ComparedItem* belowThreshold = itemAt(c, 300, 100);
        QVERIFY(belowThreshold && belowThreshold->kind == CompareKind::OnlyA && belowThreshold->gap == CompareGap::BelowThreshold);
        QVERIFY(belowThreshold->b.present && belowThreshold->b.state == ObjectState::Rejected && belowThreshold->b.score == 0.50);

        const ComparedItem* inferredOnly = itemAt(c, 400, 100);
        QVERIFY(inferredOnly && inferredOnly->kind == CompareKind::OnlyA && inferredOnly->gap == CompareGap::InferredOnly);
        QVERIFY(inferredOnly->b.state == ObjectState::Inferred && !inferredOnly->b.score);

        const ComparedItem* notFound = itemAt(c, 500, 100);
        QVERIFY(notFound && notFound->kind == CompareKind::OnlyA && notFound->gap == CompareGap::NotFound && !notFound->b.present);

        const ComparedItem* onlyB = itemAt(c, 600, 100);  // A 有 0.40（低於門檻）、B 有 0.91
        QVERIFY(onlyB && onlyB->kind == CompareKind::OnlyB && onlyB->gap == CompareGap::BelowThreshold && onlyB->a.score == 0.40);
        const ComparedItem* onlyBNotFound = itemAt(c, 700, 100);
        QVERIFY(onlyBNotFound && onlyBNotFound->kind == CompareKind::OnlyB && onlyBNotFound->gap == CompareGap::NotFound);

        const CompareSummary& s = c.summary;
        QCOMPARE(s.both, std::size_t{2});  // 樣本與 (200, 100)
        QCOMPARE(s.onlyA, std::size_t{3});
        QCOMPARE(s.onlyABelowThreshold, std::size_t{1});
        QCOMPARE(s.onlyAInferredOnly, std::size_t{1});
        QCOMPARE(s.onlyANotFound, std::size_t{1});
        QCOMPARE(s.onlyB, std::size_t{2});
        QCOMPARE(s.onlyBBelowThreshold, std::size_t{1});
        QCOMPARE(s.onlyBNotFound, std::size_t{1});
        QCOMPARE(c.items.size(), s.both + s.onlyA + s.onlyB);
        QCOMPARE(s.a.found, std::size_t{5});
        QCOMPARE(s.a.rejected, std::size_t{1});
        QCOMPARE(s.b.found, std::size_t{4});
        QCOMPARE(s.b.rejected, std::size_t{1});
        QCOMPARE(s.b.inferred, std::size_t{1});
        QVERIFY(approxEqual(s.maxDistancePx, std::hypot(0.3, 0.1)));
        QVERIFY(approxEqual(s.maxAngleDiffDeg, 0.4));
    }

    // 兩邊的分數不是同一把尺：門檻各自套。B 的門檻降低，原本「只有 A」的那一個就變成兩邊都有（不必重新搜尋）。
    void eachSideHasItsOwnThreshold() {
        const SampleProposal a = proposal({sample(100, 100), seed(200, 100, 0.92)});
        const SampleProposal b = proposal({sample(100, 100), seed(200, 100, 0.62)});
        QCOMPARE(compareProposals(a, 0.70, b, 0.70).summary.onlyA, std::size_t{1});
        const BackendComparison lowered = compareProposals(a, 0.70, b, 0.60);
        QCOMPARE(lowered.summary.onlyA, std::size_t{0});
        QCOMPARE(lowered.summary.both, std::size_t{2});
    }

    // 兩邊都沒找到的位置（都低於門檻、都只是推算、一個低於門檻一個推算）不列出來。
    void placesNeitherSideFoundAreNotListed() {
        const SampleProposal a = proposal({seed(100, 100, 0.40), predicted(200, 100), seed(300, 100, 0.2)});
        const SampleProposal b = proposal({seed(100, 100, 0.50), predicted(200, 100), predicted(300, 100)});
        const BackendComparison c = compareProposals(a, 0.70, b, 0.70);
        QVERIFY(c.items.empty());
        QCOMPARE(c.summary.both + c.summary.onlyA + c.summary.onlyB, std::size_t{0});
        QCOMPARE(c.summary.a.inferred, std::size_t{1});
        QCOMPARE(c.summary.b.inferred, std::size_t{2});
    }

    // 「同一個元件」的距離依元件大小：短邊 × 0.25，至少 3 像素。相鄰的元件（格距 ≥ 1.5 倍短邊）不會被當成同一個。
    void theToleranceFollowsThePartSize() {
        QCOMPARE(compareProposals(proposal({}, {40, 20}), 0.7, proposal({}, {40, 20}), 0.7).tolerancePx, 5.0);
        QCOMPARE(compareProposals(proposal({}, {8, 8}), 0.7, proposal({}, {8, 8}), 0.7).tolerancePx, 3.0);
        // 位移 4.9 px：同一個（容差 5）；5.1 px：兩個不同的位置，各自「只有一邊」。
        const SampleProposal a = proposal({seed(100, 100, 0.9), seed(300, 100, 0.9)});
        const SampleProposal b = proposal({seed(104.9, 100, 0.9), seed(305.1, 100, 0.9)});
        const BackendComparison c = compareProposals(a, 0.7, b, 0.7);
        QCOMPARE(c.summary.both, std::size_t{1});
        QCOMPARE(c.summary.onlyA, std::size_t{1});
        QCOMPARE(c.summary.onlyB, std::size_t{1});
        QCOMPARE(c.summary.onlyANotFound, std::size_t{1});
    }

    // 一對一：B 的一個候選不能同時配給 A 的兩個；最近的先配。
    void pairingIsOneToOneNearestFirst() {
        const SampleProposal a = proposal({seed(100, 100, 0.9), seed(103, 100, 0.9)});
        const SampleProposal b = proposal({seed(102.5, 100, 0.9)});
        const BackendComparison c = compareProposals(a, 0.7, b, 0.7);
        QCOMPARE(c.summary.both, std::size_t{1});
        QCOMPARE(c.summary.onlyA, std::size_t{1});
        const ComparedItem* both = nullptr;
        for (const ComparedItem& item : c.items) {
            if (item.kind == CompareKind::Both) both = &item;
        }
        QVERIFY(both != nullptr);
        QVERIFY(approxEqual(both->a.pose.x, 103.0));  // 距離 0.5 的那一個，不是 2.5 的
    }

    // 差異在前（只有 A、再只有 B），然後兩邊都有；同一類依 y、再依 x。結果可重現。
    void itemsAreOrderedDifferencesFirst() {
        const SampleProposal a = proposal({seed(300, 200, 0.9), seed(100, 200, 0.9), seed(500, 100, 0.9), seed(700, 100, 0.9)});
        const SampleProposal b = proposal({seed(700, 100, 0.9), seed(900, 50, 0.9), seed(100, 200, 0.9)});
        const BackendComparison c = compareProposals(a, 0.7, b, 0.7);
        QCOMPARE(c.items.size(), std::size_t{5});
        QVERIFY(c.items[0].kind == CompareKind::OnlyA && c.items[0].pose().x == 500.0);  // y 100
        QVERIFY(c.items[1].kind == CompareKind::OnlyA && c.items[1].pose().x == 300.0);  // y 200
        QVERIFY(c.items[2].kind == CompareKind::OnlyB && c.items[2].pose().x == 900.0);
        QVERIFY(c.items[3].kind == CompareKind::Both && c.items[3].pose().x == 700.0);
        QVERIFY(c.items[4].kind == CompareKind::Both && c.items[4].pose().x == 100.0);
    }

    // 樣本有問題（搜尋前就知道）的一邊沒有候選：另一邊找到的全都是「只有它」。
    void aSideWithASampleProblemFindsNothing() {
        SampleProposal a = proposal({});
        a.sampleProblem = NoMatchInfo{NoMatchReason::LowContrast, 0.0, 0.0, 0};
        const SampleProposal b = proposal({sample(100, 100), seed(200, 100, 0.9)});
        const BackendComparison c = compareProposals(a, 0.7, b, 0.7);
        QVERIFY(c.summary.a.sampleProblem);
        QCOMPARE(c.summary.onlyB, std::size_t{2});
        QCOMPARE(c.summary.onlyBNotFound, std::size_t{2});
    }

    // 5 000 個對 5 000 個（U3 量級）：配對用格子，成本與點數成線性；這裡只確認結果（時間在 perf_ab）。
    void aLargeBoardPairsEveryPart() {
        std::vector<Candidate> ca;
        std::vector<Candidate> cb;
        for (int r = 0; r < 50; ++r) {
            for (int k = 0; k < 100; ++k) {
                ca.push_back(seed(20.0 + 60.0 * k, 20.0 + 30.0 * r, 0.9));
                cb.push_back(seed(20.3 + 60.0 * k, 19.8 + 30.0 * r, 0.9));
            }
        }
        const BackendComparison c = compareProposals(proposal(ca), 0.7, proposal(cb), 0.7);
        QCOMPARE(c.summary.both, std::size_t{5000});
        QVERIFY(approxEqual(c.summary.maxDistancePx, std::hypot(0.3, 0.2), 1e-6));
    }

    // 依序跑兩個 back end（同一個請求）：兩份提案都回來，進度 A 在前半、B 在後半。
    void runSideBySideRunsBothOnTheSameRequest() {
        const TestImage image(800, 600);
        fake::FakeSpec spec;
        spec.halfCols = 3;
        spec.halfRows = 2;
        fake::FakeTemplateBuilder builderA(spec);
        fake::FakeSearcher searcherA(spec);
        fake::FakeSpec other = spec;
        other.seed = 99;
        other.missingFraction = 0.3;
        fake::FakeTemplateBuilder builderB(other);
        fake::FakeSearcher searcherB(other);
        const GenerateObjectsFromSample useA(builderA, searcherA);
        const GenerateObjectsFromSample useB(builderB, searcherB);
        RecordingProgress progress;
        const Result<SideBySide> r = runSideBySide(useA, useB, makeRequest(image, {380, 285, 40, 30}), CancelToken(), &progress);
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        QVERIFY(!r.value().a.candidates.empty());
        QVERIFY(!r.value().b.candidates.empty());
        QVERIFY(progress.firstHalf.load() > 0 && progress.secondHalf.load() > 0);
        QVERIFY(progress.maxFraction.load() <= 1.0);
        const BackendComparison c = compareProposals(r.value().a, 0.7, r.value().b, 0.7);
        QVERIFY(c.summary.both > 0);
        QVERIFY(c.summary.onlyA > 0);  // B 的世界少了 30% 的元件
    }

    void runSideBySideStopsOnCancelOrFailure() {
        const TestImage image(800, 600);
        fake::FakeTemplateBuilder builder;
        fake::FakeSearcher searcher;
        const GenerateObjectsFromSample use(builder, searcher);
        CancelSource cancel;
        cancel.cancel();
        QCOMPARE_CODE(runSideBySide(use, use, makeRequest(image, {380, 285, 40, 30}), cancel.token()).error().code, ErrorCode::Cancelled);
        SampleRequest bad = makeRequest(image, {380, 285, 40, 30});
        bad.image = ImageView{};
        QVERIFY(!runSideBySide(use, use, bad, CancelToken()).ok());
    }
};

QTEST_APPLESS_MAIN(TstBackendComparison)
#include "tst_backend_comparison.moc"
