// LatticeEstimator（S05）：用精確數字驗證擬合、缺格、退化情況與多組晶格。
// 前 11 個測試是 S03 暫代版 SimpleGrid 的規格（tests/core/tst_grid.cpp 搬來改寫）：S05 取代它，行為要一樣或更好。
// 唯一改變的是 S03 寫成「已知限制」的那一條：間距差 20 倍時，S05 要找得到第二個方向。
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "LatticeTestSupport.h"
#include "domain/geometry.h"
#include "domain/lattice/lattice_confidence.h"
#include "domain/lattice/lattice_estimator.h"
#include "domain/lattice/lattice_predictor.h"

using namespace aoi;
using namespace aoi::test;

namespace {

const Rect kBig{0.0, 0.0, 2000.0, 2000.0};

std::vector<Point2D> holesOf(const LatticeModel& m, const Rect& bounds = kBig, std::size_t max = LatticePredictor::kUnlimited) {
    return positions(LatticePredictor(m, bounds).holes(max));
}

double len(Point2D v) { return std::hypot(v.x, v.y); }

// 每個內點都落在它的格點上（容差內），而且每格只有一個內點。
void checkInliersSitOnTheirCells(const LatticeModel& m, const std::vector<Point2D>& points) {
    QCOMPARE(m.inliers.size(), m.cells.size());
    for (std::size_t k = 0; k < m.inliers.size(); ++k) {
        QVERIFY(distance(points[m.inliers[k]], m.basis.at(m.cells[k])) <= m.tolerancePx + 1e-9);
        for (std::size_t q = k + 1; q < m.inliers.size(); ++q) QVERIFY(!(m.cells[k] == m.cells[q]));
    }
    QVERIFY(std::is_sorted(m.inliers.begin(), m.inliers.end()));
}

// 包一層分群策略，記下被呼叫幾次：證明估計器真的用注入的策略（O：換策略不改估計器）。
class CountingClustering final : public IVectorClustering {
public:
    VectorClusters cluster(std::span<const Point2D> vectors, double scale) const override {
        ++calls;
        return inner.cluster(vectors, scale);
    }
    mutable int calls = 0;
    GridVoteClustering inner;
};

}  // namespace

class TstLattice : public QObject {
    Q_OBJECT

private slots:
    // ---- S03 的規格（SimpleGrid → LatticeEstimator） ----

    void aCompleteGridHasNoHolesAndTheRightPitch() {
        const GridSpec g;
        const auto model = LatticeEstimator().fit(makePoints(g));
        QVERIFY(model.has_value());
        QVERIFY(model->hasV());
        QCOMPARE(model->inliers.size(), std::size_t{20});
        QVERIFY2(model->rmsPx < 1e-6, "an exact grid fits with no residual");
        std::vector<double> pitches{len(model->basis.u), len(model->basis.v)};
        std::sort(pitches.begin(), pitches.end());
        QVERIFY(std::abs(pitches[0] - 20.0) < 1e-6);
        QVERIFY(std::abs(pitches[1] - 30.0) < 1e-6);
        QVERIFY(holesOf(*model).empty());
        QCOMPARE(model->coverage, 1.0);
    }

    void missingInteriorCellsBecomeHolesAtTheExactPositions() {
        const GridSpec g;
        const std::vector<std::pair<int, int>> skip{{1, 1}, {2, 2}, {3, 1}};
        const auto model = LatticeEstimator().fit(makePoints(g, skip));
        QVERIFY(model.has_value());
        const std::vector<Point2D> holes = holesOf(*model);
        QCOMPARE(holes.size(), skip.size());
        for (const auto& [c, r] : skip) {
            QVERIFY2(hasPointNear(holes, cellPosition(g, c, r), 1e-6), "a skipped cell must be reported at its exact position");
        }
        QCOMPARE(model->coverage, 17.0 / 20.0);
    }

    // 只補「已找到的點的索引範圍之內」的缺格：邊上整列缺失（範圍縮小）不會往外推算。
    void holesOutsideTheFoundRangeAreNotPredicted() {
        const GridSpec g;
        std::vector<std::pair<int, int>> skip;
        for (int r = 0; r < g.rows; ++r) skip.push_back({4, r});  // 最後一欄整欄缺
        const auto model = LatticeEstimator().fit(makePoints(g, skip));
        QVERIFY(model.has_value());
        QVERIFY(holesOf(*model).empty());
    }

    void aRotatedNoisyGridStillFindsItsHoles() {
        GridSpec g;
        g.cols = 8;
        g.rows = 6;
        g.angleDeg = 15.0;
        const std::vector<std::pair<int, int>> skip{{3, 2}, {5, 4}};
        Rng rng(12345);
        std::vector<Point2D> points;
        for (int c = 0; c < g.cols; ++c) {
            for (int r = 0; r < g.rows; ++r) {
                if (std::find(skip.begin(), skip.end(), std::make_pair(c, r)) != skip.end()) continue;
                const Point2D p = cellPosition(g, c, r);
                points.push_back({p.x + rng.symmetric(0.5), p.y + rng.symmetric(0.5)});
            }
        }
        const auto model = LatticeEstimator().fit(points);
        QVERIFY(model.has_value());
        QVERIFY(model->rmsPx < 1.0);
        const std::vector<Point2D> holes = holesOf(*model);
        QCOMPARE(holes.size(), skip.size());
        for (const auto& [c, r] : skip) QVERIFY(hasPointNear(holes, cellPosition(g, c, r), 1.5));
        checkInliersSitOnTheirCells(*model, points);
    }

    void aSingleRowIsRecognizedAsOneDimensional() {
        GridSpec g;
        g.cols = 8;
        g.rows = 1;
        const auto model = LatticeEstimator().fit(makePoints(g, {{4, 0}}));
        QVERIFY(model.has_value());
        QVERIFY(!model->hasV());
        const std::vector<Point2D> holes = holesOf(*model);
        QCOMPARE(holes.size(), std::size_t{1});
        QVERIFY(hasPointNear(holes, cellPosition(g, 4, 0), 1e-6));
    }

    void anOutlierDoesNotChangeTheFit() {
        const GridSpec g;
        std::vector<Point2D> points = makePoints(g);
        points.push_back({1234.5, 777.7});  // 不在晶格上
        const auto model = LatticeEstimator().fit(points);
        QVERIFY(model.has_value());
        QCOMPARE(model->inliers.size(), std::size_t{20});  // 離群點沒有佔格子
        QVERIFY(std::find(model->inliers.begin(), model->inliers.end(), points.size() - 1) == model->inliers.end());
        QVERIFY(model->rmsPx < 1e-6);
    }

    void tooFewOrDegeneratePointsGiveNoModel() {
        const LatticeEstimator e;
        QVERIFY(!e.fit(std::vector<Point2D>{}).has_value());
        QVERIFY(!e.fit(std::vector<Point2D>{{1.0, 2.0}}).has_value());
        QVERIFY2(!e.fit(std::vector<Point2D>{{5.0, 5.0}, {5.0, 5.0}, {5.0, 5.0}}).has_value(), "overlapping points have no direction");
        QVERIFY(e.fitAll(std::vector<Point2D>{}).reason == NoLatticeReason::TooFewPoints);
        QVERIFY(e.fitAll(std::vector<Point2D>{{1.0, 2.0}}).reason == NoLatticeReason::TooFewPoints);
        QVERIFY(e.fitAll(std::vector<Point2D>{{5.0, 5.0}, {5.0, 5.0}, {5.0, 5.0}}).reason == NoLatticeReason::NoDirection);
    }

    void twoPointsMakeAOneDimensionalModelWithoutHoles() {
        const auto model = LatticeEstimator().fit(std::vector<Point2D>{{10.0, 10.0}, {40.0, 10.0}});
        QVERIFY(model.has_value());
        QVERIFY(!model->hasV());
        QVERIFY(holesOf(*model).empty());
        QVERIFY(std::abs(len(model->basis.u) - 30.0) < 1e-9);
        QVERIFY2(model->suggestCheck, "two points are little evidence: the user should check");
    }

    void holesOutsideTheBoundsAreDropped() {
        const GridSpec g;
        const auto model = LatticeEstimator().fit(makePoints(g, {{2, 1}}));
        QVERIFY(model.has_value());
        const Rect farAway{0.0, 0.0, 50.0, 50.0};  // 缺的格子在 (160, 120) 附近，不在這個範圍內
        QVERIFY(holesOf(*model, farAway).empty());
        QCOMPARE(holesOf(*model).size(), std::size_t{1});
    }

    void theHoleCountIsCapped() {
        GridSpec g;
        g.cols = 10;
        g.rows = 10;
        std::vector<std::pair<int, int>> skip;
        for (int c = 2; c < 8; ++c) skip.push_back({c, 5});  // 6 個缺格
        const auto model = LatticeEstimator().fit(makePoints(g, skip));
        QVERIFY(model.has_value());
        QCOMPARE(holesOf(*model, kBig, 4).size(), std::size_t{4});
        QCOMPARE(holesOf(*model, kBig, 100).size(), std::size_t{6});
    }

    // S03 的已知限制（只看最近 6 個鄰居，間距差 20 倍就當成一排）：S05 要找得到第二個方向。
    void veryDifferentPitchesAreFoundInBothDirections() {
        GridSpec g;
        g.cols = 10;
        g.rows = 3;
        g.pitchX = 10.0;
        g.pitchY = 200.0;  // 20 倍：每個點最近的鄰居都在同一列
        const std::vector<Point2D> points = makePoints(g, {{6, 1}});
        const auto model = LatticeEstimator().fit(points);
        QVERIFY(model.has_value());
        QVERIFY2(model->hasV(), "the second direction must be found even when it is 20 times longer");
        QVERIFY(std::abs(len(model->basis.u) - 10.0) < 1e-6);
        QVERIFY(std::abs(len(model->basis.v) - 200.0) < 1e-6);
        const std::vector<Point2D> holes = holesOf(*model);
        QCOMPARE(holes.size(), std::size_t{1});
        QVERIFY(hasPointNear(holes, cellPosition(g, 6, 1), 1e-6));
    }

    // ---- 基底的形式與索引 ----

    void theBasisIsReducedOrientedAndIndexedFromZero() {
        GridSpec g;
        g.cols = 7;
        g.rows = 5;
        g.pitchX = 25.0;
        g.pitchY = 40.0;
        g.angleDeg = -22.0;
        const std::vector<Point2D> points = makePoints(g);
        const auto model = LatticeEstimator().fit(points);
        QVERIFY(model.has_value());
        const LatticeBasis& b = model->basis;
        QVERIFY2(len(b.u) <= len(b.v) + 1e-9, "u is the shortest vector");
        QVERIFY2(std::abs(b.u.x * b.v.x + b.u.y * b.v.y) / len(b.u) <= 0.5 * len(b.u) + 1e-9, "v is reduced against u");
        QVERIFY2(b.u.x * b.v.y - b.u.y * b.v.x > 0.0, "cross(u, v) > 0");
        QVERIFY2(b.u.x > 0.0 || (b.u.x == 0.0 && b.u.y > 0.0), "u points into the right half plane");
        QCOMPARE(model->minI, 0);
        QCOMPARE(model->minJ, 0);
        QCOMPARE(model->maxI, 6);  // u ＝ 25 px 的方向有 7 格
        QCOMPARE(model->maxJ, 4);
        QVERIFY(distance(b.origin, b.at(0, 0)) < 1e-12);
        checkInliersSitOnTheirCells(*model, points);
    }

    void theEquationsInTheModelReproduceTheBasis() {
        const GridSpec g;
        const auto model = LatticeEstimator().fit(makePoints(g, {{1, 1}}));
        QVERIFY(model.has_value());
        QCOMPARE(model->equations.count(), model->inliers.size());
        const auto solved = model->equations.solve();
        QVERIFY(solved.has_value());
        QVERIFY(distance(solved->origin, model->basis.origin) < 1e-9);
        QVERIFY(distance(solved->u, model->basis.u) < 1e-9);
        QVERIFY(distance(solved->v, model->basis.v) < 1e-9);
    }

    // ---- 退化情況：不當機、結果合理 ----

    void degenerateInputsNeverCrash_data() {
        QTest::addColumn<QList<QPointF>>("points");
        QTest::addColumn<bool>("expectModel");
        QTest::newRow("two collinear points") << QList<QPointF>{{0, 0}, {10, 0}} << true;
        QTest::newRow("two vertical points") << QList<QPointF>{{3, 0}, {3, 25}} << true;
        QTest::newRow("all overlapping") << QList<QPointF>{{7, 7}, {7, 7}, {7, 7}, {7, 7}} << false;
        QTest::newRow("two piles of overlapping points") << QList<QPointF>{{0, 0}, {0, 0}, {0, 0}, {20, 0}, {20, 0}} << true;
        QTest::newRow("NaN and infinity are ignored")
            << QList<QPointF>{{0, 0}, {qQNaN(), 1}, {10, 0}, {qInf(), qInf()}, {20, 0}} << true;
        QTest::newRow("only NaN") << QList<QPointF>{{qQNaN(), qQNaN()}, {qQNaN(), 0}} << false;
        QTest::newRow("huge coordinates") << QList<QPointF>{{1e7, 1e7}, {1e7 + 30, 1e7}, {1e7 + 60, 1e7}} << true;
        QTest::newRow("tiny pitch") << QList<QPointF>{{0, 0}, {1e-4, 0}, {2e-4, 0}, {3e-4, 0}} << true;
        QTest::newRow("uneven collinear spacing") << QList<QPointF>{{0, 0}, {10, 0}, {25, 0}, {47, 0}} << true;
    }
    void degenerateInputsNeverCrash() {
        QFETCH(QList<QPointF>, points);
        QFETCH(bool, expectModel);
        std::vector<Point2D> p;
        for (const QPointF& q : points) p.push_back({q.x(), q.y()});
        const LatticeFit fit = LatticeEstimator().fitAll(p);
        QCOMPARE(!fit.models.empty(), expectModel);
        for (const LatticeModel& m : fit.models) {
            QVERIFY(m.inliers.size() >= 2);
            QVERIFY(std::isfinite(m.basis.u.x) && std::isfinite(m.basis.u.y));
            QVERIFY(m.confidence >= 0.0 && m.confidence <= 1.0);
            for (const std::size_t k : m.inliers) QVERIFY(std::isfinite(p[k].x) && std::isfinite(p[k].y));
        }
        if (!expectModel) QVERIFY(fit.reason != NoLatticeReason::None);
    }

    void aDuplicatePointOccupiesItsCellOnce() {
        const GridSpec g;
        std::vector<Point2D> points = makePoints(g);
        points.push_back({cellPosition(g, 2, 2).x + 0.3, cellPosition(g, 2, 2).y});  // 同一個元件被找到兩次
        const auto model = LatticeEstimator().fit(points);
        QVERIFY(model.has_value());
        QCOMPARE(model->inliers.size(), std::size_t{20});
        QCOMPARE(model->coverage, 1.0);
        checkInliersSitOnTheirCells(*model, points);
    }

    // 一排加上一個在它上方的離群點：不可以因此長出一整排假的缺格（二維要有至少兩個點在主列之外）。
    void aRowWithOneStrayPointAboveStaysOneDimensional() {
        GridSpec g;
        g.cols = 20;
        g.rows = 1;
        std::vector<Point2D> points = makePoints(g);
        points.push_back({cellPosition(g, 7, 0).x + 4.0, cellPosition(g, 7, 0).y + 35.0});
        const auto model = LatticeEstimator().fit(points);
        QVERIFY(model.has_value());
        QVERIFY(!model->hasV());
        QCOMPARE(model->inliers.size(), std::size_t{20});
        QVERIFY(holesOf(*model).empty());
    }

    // 第二排找到的不多：找到 3 個，二維成立、第二排其餘 9 格補上；只找到 2 個，證據不夠補 10 格（holesAreBacked），
    // 退回一排、不補。這條界線是刻意的：散佈的零件常有幾個點湊巧排得像晶格（randomPointsPredictAtMostAFlaggedHole）。
    void aSparseSecondRowNeedsEnoughEvidence_data() {
        QTest::addColumn<QList<int>>("found");
        QTest::addColumn<bool>("twoDimensional");
        QTest::addColumn<int>("holes");
        QTest::newRow("three found") << QList<int>{3, 6, 8} << true << 9;
        QTest::newRow("two found") << QList<int>{3, 8} << false << 0;
    }
    void aSparseSecondRowNeedsEnoughEvidence() {
        QFETCH(QList<int>, found);
        QFETCH(bool, twoDimensional);
        QFETCH(int, holes);
        GridSpec g;
        g.cols = 12;
        g.rows = 2;
        std::vector<std::pair<int, int>> skip;
        for (int c = 0; c < 12; ++c) {
            if (!found.contains(c)) skip.push_back({c, 1});
        }
        const auto model = LatticeEstimator().fit(makePoints(g, skip));
        QVERIFY(model.has_value());
        QCOMPARE(model->hasV(), twoDimensional);
        QCOMPARE(holesOf(*model).size(), static_cast<std::size_t>(holes));
        QCOMPARE(model->inliers.size(), twoDimensional ? std::size_t{12} + static_cast<std::size_t>(found.size()) : std::size_t{12});
        if (twoDimensional) {
            // 票數多的方向是 30（第一排），但 20（兩排之間）比較短：約化之後 u 是 20、v 是 30。
            QVERIFY2(std::abs(len(model->basis.u) - 20.0) < 1e-6 && std::abs(len(model->basis.v) - 30.0) < 1e-6, describe(*model).constData());
        }
    }

    // 隨機的點不是陣列（板上散佈、不成陣列的同型零件）：20 團、每團 20–100 個點，不可以補出一片假的缺格。
    // 3–4 個點湊巧等距共線，統計上和真的一小排分不開（spec.md「已知會失敗的情況」）：允許，但害處有上限——
    // 每組最多補 1 格（holesAreBacked：缺格 ＜ 內點 − 定義晶格最少要的點數），而且一定標成「建議檢查」。
    void randomPointsPredictAtMostAFlaggedHole_data() {
        QTest::addColumn<int>("seed");
        for (int seed = 1; seed <= 20; ++seed) QTest::addRow("cloud %d", seed) << seed;
    }
    void randomPointsPredictAtMostAFlaggedHole() {
        QFETCH(int, seed);
        Rng rng(static_cast<std::uint64_t>(seed) * 7919u);
        const int count = 20 + static_cast<int>(rng.below(81));
        std::vector<Point2D> points;
        for (int k = 0; k < count; ++k) points.push_back({rng.uniform() * 500.0, rng.uniform() * 400.0});
        const LatticeFit fit = LatticeEstimator().fitAll(points);
        for (const LatticeModel& m : fit.models) {
            const QByteArray why = "a lattice in random points: " + describe(m);
            QVERIFY2(holesOf(m).size() <= 1, why.constData());
            QVERIFY2(m.suggestCheck, why.constData());
            QVERIFY2(m.inliers.size() <= 5, why.constData());
        }
        if (fit.models.empty()) QVERIFY(fit.reason != NoLatticeReason::None);
    }

    // ---- 多組晶格 ----

    // 上下兩排方向不同（計畫書的例子）：兩組一維晶格，各自的點。
    void twoRowsWithDifferentDirectionsAreTwoLattices() {
        GridSpec top;
        top.cols = 12;
        top.rows = 1;
        top.pitchX = 30.0;
        top.origin = {100.0, 100.0};
        GridSpec bottom;
        bottom.cols = 9;
        bottom.rows = 1;
        bottom.pitchX = 25.0;
        bottom.angleDeg = 10.0;
        bottom.origin = {120.0, 400.0};
        std::vector<Point2D> points = makePoints(top);
        const std::vector<Point2D> second = makePoints(bottom);
        points.insert(points.end(), second.begin(), second.end());

        const LatticeFit fit = LatticeEstimator().fitAll(points);
        QCOMPARE(fit.models.size(), std::size_t{2});
        std::vector<double> pitches;
        std::size_t total = 0;
        for (const LatticeModel& m : fit.models) {
            QVERIFY(!m.hasV());
            QVERIFY(m.rmsPx < 1e-6);
            pitches.push_back(len(m.basis.u));
            total += m.inliers.size();
            checkInliersSitOnTheirCells(m, points);
        }
        QCOMPARE(total, points.size());
        std::sort(pitches.begin(), pitches.end());
        QVERIFY(std::abs(pitches[0] - 25.0) < 1e-6);
        QVERIFY(std::abs(pitches[1] - 30.0) < 1e-6);
        QCOMPARE(fit.models[0].inliers.size(), std::size_t{12});  // 點多的那一組是主晶格
    }

    // 斜的一排從另一排的「第二列」穿過：只有幾個點剛好落在列的線上、其中幾個碰巧在格點上。
    // 列一致性要看「歸到這一列的所有點」，不能只看正好在線上的（說明頁的情境在 S05 抓到這個漏洞：曾補出 9 個假缺格）。
    void aTiltedRowCrossingTheSecondRowIsNotASecondRow() {
        GridSpec top;
        top.cols = 12;
        top.rows = 1;
        top.pitchX = 34.0;
        top.origin = {130.0, 110.0};
        GridSpec bottom;
        bottom.cols = 9;
        bottom.rows = 1;
        bottom.pitchX = 30.0;
        bottom.angleDeg = -10.0;
        bottom.origin = {170.0, 330.0};
        std::vector<Point2D> points = makePoints(top);
        const std::vector<Point2D> second = makePoints(bottom);
        points.insert(points.end(), second.begin(), second.end());
        const LatticeFit fit = LatticeEstimator().fitAll(points);
        QCOMPARE(fit.models.size(), std::size_t{2});
        for (const LatticeModel& m : fit.models) {
            QVERIFY2(!m.hasV(), describe(m).constData());
            QVERIFY2(holesOf(m).empty(), describe(m).constData());
        }
    }

    // 兩個同樣間距的陣列中間隔了一大段：分成兩組，中間不補出一片假的缺格。
    void twoSeparateArraysWithTheSamePitchAreNotBridged() {
        GridSpec left;
        left.cols = 6;
        left.rows = 4;
        GridSpec right = left;
        right.origin = {left.origin.x + 16 * left.pitchX, left.origin.y};  // 中間空 10 欄
        std::vector<Point2D> points = makePoints(left, {{2, 1}});
        const std::vector<Point2D> second = makePoints(right, {{4, 2}});
        points.insert(points.end(), second.begin(), second.end());

        const LatticeFit fit = LatticeEstimator().fitAll(points);
        QCOMPARE(fit.models.size(), std::size_t{2});
        std::vector<Point2D> holes;
        for (const LatticeModel& m : fit.models) {
            QCOMPARE(m.inliers.size(), std::size_t{23});
            const std::vector<Point2D> h = holesOf(m);
            holes.insert(holes.end(), h.begin(), h.end());
        }
        QCOMPARE(holes.size(), std::size_t{2});
        QVERIFY(hasPointNear(holes, cellPosition(left, 2, 1), 1e-6));
        QVERIFY(hasPointNear(holes, cellPosition(right, 4, 2), 1e-6));
    }

    void maxLatticesLimitsTheNumberOfModels() {
        std::vector<Point2D> points;
        for (int k = 0; k < 3; ++k) {
            GridSpec row;
            row.cols = 8;
            row.rows = 1;
            row.pitchX = 20.0 + 7.0 * k;
            row.origin = {100.0, 100.0 + 150.0 * k};
            const std::vector<Point2D> p = makePoints(row);
            points.insert(points.end(), p.begin(), p.end());
        }
        LatticeOptions options;
        options.maxLattices = 2;
        QCOMPARE(LatticeEstimator(options).fitAll(points).models.size(), std::size_t{2});
        const LatticeFit all = LatticeEstimator().fitAll(points);
        QByteArray found;
        for (const LatticeModel& m : all.models) found += describe(m) + "; ";
        QVERIFY2(all.models.size() == 3, found.constData());
        QCOMPARE(LatticeEstimator().fit(points).has_value(), true);
        // 三排間距不同（20、27、34）：27 與 34 的點有一半碰巧落在 20 的格點容差內（27×3 ≈ 20×4），
        // 但不可以湊成一組二維晶格（會補出 20 幾個假的缺格）。每一排是一組一維晶格，沒有缺格。
        for (const LatticeModel& m : all.models) {
            QVERIFY2(!m.hasV(), describe(m).constData());
            QCOMPARE(m.inliers.size(), std::size_t{8});
            QVERIFY(holesOf(m).empty());
        }
    }

    // ---- 信心 ----

    void confidenceFollowsTheDocumentedFormula() {
        const LatticeQuality q = assessLattice(10, 0.9, 2.0, 0.8, 0.9, false);  // rms ÷ 容差 ＝ 0.45：精度在線性段的中間
        QVERIFY(std::abs(q.support - (1.0 - std::exp(-3.0))) < 1e-12);
        QVERIFY(std::abs(q.precision - 0.5) < 1e-12);
        QCOMPARE(q.coverage, 0.8);
        QCOMPARE(q.consistency, 0.9);
        QVERIFY(std::abs(q.confidence - q.support * q.precision * q.coverage * q.consistency) < 1e-15);
        QCOMPARE(assessLattice(10, 0.6, 2.0, 1.0, 1.0, false).precision, 1.0);  // 0.3 × 容差以內：正常雜訊，滿分
        QCOMPARE(assessLattice(5, 1.2, 2.0, 1.0, 1.0, false).precision, 0.0);   // 0.6 × 容差：像巧合，歸零
        QCOMPARE(assessLattice(5, 3.0, 2.0, 1.0, 1.0, false).precision, 0.0);
        QCOMPARE(assessLattice(0, 0.0, 1.0, 1.0, 1.0, false).confidence, 0.0);
        // 剛好決定一組晶格的點數（一維 2、二維 3）證據一樣少；二維多一個點才和一維 3 點一樣。
        const double minimal = 1.0 - std::exp(-1.0 / 3.0);
        QVERIFY(std::abs(assessLattice(2, 0.0, 1.0, 1.0, 1.0, false).support - minimal) < 1e-12);
        QVERIFY(std::abs(assessLattice(3, 0.0, 1.0, 1.0, 1.0, true).support - minimal) < 1e-12);
        QCOMPARE(assessLattice(4, 0.0, 1.0, 1.0, 1.0, true).support, assessLattice(3, 0.0, 1.0, 1.0, 1.0, false).support);
        QCOMPARE(assessLattice(2, 0.0, 1.0, 1.0, 1.0, true).support, 0.0);
    }

    // 陣列範圍內有不在格點上的點（例如雜訊大時找到「每隔一格」的粗晶格）：一致性低，信心跟著低。
    void pointsBetweenTheNodesLowerTheConsistency() {
        GridSpec g;
        g.cols = 10;
        g.rows = 6;
        const std::vector<Point2D> points = makePoints(g);
        const auto full = LatticeEstimator().fit(points);
        QVERIFY(full.has_value());
        QCOMPARE(full->consistency, 1.0);

        std::vector<Point2D> withStrays = points;  // 6 個點落在格子正中間（四個格點之間）
        for (int k = 0; k < 6; ++k) {
            const Point2D a = cellPosition(g, 1 + k, 2);
            withStrays.push_back({a.x + 0.5 * g.pitchX, a.y + 0.5 * g.pitchY});
        }
        const auto messy = LatticeEstimator().fit(withStrays);
        QVERIFY(messy.has_value());
        QCOMPARE(messy->inliers.size(), std::size_t{60});
        QVERIFY(std::abs(messy->consistency - 60.0 / 66.0) < 1e-12);
        QVERIFY(messy->confidence < full->confidence);
    }

    void aCleanLargeGridIsConfidentAndAThinOneSuggestsACheck() {
        GridSpec g;
        g.cols = 10;
        g.rows = 6;
        const auto clean = LatticeEstimator().fit(makePoints(g));
        QVERIFY(clean.has_value());
        QVERIFY(clean->confidence > 0.95);
        QVERIFY(!clean->suggestCheck);

        std::vector<std::pair<int, int>> skip;  // 一半缺件
        for (int c = 0; c < g.cols; ++c) {
            for (int r = 0; r < g.rows; ++r) {
                if ((c + 2 * r) % 2 == 1 && !(c == 9 || r == 5 || c == 0 || r == 0)) skip.push_back({c, r});
            }
        }
        const auto thin = LatticeEstimator().fit(makePoints(g, skip));
        QVERIFY(thin.has_value());
        QVERIFY(thin->confidence < clean->confidence);
        QVERIFY(thin->coverage < 0.8);
    }

    void aHighMinConfidenceTurnsALatticeIntoNoLattice() {
        LatticeOptions options;
        options.minConfidence = 0.99;
        const LatticeFit fit = LatticeEstimator(options).fitAll(std::vector<Point2D>{{0, 0}, {10, 0}, {20, 0}});
        QVERIFY(fit.models.empty());
        QVERIFY(fit.reason == NoLatticeReason::LowConfidence);
        QVERIFY(fit.rejectedConfidence > 0.0 && fit.rejectedConfidence < 0.99);
    }

    // ---- 可重現、與順序無關、策略可以替換 ----

    void resultsAreReproducibleAndIndependentOfInputOrder() {
        Scenario s;
        s.grid.cols = 14;
        s.grid.rows = 9;
        s.grid.angleDeg = 7.0;
        s.noisePx = 1.0;
        s.missingFraction = 0.1;
        s.outliers = 6;
        s.seed = 4242;
        const Observed o = generate(s);
        const LatticeEstimator e;
        const auto a = e.fit(o.points);
        const auto b = e.fit(o.points);
        QVERIFY(a.has_value() && b.has_value());
        QCOMPARE(a->inliers, b->inliers);
        QCOMPARE(a->basis.u.x, b->basis.u.x);  // 完全相同，不是「差不多」
        QCOMPARE(a->basis.origin.y, b->basis.origin.y);

        std::vector<std::size_t> perm(o.points.size());
        for (std::size_t k = 0; k < perm.size(); ++k) perm[k] = (k * 37 + 11) % perm.size();
        std::vector<Point2D> shuffled;
        for (const std::size_t k : perm) shuffled.push_back(o.points[k]);
        const auto c = e.fit(shuffled);
        QVERIFY(c.has_value());
        QCOMPARE(c->inliers.size(), a->inliers.size());
        std::vector<std::size_t> mapped;
        for (const std::size_t k : c->inliers) mapped.push_back(perm[k]);
        std::sort(mapped.begin(), mapped.end());
        QCOMPARE(mapped, a->inliers);
        QVERIFY(distance(c->basis.u, a->basis.u) < 1e-9);
        QVERIFY(distance(c->basis.v, a->basis.v) < 1e-9);
    }

    void theInjectedClusteringStrategyIsUsed() {
        auto counting = std::make_shared<CountingClustering>();
        const LatticeEstimator e(LatticeOptions{}, counting);
        QVERIFY(e.fit(makePoints(GridSpec{})).has_value());
        QVERIFY2(counting->calls >= 1, "the neighbour directions are clustered with the injected strategy");
        // 間距差 20 倍：v 不在最近鄰的方向裡，要沿 u 分列、再分群一次（findV）——也用注入的策略。
        GridSpec far;
        far.cols = 10;
        far.rows = 3;
        far.pitchX = 10.0;
        far.pitchY = 200.0;
        counting->calls = 0;
        const auto model = e.fit(makePoints(far));
        QVERIFY(model.has_value() && model->hasV());
        QVERIFY2(counting->calls >= 2, "one call for the directions and one for v between the rows");
    }

    // 兩個分群策略跑同一組情境（契約寫在 vector_clustering.h）：換策略，結果一樣對。
    void bothClusteringStrategiesFitTheSameScenarios_data() {
        QTest::addColumn<int>("strategy");
        QTest::addColumn<double>("angle");
        QTest::addColumn<double>("noise");
        for (const int strategy : {0, 1}) {
            for (const double angle : {0.0, 12.0, -35.0}) {
                for (const double noise : {0.0, 1.5}) {
                    QTest::addRow("%s angle=%g noise=%g", strategy == 0 ? "grid-vote" : "greedy", angle, noise) << strategy << angle << noise;
                }
            }
        }
    }
    void bothClusteringStrategiesFitTheSameScenarios() {
        QFETCH(int, strategy);
        QFETCH(double, angle);
        QFETCH(double, noise);
        Scenario s;
        s.grid.cols = 9;
        s.grid.rows = 6;
        s.grid.pitchX = 32.0;
        s.grid.pitchY = 24.0;
        s.grid.angleDeg = angle;
        s.noisePx = noise;
        s.missingFraction = 0.1;
        s.outliers = 3;
        s.seed = 99;
        const Observed o = generate(s);
        std::shared_ptr<const IVectorClustering> clustering;
        if (strategy == 0) clustering = std::make_shared<GridVoteClustering>();
        else clustering = std::make_shared<GreedyClustering>();
        const auto model = LatticeEstimator(LatticeOptions{}, clustering).fit(o.points);
        QVERIFY(model.has_value());
        const Agreement a = compare(*model, o, s.grid, 0.25 * 24.0);
        QCOMPARE(a.wrongCells, std::size_t{0});
        QCOMPARE(a.trueInliers, o.points.size() - 3);
        QCOMPARE(a.missedHoles, std::size_t{0});
        QCOMPARE(a.trueHoles, a.holes);
    }

    // 分群策略的契約（vector_clustering.h）：labels 與 members、center 一致；排序；非有限值標 −1。
    void clusteringStrategiesKeepTheirContract_data() {
        QTest::addColumn<int>("strategy");
        QTest::newRow("grid-vote") << 0;
        QTest::newRow("greedy") << 1;
    }
    void clusteringStrategiesKeepTheirContract() {
        QFETCH(int, strategy);
        Rng rng(5);
        std::vector<Point2D> v;
        for (int k = 0; k < 40; ++k) v.push_back({30.0 + rng.symmetric(0.8), rng.symmetric(0.8)});
        for (int k = 0; k < 25; ++k) v.push_back({rng.symmetric(0.8), 20.0 + rng.symmetric(0.8)});
        for (int k = 0; k < 5; ++k) v.push_back({rng.uniform() * 100.0, rng.uniform() * 100.0});
        v.push_back({qQNaN(), 1.0});
        const GridVoteClustering vote;
        const GreedyClustering greedy;
        const IVectorClustering& c = strategy == 0 ? static_cast<const IVectorClustering&>(vote) : greedy;
        const VectorClusters r = c.cluster(v, 25.0);
        QCOMPARE(r.labels.size(), v.size());
        QCOMPARE(r.labels.back(), -1);
        for (std::size_t k = 0; k < r.clusters.size(); ++k) {
            std::size_t n = 0;
            double sx = 0.0, sy = 0.0;
            for (std::size_t q = 0; q < v.size(); ++q) {
                if (r.labels[q] != static_cast<int>(k)) continue;
                ++n;
                sx += v[q].x;
                sy += v[q].y;
            }
            QCOMPARE(r.clusters[k].members, n);
            QVERIFY(std::abs(r.clusters[k].center.x - sx / static_cast<double>(n)) < 1e-9);
            QVERIFY(std::abs(r.clusters[k].center.y - sy / static_cast<double>(n)) < 1e-9);
            if (k > 0) QVERIFY(r.clusters[k - 1].members >= r.clusters[k].members);
        }
        QVERIFY(r.clusters.size() >= 2);
        QVERIFY(distance(r.clusters[0].center, {30.0, 0.0}) < 0.5);
        QVERIFY(distance(r.clusters[1].center, {0.0, 20.0}) < 0.5);
        QCOMPARE(c.cluster({}, 1.0).clusters.size(), std::size_t{0});
    }
};

QTEST_APPLESS_MAIN(TstLattice)
#include "tst_lattice.moc"
