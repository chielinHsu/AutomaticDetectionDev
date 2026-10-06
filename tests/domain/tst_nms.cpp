// 非極大值抑制（S05）：旋轉矩形的重疊面積（精確數字）、壓制規則與排序、空間雜湊與「全部兩兩比較」的結果相同。
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "LatticeTestSupport.h"
#include "domain/lattice/non_max_suppression.h"

using namespace aoi;
using namespace aoi::test;

namespace {

RectObj box(double x, double y, double w, double h, double angle = 0.0) { return {{x, y, angle}, w, h}; }

// 對照組：不用空間雜湊，每個框和所有已留下的框比。
std::vector<std::size_t> bruteForceKeep(const std::vector<ScoredBox>& boxes, const NmsOptions& o) {
    std::vector<std::size_t> order(boxes.size());
    for (std::size_t k = 0; k < order.size(); ++k) order[k] = k;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (boxes[a].score != boxes[b].score) return boxes[a].score > boxes[b].score;
        if (boxes[a].rect.pose.y != boxes[b].rect.pose.y) return boxes[a].rect.pose.y < boxes[b].rect.pose.y;
        if (boxes[a].rect.pose.x != boxes[b].rect.pose.x) return boxes[a].rect.pose.x < boxes[b].rect.pose.x;
        return a < b;
    });
    std::vector<std::size_t> kept;
    for (const std::size_t k : order) {
        bool suppressed = false;
        for (const std::size_t q : kept) suppressed = suppressed || overlapRatio(boxes[k].rect, boxes[q].rect, o.measure) > o.maxOverlap;
        if (!suppressed) kept.push_back(k);
    }
    return kept;
}

}  // namespace

class TstNms : public QObject {
    Q_OBJECT

private slots:
    // ---- 重疊面積（精確數字） ----

    void overlapAreasHaveExactValues() {
        QVERIFY(std::abs(overlapArea(box(0, 0, 10, 10), box(0, 0, 10, 10)) - 100.0) < 1e-9);  // 完全相同
        QVERIFY(std::abs(overlapArea(box(0, 0, 10, 10), box(5, 0, 10, 10)) - 50.0) < 1e-9);   // 錯開半個
        QVERIFY(std::abs(overlapArea(box(0, 0, 10, 10), box(5, 5, 10, 10)) - 25.0) < 1e-9);   // 錯開一個角
        QCOMPARE(overlapArea(box(0, 0, 10, 10), box(10, 0, 10, 10)), 0.0);                     // 只碰到邊
        QCOMPARE(overlapArea(box(0, 0, 10, 10), box(30, 0, 10, 10)), 0.0);                     // 離很遠
        QVERIFY(std::abs(overlapArea(box(0, 0, 10, 10), box(0, 0, 4, 4)) - 16.0) < 1e-9);     // 小框在裡面
        // 邊長 10 的正方形轉 45°，和軸對齊的同中心正方形：交集是正八邊形，面積 200(√2 − 1)。
        const double octagon = 200.0 * (std::numbers::sqrt2 - 1.0);
        QVERIFY(std::abs(overlapArea(box(0, 0, 10, 10), box(0, 0, 10, 10, 45.0)) - octagon) < 1e-9);
        // 長條轉 90°：兩個 20×4 的十字交叉，交集 4×4。
        QVERIFY(std::abs(overlapArea(box(0, 0, 20, 4), box(0, 0, 20, 4, 90.0)) - 16.0) < 1e-9);
        // 寬或高 ≤ 0 的矩形面積是 0。
        QCOMPARE(overlapArea(box(0, 0, 0, 10), box(0, 0, 10, 10)), 0.0);
        QCOMPARE(overlapArea(box(0, 0, -3, 10), box(0, 0, 10, 10)), 0.0);
    }

    void overlapIsSymmetricAndOrientationIndependent() {
        Rng rng(9);
        for (int k = 0; k < 200; ++k) {
            const RectObj a = box(rng.symmetric(10), rng.symmetric(10), 2 + rng.uniform() * 12, 2 + rng.uniform() * 12, rng.symmetric(180));
            const RectObj b = box(rng.symmetric(10), rng.symmetric(10), 2 + rng.uniform() * 12, 2 + rng.uniform() * 12, rng.symmetric(180));
            const double ab = overlapArea(a, b);
            QVERIFY(std::abs(ab - overlapArea(b, a)) < 1e-9);
            QVERIFY(ab >= 0.0 && ab <= std::min(a.width * a.height, b.width * b.height) + 1e-9);
            RectObj turned = a;  // 轉 180° 是同一個矩形
            turned.pose.angleDeg += 180.0;
            QVERIFY(std::abs(overlapArea(turned, b) - ab) < 1e-9);
            RectObj swapped = a;  // 寬高互換再轉 90° 也是同一個矩形
            std::swap(swapped.width, swapped.height);
            swapped.pose.angleDeg += 90.0;
            QVERIFY(std::abs(overlapArea(swapped, b) - ab) < 1e-9);
        }
    }

    void overlapRatiosFollowTheirDefinitions() {
        const RectObj a = box(0, 0, 10, 10);
        const RectObj b = box(5, 0, 10, 10);
        QVERIFY(std::abs(overlapRatio(a, b, OverlapMeasure::IntersectionOverUnion) - 50.0 / 150.0) < 1e-12);
        QVERIFY(std::abs(overlapRatio(a, b, OverlapMeasure::IntersectionOverSmaller) - 0.5) < 1e-12);
        const RectObj inner = box(0, 0, 4, 4);
        QVERIFY(std::abs(overlapRatio(a, inner, OverlapMeasure::IntersectionOverUnion) - 0.16) < 1e-12);
        QVERIFY(std::abs(overlapRatio(a, inner, OverlapMeasure::IntersectionOverSmaller) - 1.0) < 1e-12);
        QCOMPARE(overlapRatio(a, box(0, 0, 0, 0), OverlapMeasure::IntersectionOverUnion), 0.0);
    }

    // 斜放的密集元件：外接矩形會重疊，實際上不重疊。用外接矩形算的話，這一排會被壓掉一半。
    void tiltedNeighboursThatOnlyTouchInTheirBoundingBoxesAreKept() {
        std::vector<ScoredBox> row;
        for (int k = 0; k < 10; ++k) {
            const Point2D p = rotate({k * 21.0, 0.0}, 30.0);
            row.push_back({box(100 + p.x, 100 + p.y, 20, 8, 30.0), 0.9});
        }
        QVERIFY(intersectionArea(boundingRect(row[0].rect), boundingRect(row[1].rect)) > 0.0);
        QCOMPARE(overlapArea(row[0].rect, row[1].rect), 0.0);
        QCOMPARE(NonMaxSuppression().keep(row).size(), std::size_t{10});
    }

    // ---- 壓制規則 ----

    void theBestOfADuplicateGroupSurvives() {
        const std::vector<ScoredBox> boxes{{box(50, 50, 20, 10), 0.80},
                                           {box(51, 50, 20, 10), 0.95},  // 同一個元件，分數最高
                                           {box(52, 51, 20, 10), 0.85},
                                           {box(100, 50, 20, 10), 0.70}};  // 另一個元件
        const std::vector<std::size_t> kept = NonMaxSuppression().keep(boxes);
        QCOMPARE(kept.size(), std::size_t{2});
        QCOMPARE(kept[0], std::size_t{1});
        QCOMPARE(kept[1], std::size_t{3});
    }

    void theToleranceDecidesWhatCountsAsTheSamePart() {
        const std::vector<ScoredBox> pair{{box(0, 0, 10, 10), 0.9}, {box(5, 0, 10, 10), 0.8}};  // IoU 1/3
        QCOMPARE(NonMaxSuppression({0.5, OverlapMeasure::IntersectionOverUnion}).keep(pair).size(), std::size_t{2});
        QCOMPARE(NonMaxSuppression({0.3, OverlapMeasure::IntersectionOverUnion}).keep(pair).size(), std::size_t{1});
        QCOMPARE(NonMaxSuppression({0.5, OverlapMeasure::IntersectionOverSmaller}).keep(pair).size(), std::size_t{2});  // 0.5 不大於 0.5
        QCOMPARE(NonMaxSuppression({0.49, OverlapMeasure::IntersectionOverSmaller}).keep(pair).size(), std::size_t{1});
    }

    void tiesAreBrokenByYThenXThenIndex() {
        const std::vector<ScoredBox> boxes{{box(30, 10, 4, 4), 0.9}, {box(10, 20, 4, 4), 0.9}, {box(20, 10, 4, 4), 0.9},
                                           {box(20, 10, 4, 4), 0.9}};  // 和 [2] 完全相同
        const std::vector<std::size_t> kept = NonMaxSuppression().keep(boxes);
        QCOMPARE(kept.size(), std::size_t{3});
        QCOMPARE(kept[0], std::size_t{2});  // y 10、x 20、索引較小
        QCOMPARE(kept[1], std::size_t{0});  // y 10、x 30
        QCOMPARE(kept[2], std::size_t{1});  // y 20
    }

    void nonFiniteBoxesAreDropped() {
        const std::vector<ScoredBox> boxes{{box(0, 0, 10, 10), qQNaN()}, {box(qInf(), 0, 10, 10), 0.9}, {box(50, 50, 10, 10), 0.5}};
        const std::vector<std::size_t> kept = NonMaxSuppression().keep(boxes);
        QCOMPARE(kept.size(), std::size_t{1});
        QCOMPARE(kept[0], std::size_t{2});
        QVERIFY(NonMaxSuppression().keep({}).empty());
    }

    // 空間雜湊只看附近的格子：結果要和每個框都兩兩比較的做法完全一樣（大小不一的框、各種角度、很多重複）。
    void theSpatialHashGivesTheSameAnswerAsComparingEveryPair_data() {
        QTest::addColumn<int>("seed");
        QTest::addColumn<int>("measure");
        for (int seed = 1; seed <= 6; ++seed) {
            QTest::addRow("seed %d IoU", seed) << seed << 0;
            QTest::addRow("seed %d IoSmaller", seed) << seed << 1;
        }
    }
    void theSpatialHashGivesTheSameAnswerAsComparingEveryPair() {
        QFETCH(int, seed);
        QFETCH(int, measure);
        Rng rng(static_cast<std::uint64_t>(seed));
        std::vector<ScoredBox> boxes;
        for (int k = 0; k < 400; ++k) {
            const double size = k % 7 == 0 ? 60.0 : 15.0;  // 有幾個大框：雜湊的格子要用最大的對角線
            boxes.push_back({box(rng.uniform() * 400, rng.uniform() * 300, size * (0.5 + rng.uniform()), size * (0.5 + rng.uniform()),
                                 rng.symmetric(90)),
                             std::round(rng.uniform() * 20) / 20});  // 很多同分
        }
        NmsOptions o;
        o.maxOverlap = 0.3;
        o.measure = measure == 0 ? OverlapMeasure::IntersectionOverUnion : OverlapMeasure::IntersectionOverSmaller;
        QCOMPARE(NonMaxSuppression(o).keep(boxes), bruteForceKeep(boxes, o));
    }

    void anOverlapIndexAnswersForTheBoxesItHolds() {
        OverlapIndex index(30.0, 0.3, OverlapMeasure::IntersectionOverUnion);
        QVERIFY(!index.overlapsAny(box(0, 0, 20, 10)));
        index.add(box(0, 0, 20, 10));
        QCOMPARE(index.size(), std::size_t{1});
        QVERIFY(index.overlapsAny(box(1, 0, 20, 10)));
        QVERIFY(!index.overlapsAny(box(25, 0, 20, 10)));
        QVERIFY(!index.overlapsAny(box(qQNaN(), 0, 20, 10)));

        // 比格子大很多的框：中心離得遠（相隔 2 格以上）也可能重疊，附近要看更多格。
        OverlapIndex big(30.0, 0.3, OverlapMeasure::IntersectionOverSmaller);
        big.add(box(200, 0, 100, 100));
        QVERIFY(std::abs(overlapRatio(box(245, 0, 60, 60), box(200, 0, 100, 100), OverlapMeasure::IntersectionOverSmaller) - 2100.0 / 3600.0) < 1e-9);
        QVERIFY(big.overlapsAny(box(245, 0, 60, 60)));
        QVERIFY(!big.overlapsAny(box(300, 0, 60, 60)));
    }
};

QTEST_APPLESS_MAIN(TstNms)
#include "tst_nms.moc"
