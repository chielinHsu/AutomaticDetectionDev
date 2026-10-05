// SimpleGrid（S03 暫代的晶格擬合）：用精確數字驗證擬合與缺格；S05 的 LatticeEstimator 取代它時，這些測試是現成的規格。
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <vector>

#include "application/usecases/SimpleGrid.h"
#include "domain/geometry.h"

using namespace aoi;

namespace {

struct GridSpec {
    int cols = 5;
    int rows = 4;
    double pitchX = 30.0;
    double pitchY = 20.0;
    double angleDeg = 0.0;
    Point2D origin{100.0, 100.0};
};

Point2D cellPosition(const GridSpec& g, int col, int row) {
    const Point2D offset = rotate({col * g.pitchX, row * g.pitchY}, g.angleDeg);
    return {g.origin.x + offset.x, g.origin.y + offset.y};
}

std::vector<Point2D> makePoints(const GridSpec& g, const std::vector<std::pair<int, int>>& skip = {}) {
    std::vector<Point2D> points;
    for (int c = 0; c < g.cols; ++c) {
        for (int r = 0; r < g.rows; ++r) {
            if (std::find(skip.begin(), skip.end(), std::make_pair(c, r)) != skip.end()) continue;
            points.push_back(cellPosition(g, c, r));
        }
    }
    return points;
}

const Rect kBig{0.0, 0.0, 2000.0, 2000.0};

bool hasPointNear(const std::vector<Point2D>& points, Point2D target, double tolerance) {
    return std::any_of(points.begin(), points.end(), [&](Point2D p) { return distance(p, target) <= tolerance; });
}

}  // namespace

class TstGrid : public QObject {
    Q_OBJECT

private slots:
    void aCompleteGridHasNoHolesAndTheRightPitch() {
        const GridSpec g;
        const auto model = fitGrid(makePoints(g));
        QVERIFY(model.has_value());
        QVERIFY(model->hasV);
        QCOMPARE(model->occupied.size(), std::size_t{20});
        QVERIFY2(model->rmsPx < 1e-6, "an exact grid fits with no residual");
        std::vector<double> pitches{std::hypot(model->u.x, model->u.y), std::hypot(model->v.x, model->v.y)};
        std::sort(pitches.begin(), pitches.end());
        QVERIFY(std::abs(pitches[0] - 20.0) < 1e-6);
        QVERIFY(std::abs(pitches[1] - 30.0) < 1e-6);
        QVERIFY(gridHoles(*model, kBig, 1000).empty());
    }

    void missingInteriorCellsBecomeHolesAtTheExactPositions() {
        const GridSpec g;
        const std::vector<std::pair<int, int>> skip{{1, 1}, {2, 2}, {3, 1}};
        const auto model = fitGrid(makePoints(g, skip));
        QVERIFY(model.has_value());
        const std::vector<Point2D> holes = gridHoles(*model, kBig, 1000);
        QCOMPARE(holes.size(), skip.size());
        for (const auto& [c, r] : skip) {
            QVERIFY2(hasPointNear(holes, cellPosition(g, c, r), 1e-6), "a skipped cell must be reported at its exact position");
        }
    }

    // 只補「已找到的點的索引範圍之內」的缺格：邊上整列缺失（範圍縮小）不會往外推算。
    void holesOutsideTheFoundRangeAreNotPredicted() {
        const GridSpec g;
        std::vector<std::pair<int, int>> skip;
        for (int r = 0; r < g.rows; ++r) skip.push_back({4, r});  // 最後一欄整欄缺
        const auto model = fitGrid(makePoints(g, skip));
        QVERIFY(model.has_value());
        QVERIFY(gridHoles(*model, kBig, 1000).empty());
    }

    void aRotatedNoisyGridStillFindsItsHoles() {
        GridSpec g;
        g.cols = 8;
        g.rows = 6;
        g.angleDeg = 15.0;
        std::vector<Point2D> points;
        const std::vector<std::pair<int, int>> skip{{3, 2}, {5, 4}};
        quint32 state = 12345;  // 固定的雜訊（不用 std 分佈：各平台一致）
        auto noise = [&] {
            state = state * 1664525u + 1013904223u;
            return (static_cast<double>(state >> 8) / 16777216.0 - 0.5);  // ±0.5
        };
        for (int c = 0; c < g.cols; ++c) {
            for (int r = 0; r < g.rows; ++r) {
                if (std::find(skip.begin(), skip.end(), std::make_pair(c, r)) != skip.end()) continue;
                Point2D p = cellPosition(g, c, r);
                points.push_back({p.x + noise(), p.y + noise()});
            }
        }
        const auto model = fitGrid(points);
        QVERIFY(model.has_value());
        QVERIFY(model->rmsPx < 1.0);
        const std::vector<Point2D> holes = gridHoles(*model, kBig, 1000);
        QCOMPARE(holes.size(), skip.size());
        for (const auto& [c, r] : skip) QVERIFY(hasPointNear(holes, cellPosition(g, c, r), 1.5));
    }

    void aSingleRowIsRecognizedAsOneDimensional() {
        GridSpec g;
        g.cols = 8;
        g.rows = 1;
        const auto model = fitGrid(makePoints(g, {{4, 0}}));
        QVERIFY(model.has_value());
        QVERIFY(!model->hasV);
        const std::vector<Point2D> holes = gridHoles(*model, kBig, 1000);
        QCOMPARE(holes.size(), std::size_t{1});
        QVERIFY(hasPointNear(holes, cellPosition(g, 4, 0), 1e-6));
    }

    void anOutlierDoesNotChangeTheFit() {
        const GridSpec g;
        std::vector<Point2D> points = makePoints(g);
        points.push_back({1234.5, 777.7});  // 不在晶格上
        const auto model = fitGrid(points);
        QVERIFY(model.has_value());
        QCOMPARE(model->occupied.size(), std::size_t{20});  // 離群點沒有佔格子
        QVERIFY(model->rmsPx < 1e-6);
    }

    void tooFewOrDegeneratePointsGiveNoModel() {
        QVERIFY(!fitGrid({}).has_value());
        QVERIFY(!fitGrid({{1.0, 2.0}}).has_value());
        QVERIFY2(!fitGrid({{5.0, 5.0}, {5.0, 5.0}, {5.0, 5.0}}).has_value(), "overlapping points have no direction");
    }

    void twoPointsMakeAOneDimensionalModelWithoutHoles() {
        const auto model = fitGrid({{10.0, 10.0}, {40.0, 10.0}});
        QVERIFY(model.has_value());
        QVERIFY(!model->hasV);
        QVERIFY(gridHoles(*model, kBig, 100).empty());
    }

    void holesOutsideTheBoundsAreDropped() {
        const GridSpec g;
        const auto model = fitGrid(makePoints(g, {{2, 1}}));
        QVERIFY(model.has_value());
        const Rect farAway{0.0, 0.0, 50.0, 50.0};  // 缺的格子在 (160, 120) 附近，不在這個範圍內
        QVERIFY(gridHoles(*model, farAway, 100).empty());
    }

    void theHoleCountIsCapped() {
        GridSpec g;
        g.cols = 10;
        g.rows = 10;
        std::vector<std::pair<int, int>> skip;
        for (int c = 2; c < 8; ++c) skip.push_back({c, 5});  // 6 個缺格
        const auto model = fitGrid(makePoints(g, skip));
        QVERIFY(model.has_value());
        QCOMPARE(gridHoles(*model, kBig, 4).size(), std::size_t{4});
        QCOMPARE(gridHoles(*model, kBig, 100).size(), std::size_t{6});
    }

    // 這是 S03 暫代版的已知限制，寫成測試讓它「被看見」：S05 取代後，這個測試應該改成預期找得到 v。
    void knownLimitationVeryDifferentPitchesAreSeenAsOneRow() {
        GridSpec g;
        g.cols = 10;
        g.rows = 3;
        g.pitchX = 10.0;
        g.pitchY = 200.0;  // 20 倍：每個點最近的 6 個鄰居都在同一列
        const auto model = fitGrid(makePoints(g));
        QVERIFY(model.has_value());
        QVERIFY2(!model->hasV, "documented limit: the simple fit only sees the nearest 6 neighbours");
    }
};

QTEST_APPLESS_MAIN(TstGrid)
#include "tst_grid.moc"
