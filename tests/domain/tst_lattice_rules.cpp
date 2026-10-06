// 晶格估計的內部規則（S05）：每一條規則單獨測。
//
// 為什麼要單獨測：估計器的規則彼此備援（例如「一排加一個離群點」同時被「找 v 時要指向 2 個以上不同的點」與
// 「第二列至少 2 個內點」擋下）。只從 LatticeEstimator 的結果看，拿掉其中一條，另一條仍然擋下，結果不變——
// 第一輪變異測試因此有四個缺陷存活（M04、M05、M06、M32，見 docs/steps/S05-lattice/perf.md）。這裡直接呼叫內部的函式，
// 讓每一條規則自己被釘住。
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <vector>

#include "LatticeTestSupport.h"
#include "domain/lattice/basis_finder.h"
#include "domain/lattice/cell_components.h"
#include "domain/lattice/lattice_acceptance.h"
#include "domain/lattice/lattice_detail.h"
#include "domain/lattice/lattice_estimator.h"

using namespace aoi;
using namespace aoi::test;

namespace {

// 和估計器的 assignAll 一樣：每個點指派到最近的格子，殘差 ≤ 容差才是成員（測試裡的點不會落在同一格）。
detail::LatticeCandidate candidateOf(const LatticeBasis& basis, double tolerance, const std::vector<Point2D>& points) {
    detail::LatticeCandidate c;
    c.basis = basis;
    c.tolerance = tolerance;
    for (std::size_t k = 0; k < points.size(); ++k) {
        const LatticeIndex cell = basis.nearestCell(points[k]);
        if (distance(points[k], basis.at(cell)) <= tolerance) {
            c.members.push_back(k);
            c.cells.push_back(cell);
        }
    }
    return c;
}

std::vector<Point2D> row(int count, double pitch, Point2D origin) {
    std::vector<Point2D> p;
    for (int k = 0; k < count; ++k) p.push_back({origin.x + k * pitch, origin.y});
    return p;
}

LatticeModel modelWith(bool twoDimensional, std::size_t inliers, int width, int height) {
    LatticeModel m;
    m.basis = {{0.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}, twoDimensional};
    m.maxI = width - 1;
    m.maxJ = twoDimensional ? height - 1 : 0;
    for (std::size_t k = 0; k < inliers; ++k) m.inliers.push_back(k);
    return m;
}

const LatticeOptions kOptions;

}  // namespace

class TstLatticeRules : public QObject {
    Q_OBJECT

private slots:
    // ---- 找 v（BasisFinder） ----

    // 一排加上方一個離群點：一整排點都指向同一個離群點，取餘後的向量完全相同；但它們只指向 1 個不同的點，不算 v。
    void aSingleStrayPointAboveARowIsNotASecondDirection() {
        std::vector<Point2D> points = row(20, 30.0, {100.0, 200.0});
        points.push_back({100.0 + 7 * 30.0 + 4.0, 200.0 + 45.0});
        const GridVoteClustering clustering;
        const detail::BasisFinder finder(kOptions, clustering, points, 1e-6);
        const std::vector<Point2D> directions = finder.directions();
        QVERIFY(!directions.empty());
        QVERIFY(std::abs(std::hypot(directions[0].x, directions[0].y) - 30.0) < 1e-6);
        QVERIFY2(!finder.secondDirection(directions[0], directions).has_value(), "one target is not a second row");
    }

    // 兩排各有很多點：v 由列與列之間找到（指向很多不同的點）。
    void twoFullRowsGiveTheSecondDirectionBetweenTheRows() {
        std::vector<Point2D> points = row(20, 10.0, {100.0, 100.0});
        const std::vector<Point2D> second = row(20, 10.0, {100.0, 300.0});
        points.insert(points.end(), second.begin(), second.end());
        const GridVoteClustering clustering;
        const detail::BasisFinder finder(kOptions, clustering, points, 1e-6);
        const std::vector<Point2D> directions = finder.directions();
        const std::optional<Point2D> v = finder.secondDirection(directions[0], directions);
        QVERIFY(v.has_value());
        QVERIFY(distance(*v, {0.0, 200.0}) < 1e-6);
    }

    // 方向裡「和 u 列距不到 0.8·|u|」的不能當 v（u 是最短的格距時，真的列距 ≥ 0.87·|u|）。只有一列的點：列與列之間也沒有 v。
    void aDirectionTooCloseToUIsNotV() {
        const std::vector<Point2D> points = row(12, 30.0, {50.0, 50.0});
        const GridVoteClustering clustering;
        const detail::BasisFinder finder(kOptions, clustering, points, 1e-6);
        const std::vector<Point2D> directions{{30.0, 0.0}, {60.0, 12.0}};  // 列距 12 ＝ 0.4·|u|
        QVERIFY(!finder.secondDirection({30.0, 0.0}, directions).has_value());
        const std::vector<Point2D> good{{30.0, 0.0}, {12.0, 27.0}};  // 列距 27 ＝ 0.9·|u|
        const std::optional<Point2D> v = finder.secondDirection({30.0, 0.0}, good);
        QVERIFY(v.has_value());
        QVERIFY(distance(*v, {12.0, 27.0}) < 1e-12);
    }

    // ---- 二維成立（twoDimensionalIsValid） ----

    void aRealTwoDimensionalGridIsValid() {
        GridSpec g;
        g.cols = 10;
        g.rows = 4;
        g.pitchX = 20.0;
        g.pitchY = 30.0;
        const LatticeBasis basis{g.origin, {20.0, 0.0}, {0.0, 30.0}, true};
        QVERIFY(detail::twoDimensionalIsValid(candidateOf(basis, 5.0, makePoints(g)), makePoints(g), kOptions));
    }

    // 主列以外只有 1 個內點：不成立（即使那一列沒有可疑之處）。
    void oneInlierOutsideTheMainRowIsNotASecondRow() {
        std::vector<Point2D> points = row(10, 30.0, {0.0, 0.0});
        points.push_back({120.0, 40.0});  // 落在 (4, 1)
        const LatticeBasis basis{{0.0, 0.0}, {30.0, 0.0}, {0.0, 40.0}, true};
        const detail::LatticeCandidate c = candidateOf(basis, 7.5, points);
        QCOMPARE(c.members.size(), std::size_t{11});
        QVERIFY(!detail::twoDimensionalIsValid(c, points, kOptions));
        points.push_back({210.0, 40.0});  // 第二個：(7, 1)
        QVERIFY(detail::twoDimensionalIsValid(candidateOf(basis, 7.5, points), points, kOptions));
    }

    // 第二列是一排間距不同的元件：約一半碰巧落在格點上，而且相鄰的間隔是 34 ＝ 1.7 格。可疑的列：不成立。
    void aRowWithItsOwnPitchIsSuspect() {
        std::vector<Point2D> points = row(10, 20.0, {0.0, 0.0});
        const std::vector<Point2D> other = row(10, 34.0, {0.0, 30.0});
        points.insert(points.end(), other.begin(), other.end());
        const LatticeBasis basis{{0.0, 0.0}, {20.0, 0.0}, {0.0, 30.0}, true};
        const detail::LatticeCandidate c = candidateOf(basis, 5.0, points);
        const std::size_t secondRow = c.members.size() - 10;
        QVERIFY2(secondRow >= 2, "the fixture must put some coincidences on the nodes of the second row");
        QVERIFY(!detail::twoDimensionalIsValid(c, points, kOptions));
    }

    // 只看「間隔」也要擋得住：第二列的點全都碰巧在格點上（比例 100%），但間隔不是整數格（點與點之間夾著不在格點上的點）。
    void irregularGapsAloneMakeARowSuspect() {
        std::vector<Point2D> points = row(10, 20.0, {0.0, 0.0});
        // 第二列：格點 0、3、6、9 上各一個，中間各夾一個不在格點上的點（列線上、x ＝ 30、90、150）。
        for (const double x : {0.0, 30.0, 60.0, 90.0, 120.0, 150.0, 180.0}) points.push_back({x, 30.0});
        const LatticeBasis basis{{0.0, 0.0}, {20.0, 0.0}, {0.0, 30.0}, true};
        LatticeOptions lenient;
        lenient.minRowConsistency = 0.0;  // 關掉「比例」那一條，只留「間隔」
        QVERIFY(!detail::twoDimensionalIsValid(candidateOf(basis, 5.0, points), points, lenient));
    }

    // ---- 證據、缺格 ----

    void evidenceNeedsThreeInliersOrEveryPoint() {
        const LatticeBasis basis{{0.0, 0.0}, {10.0, 0.0}, {0.0, 0.0}, false};
        const std::vector<Point2D> two{{0.0, 0.0}, {10.0, 0.0}};
        QVERIFY2(detail::enoughEvidence(candidateOf(basis, 2.5, two), two, kOptions), "two points explain everything");
        std::vector<Point2D> withStrays = two;
        withStrays.push_back({4.0, 30.0});
        withStrays.push_back({55.0, -20.0});
        QVERIFY(!detail::enoughEvidence(candidateOf(basis, 2.5, withStrays), withStrays, kOptions));
        withStrays.push_back({20.0, 0.0});
        QVERIFY(detail::enoughEvidence(candidateOf(basis, 2.5, withStrays), withStrays, kOptions));
    }

    void holesNeedEvidenceAtTheExactBoundary() {
        QVERIFY(detail::holesAreBacked(modelWith(false, 2, 2, 1)));    // 沒有缺格：不受限
        QVERIFY(!detail::holesAreBacked(modelWith(false, 4, 5, 1)));   // 一維：1 個缺格要 ≥ 5 個內點
        QVERIFY(detail::holesAreBacked(modelWith(false, 5, 6, 1)));
        QVERIFY(!detail::holesAreBacked(modelWith(true, 14, 12, 2)));  // 二維：10 個缺格要 ≥ 15 個內點
        QVERIFY(detail::holesAreBacked(modelWith(true, 15, 12, 2)));
    }

    // ---- 連通（largestComponent） ----

    void cellsLinkAcrossAtMostTwoEmptyCells() {
        // 0..3 一塊；6..8 一塊（3 與 6 之間空 2 格：仍相連）；12 單獨（8 與 12 之間空 3 格：分開）。
        std::vector<LatticeIndex> cells;
        for (const int i : {0, 1, 2, 3, 6, 7, 8, 12}) cells.push_back({i, 0});
        const std::vector<std::size_t> keep = detail::largestComponent(cells, 3, false);
        QCOMPARE(keep.size(), std::size_t{7});
        QVERIFY(std::find(keep.begin(), keep.end(), std::size_t{7}) == keep.end());
    }

    // 往右上、右下、左上、左下的斜鄰居都要看到（逐列掃描時，上一列的右邊也是鄰居）。
    void diagonalNeighboursInEveryDirectionLink() {
        for (const LatticeIndex other : {LatticeIndex{3, -3}, LatticeIndex{-3, -3}, LatticeIndex{3, 3}, LatticeIndex{-3, 3}}) {
            const std::vector<LatticeIndex> cells{{0, 0}, other};
            QCOMPARE(detail::largestComponent(cells, 3, true).size(), std::size_t{2});
        }
        QCOMPARE(detail::largestComponent({{0, 0}, {4, -4}}, 3, true).size(), std::size_t{1});
    }

    void aFarAwayCellUsesTheSparsePathAndStillLinks() {
        // 一個離得很遠的格子讓索引範圍很大（走雜湊表的路）；附近的兩塊照樣正確。
        std::vector<LatticeIndex> cells{{0, 0}, {1, 0}, {2, 1}, {100000, 50000}, {100001, 50000}};
        const std::vector<std::size_t> keep = detail::largestComponent(cells, 3, true);
        QCOMPARE(keep.size(), std::size_t{3});
        QCOMPARE(keep[0], std::size_t{0});
    }
};

QTEST_APPLESS_MAIN(TstLatticeRules)
#include "tst_lattice_rules.moc"
