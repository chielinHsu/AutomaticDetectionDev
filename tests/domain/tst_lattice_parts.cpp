// 晶格模組的零件（S05）：正規方程累加器（O(1) 漸進重新擬合）與預測器。用精確數字驗證。
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <vector>

#include "LatticeTestSupport.h"
#include "domain/lattice/lattice_accumulator.h"
#include "domain/lattice/lattice_estimator.h"
#include "domain/lattice/lattice_predictor.h"

using namespace aoi;
using namespace aoi::test;

namespace {

const LatticeBasis kBasis{{100.0, 50.0}, {29.0, 7.0}, {-5.0, 22.0}, true};

bool near(Point2D a, Point2D b, double tol) { return distance(a, b) <= tol; }

// 由基底直接造一個模型（不經估計器）：給預測器的測試用精確的格子。
LatticeModel modelOf(const LatticeBasis& b, const std::vector<LatticeIndex>& cells) {
    LatticeModel m;
    m.basis = b;
    m.cells = cells;
    m.minI = m.maxI = cells.front().i;
    m.minJ = m.maxJ = cells.front().j;
    for (std::size_t k = 0; k < cells.size(); ++k) {
        m.inliers.push_back(k);
        m.minI = std::min(m.minI, cells[k].i);
        m.maxI = std::max(m.maxI, cells[k].i);
        m.minJ = std::min(m.minJ, cells[k].j);
        m.maxJ = std::max(m.maxJ, cells[k].j);
    }
    return m;
}

}  // namespace

class TstLatticeParts : public QObject {
    Q_OBJECT

private slots:
    // ---- LatticeAccumulator ----

    void exactPointsGiveTheExactBasis() {
        LatticeAccumulator acc({100.0, 50.0}, {0, 0});
        for (int i = -2; i <= 3; ++i) {
            for (int j = 0; j <= 2; ++j) acc.add({i, j}, kBasis.at(i, j));
        }
        QVERIFY(acc.twoDimensional());
        const auto b = acc.solve();
        QVERIFY(b.has_value());
        QVERIFY(b->hasV);
        QVERIFY(near(b->origin, kBasis.origin, 1e-9));
        QVERIFY(near(b->u, kBasis.u, 1e-9));
        QVERIFY(near(b->v, kBasis.v, 1e-9));
        QVERIFY(acc.residualSumOfSquares(*b) < 1e-12);
        QCOMPARE(acc.count(), std::size_t{18});
    }

    // 每加一個點就解一次（O(1)），結果要和「從頭把到目前為止的點全部加一遍再解」一樣。
    void incrementalSolvesEqualBatchSolves() {
        Rng rng(3);
        std::vector<std::pair<LatticeIndex, Point2D>> points;
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 5; ++j) {
                const Point2D p = kBasis.at(i, j);
                points.push_back({{i, j}, {p.x + rng.symmetric(0.7), p.y + rng.symmetric(0.7)}});
            }
        }
        LatticeAccumulator incremental(points.front().second, points.front().first);
        for (std::size_t k = 0; k < points.size(); ++k) {
            incremental.add(points[k].first, points[k].second);
            LatticeAccumulator batch(points.front().second, points.front().first);
            for (std::size_t q = 0; q <= k; ++q) batch.add(points[q].first, points[q].second);
            const auto a = incremental.solve();
            const auto b = batch.solve();
            QCOMPARE(a.has_value(), b.has_value());
            if (!a) continue;
            QCOMPARE(a->hasV, b->hasV);
            QVERIFY(near(a->origin, b->origin, 1e-9));
            QVERIFY(near(a->u, b->u, 1e-9));
            QVERIFY(near(a->v, b->v, 1e-9));
        }
    }

    // 殘差平方和從總和直接算（O(1)），要等於逐點算的。
    void theResidualSumOfSquaresFromTheSumsMatchesTheDirectSum() {
        Rng rng(5);
        LatticeAccumulator acc({100.0, 50.0}, {1, 1});
        std::vector<std::pair<LatticeIndex, Point2D>> points;
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 4; ++j) {
                const Point2D p = kBasis.at(i, j);
                points.push_back({{i, j}, {p.x + rng.symmetric(1.0), p.y + rng.symmetric(1.0)}});
                acc.add(points.back().first, points.back().second);
            }
        }
        const auto b = acc.solve();
        QVERIFY(b.has_value());
        double direct = 0.0;
        for (const auto& [cell, p] : points) direct += std::pow(distance(p, b->at(cell)), 2);
        QVERIFY(std::abs(acc.residualSumOfSquares(*b) - direct) < 1e-9 * std::max(1.0, direct));
        // 任意的基底（不是最小平方解）也一樣。
        const double other = acc.residualSumOfSquares(kBasis);
        double otherDirect = 0.0;
        for (const auto& [cell, p] : points) otherDirect += std::pow(distance(p, kBasis.at(cell)), 2);
        QVERIFY(std::abs(other - otherDirect) < 1e-9 * std::max(1.0, otherDirect));
        QVERIFY(acc.residualSumOfSquares(*b) <= other);  // 最小平方解的殘差最小
    }

    void removeUndoesAdd() {
        LatticeAccumulator acc({0.0, 0.0}, {0, 0});
        for (int i = 0; i < 5; ++i) acc.add({i, 0}, {10.0 * i, 3.0});
        const auto before = acc.solve();
        acc.add({7, 2}, {999.0, -42.0});  // 一個錯的點
        acc.remove({7, 2}, {999.0, -42.0});
        const auto after = acc.solve();
        QVERIFY(before.has_value() && after.has_value());
        QCOMPARE(after->hasV, before->hasV);
        QVERIFY(near(after->u, before->u, 1e-12));
        QVERIFY(near(after->origin, before->origin, 1e-12));
        QCOMPARE(acc.count(), std::size_t{5});
    }

    void oneRowSolvesAsOneDimensionalAndSameCellHasNoSolution() {
        LatticeAccumulator row;
        for (int i = 0; i < 4; ++i) row.add({i, 3}, {5.0 + 12.0 * i, 8.0});
        QVERIFY(!row.twoDimensional());
        const auto b = row.solve();
        QVERIFY(b.has_value());
        QVERIFY(!b->hasV);
        QVERIFY(near(b->u, {12.0, 0.0}, 1e-12));
        QVERIFY(near(b->at(2, 3), {29.0, 8.0}, 1e-12));  // 一維：j 不影響位置

        LatticeAccumulator same;
        same.add({1, 1}, {0.0, 0.0});
        same.add({1, 1}, {1.0, 0.0});
        QVERIFY2(!same.solve().has_value(), "all points in one cell: no direction");
        LatticeAccumulator column;  // i 都一樣、只有 j 在變：一維晶格要沿 i 編號
        column.add({0, 0}, {0.0, 0.0});
        column.add({0, 1}, {0.0, 10.0});
        QVERIFY(!column.solve().has_value());
        QVERIFY(!LatticeAccumulator().solve().has_value());
        LatticeAccumulator one;
        one.add({0, 0}, {3.0, 4.0});
        QVERIFY(!one.solve().has_value());
    }

    // 影像座標上萬像素時，總和相對於參考點，精度不會掉（不相對於參考點的話，平方的總和會吃掉有效位數）。
    void largeCoordinatesKeepTheirPrecision() {
        const LatticeBasis far{{1.0e5 + 0.25, 7.0e4 - 0.75}, {20.0, 0.5}, {-0.5, 20.0}, true};
        LatticeAccumulator acc(far.at(10, 10), {10, 10});
        for (int i = 0; i < 40; ++i) {
            for (int j = 0; j < 30; ++j) acc.add({i, j}, far.at(i, j));
        }
        const auto b = acc.solve();
        QVERIFY(b.has_value());
        QVERIFY(near(b->u, far.u, 1e-9));
        QVERIFY(near(b->v, far.v, 1e-9));
        QVERIFY(near(b->at(39, 29), far.at(39, 29), 1e-7));
        QVERIFY(acc.residualSumOfSquares(*b) < 1e-6);
    }

    // ---- LatticePredictor ----

    void holesAreTheUnoccupiedCellsInTheRangeInIndexOrder() {
        std::vector<LatticeIndex> cells;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 3; ++j) {
                if ((i == 1 && j == 1) || (i == 3 && j == 0)) continue;
                cells.push_back({i, j});
            }
        }
        const LatticeModel m = modelOf(kBasis, cells);
        const std::vector<PredictedCell> holes = LatticePredictor(m, {0, 0, 1000, 1000}).holes();
        QCOMPARE(holes.size(), std::size_t{2});
        QVERIFY(holes[0].cell == (LatticeIndex{1, 1}));
        QVERIFY(holes[1].cell == (LatticeIndex{3, 0}));
        QVERIFY(near(holes[0].position, kBasis.at(1, 1), 1e-12));
        QCOMPARE(LatticePredictor(m, {0, 0, 1000, 1000}).holes(1).size(), std::size_t{1});
        QCOMPARE(LatticePredictor(m, {0, 0, 1000, 1000}).cellsInRange().size(), std::size_t{12});
    }

    void boundsAreHalfOpenLikeRect() {
        const LatticeModel m = modelOf({{0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}, true}, {{0, 0}, {2, 2}});
        // 格點 (10, 10) 在 [0, 10) 之外：Rect 的右、下邊不含。
        const std::vector<PredictedCell> inside = LatticePredictor(m, {0.0, 0.0, 10.0, 10.0}).cellsInRange();
        QCOMPARE(inside.size(), std::size_t{1});
        QVERIFY(inside[0].cell == (LatticeIndex{0, 0}));
        QCOMPARE(LatticePredictor(m, {0.0, 0.0, 10.0001, 10.0001}).cellsInRange().size(), std::size_t{4});
    }

    void aMarginExtendsTheRange() {
        const LatticeModel m = modelOf(kBasis, {{0, 0}, {1, 0}, {0, 1}, {1, 1}});
        QCOMPARE(LatticePredictor(m, {-1e6, -1e6, 2e6, 2e6}).cellsInRange(1).size(), std::size_t{16});
        QCOMPARE(LatticePredictor(m, {-1e6, -1e6, 2e6, 2e6}).cellsInRange(2).size(), std::size_t{36});
    }

    void allInBoundsCoversTheWholeRectangleAndNothingOutside() {
        const LatticeModel m = modelOf({{50.0, 40.0}, {20.0, 0.0}, {0.0, 25.0}, true}, {{0, 0}, {1, 1}});
        const Rect bounds{0.0, 0.0, 200.0, 100.0};
        const std::vector<PredictedCell> all = LatticePredictor(m, bounds).allInBounds(10000);
        // x ＝ 50 ＋ 20i ∈ [0, 200) → i ∈ [−2, 7]（10 個）；y ＝ 40 ＋ 25j ∈ [0, 100) → j ∈ [−1, 2]（4 個）。
        QCOMPARE(all.size(), std::size_t{40});
        for (const PredictedCell& c : all) QVERIFY(bounds.contains(c.position));
        QCOMPARE(LatticePredictor(m, bounds).allInBounds(7).size(), std::size_t{7});

        const LatticeModel row = modelOf({{5.0, 50.0}, {10.0, 0.0}, {0.0, 0.0}, false}, {{0, 0}, {1, 0}});
        QCOMPARE(LatticePredictor(row, bounds).allInBounds(10000).size(), std::size_t{20});  // x ＝ 5, 15, …, 195
        QCOMPARE(LatticePredictor(row, {0.0, 60.0, 200.0, 10.0}).allInBounds(10000).size(), std::size_t{0});  // 不在線上
    }

    void nearToFarIsByDistanceThenIndex() {
        const LatticeModel m = modelOf({{0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}, true}, {{0, 0}, {4, 4}});
        std::vector<PredictedCell> cells = LatticePredictor(m, {-1, -1, 100, 100}).cellsInRange();
        LatticePredictor::orderNearToFar(cells, {20.0, 20.0});
        QCOMPARE(cells.size(), std::size_t{25});
        QVERIFY(cells[0].cell == (LatticeIndex{2, 2}));
        // 距離 10 的四個：(1,2)、(2,1)、(2,3)、(3,2)，同距離依 i、再依 j。
        QVERIFY(cells[1].cell == (LatticeIndex{1, 2}));
        QVERIFY(cells[2].cell == (LatticeIndex{2, 1}));
        QVERIFY(cells[3].cell == (LatticeIndex{2, 3}));
        QVERIFY(cells[4].cell == (LatticeIndex{3, 2}));
        for (std::size_t k = 1; k < cells.size(); ++k) {
            QVERIFY(distance(cells[k - 1].position, {20.0, 20.0}) <= distance(cells[k].position, {20.0, 20.0}) + 1e-12);
        }
        std::vector<PredictedCell> reversed(cells.rbegin(), cells.rend());
        LatticePredictor::orderNearToFar(reversed, {20.0, 20.0});
        for (std::size_t k = 0; k < cells.size(); ++k) QVERIFY(reversed[k].cell == cells[k].cell);  // 與輸入順序無關
    }

    void oneDimensionalHolesIgnoreJ() {
        const LatticeModel m = modelOf({{0.0, 0.0}, {10.0, 0.0}, {0.0, 0.0}, false}, {{0, 0}, {1, 0}, {3, 0}, {6, 0}});
        const std::vector<PredictedCell> holes = LatticePredictor(m, {-1, -1, 100, 100}).holes();
        QCOMPARE(holes.size(), std::size_t{3});  // 2、4、5
        QVERIFY(near(holes[0].position, {20.0, 0.0}, 1e-12));
        QVERIFY(near(holes[2].position, {50.0, 0.0}, 1e-12));
    }

    // 預測器與估計器接起來：一個缺件的陣列，預測的缺格就是缺的那幾格，位置是精確的。
    void anEstimatedModelPredictsTheMissingCells() {
        GridSpec g;
        g.cols = 9;
        g.rows = 4;
        g.angleDeg = 11.0;
        const std::vector<std::pair<int, int>> skip{{0, 0}, {4, 2}, {8, 3}};
        const auto model = LatticeEstimator().fit(makePoints(g, skip));
        QVERIFY(model.has_value());
        const std::vector<PredictedCell> holes = LatticePredictor(*model, {-1e4, -1e4, 2e4, 2e4}).holes();
        QCOMPARE(holes.size(), skip.size());
        for (const auto& [c, r] : skip) QVERIFY(hasPointNear(positions(holes), cellPosition(g, c, r), 1e-6));
    }
};

QTEST_APPLESS_MAIN(TstLatticeParts)
#include "tst_lattice_parts.moc"
