// domain 型別：座標慣例（geometry.h）的數學、ImageView 的有效性、搜尋區域。
// domain 只用標準庫；測試用 Qt Test（ADR-003）。
#include <QtTest>
#include <cmath>

#include "domain/component.h"
#include "domain/geometry.h"
#include "domain/image_view.h"
#include "domain/match.h"
#include "domain/search_params.h"

using namespace aoi;

namespace {
bool close(double a, double b, double eps = 1e-9) { return std::abs(a - b) <= eps; }
}  // namespace

class TstDomain : public QObject {
    Q_OBJECT

private slots:
    void anglesAreNormalizedToHalfOpenRange_data() {
        QTest::addColumn<double>("input");
        QTest::addColumn<double>("expected");
        QTest::newRow("zero") << 0.0 << 0.0;
        QTest::newRow("+180 stays") << 180.0 << 180.0;
        QTest::newRow("-180 becomes +180") << -180.0 << 180.0;
        QTest::newRow("190") << 190.0 << -170.0;
        QTest::newRow("-190") << -190.0 << 170.0;
        QTest::newRow("360") << 360.0 << 0.0;
        QTest::newRow("540") << 540.0 << 180.0;
        QTest::newRow("725") << 725.0 << 5.0;
        QTest::newRow("-725") << -725.0 << -5.0;
    }
    void anglesAreNormalizedToHalfOpenRange() {
        QFETCH(double, input);
        QFETCH(double, expected);
        QVERIFY2(close(normalizeAngleDeg(input), expected), qPrintable(QString::number(normalizeAngleDeg(input))));
    }

    // 座標慣例的核心：y 向下、正角度在螢幕上是逆時針。(1, 0)（畫面右方）轉 +90° 要到畫面正上方 (0, −1)。
    void positiveAngleIsCounterClockwiseOnScreen() {
        const Point2D up = rotate({1.0, 0.0}, 90.0);
        QVERIFY(close(up.x, 0.0));
        QVERIFY(close(up.y, -1.0));
        const Point2D left = rotate({1.0, 0.0}, 180.0);
        QVERIFY(close(left.x, -1.0));
        QVERIFY(close(left.y, 0.0, 1e-12));
        const Point2D rightOfDown = rotate({0.0, 1.0}, 90.0);  // 畫面正下方轉 +90° 到畫面右方
        QVERIFY(close(rightOfDown.x, 1.0));
        QVERIFY(close(rightOfDown.y, 0.0));
    }

    void rotationKeepsLengthAndIsInvertible() {
        const Point2D v{12.5, -7.25};
        for (double deg : {-170.0, -45.0, 0.0, 17.0, 90.0, 133.0}) {
            const Point2D r = rotate(v, deg);
            QVERIFY(close(std::hypot(r.x, r.y), std::hypot(v.x, v.y)));
            const Point2D back = rotate(r, -deg);
            QVERIFY(close(back.x, v.x, 1e-9));
            QVERIFY(close(back.y, v.y, 1e-9));
        }
    }

    void degreesAndRadiansConvertBothWays() {
        QVERIFY(close(degreesToRadians(180.0), std::acos(-1.0)));
        QVERIFY(close(radiansToDegrees(degreesToRadians(33.3)), 33.3));
    }

    void rectEdgesAreHalfOpen() {
        const Rect r{0.0, 0.0, 10.0, 10.0};
        QVERIFY(r.contains({0.0, 0.0}));
        QVERIFY(r.contains({9.999, 9.999}));
        QVERIFY(!r.contains({10.0, 5.0}));
        QVERIFY(!r.contains({5.0, 10.0}));
        QVERIFY(!r.contains({-0.001, 5.0}));
    }

    void rectBasics() {
        const Rect r = rectFromCenter({50.0, 40.0}, {20.0, 10.0});
        QVERIFY(close(r.x, 40.0) && close(r.y, 35.0) && close(r.w, 20.0) && close(r.h, 10.0));
        QVERIFY(close(r.center().x, 50.0) && close(r.center().y, 40.0));
        QVERIFY(close(r.area(), 200.0));
        QVERIFY(Rect{}.empty());
        QVERIFY((Rect{0.0, 0.0, -3.0, 5.0}.empty()));
        QVERIFY(close((Rect{0.0, 0.0, -3.0, 5.0}.area()), 0.0));
    }

    void intersectionOfOverlapDisjointAndTouchingRects() {
        const Rect a{0.0, 0.0, 10.0, 10.0};
        const Rect overlap = intersection(a, {5.0, 6.0, 10.0, 10.0});
        QVERIFY(close(overlap.x, 5.0) && close(overlap.y, 6.0) && close(overlap.w, 5.0) && close(overlap.h, 4.0));
        QVERIFY(close(intersectionArea(a, {5.0, 6.0, 10.0, 10.0}), 20.0));
        QVERIFY(intersection(a, {20.0, 20.0, 5.0, 5.0}).empty());
        QVERIFY(intersection(a, {10.0, 0.0, 5.0, 5.0}).empty());  // 只碰到邊：面積 0
    }

    void boundingRectOfARotatedRect() {
        const RectObj square{{0.0, 0.0, 45.0}, 10.0, 10.0};
        const Rect b = boundingRect(square);
        QVERIFY(close(b.w, 10.0 * std::sqrt(2.0)) && close(b.h, 10.0 * std::sqrt(2.0)));
        const RectObj wide{{100.0, 50.0, 90.0}, 40.0, 10.0};
        const Rect w = boundingRect(wide);  // 轉 90°：寬高對調，中心不變
        QVERIFY(close(w.w, 10.0, 1e-9) && close(w.h, 40.0, 1e-9));
        QVERIFY(close(w.center().x, 100.0) && close(w.center().y, 50.0));
    }

    void imageViewValidity() {
        const std::uint8_t pixel = 0;
        ImageView v;
        QVERIFY(!v.valid());
        v.data = &pixel;
        v.width = 4;
        v.height = 2;
        v.strideBytes = 4;
        v.format = PixelFormat::Gray8;
        QVERIFY(v.valid());
        QCOMPARE(v.sizeBytes(), std::size_t{8});
        v.strideBytes = 3;  // stride 小於一列的位元組數
        QVERIFY(!v.valid());
        v.format = PixelFormat::Bgrx32;
        v.strideBytes = 16;
        QVERIFY(v.valid());
        QCOMPARE(bytesPerPixel(PixelFormat::Rgb24), 3);
        QVERIFY2(ImageView{}.umPerPixel == 0.0, "an uncalibrated image has umPerPixel 0");
    }

    void searchRegionFactories() {
        const SearchRegion whole = SearchRegion::wholeImage();
        QVERIFY(whole.kind == SearchRegion::Kind::WholeImage);
        QVERIFY(whole.windows.empty());
        const SearchRegion windows = SearchRegion::inWindows({{0, 0, 4, 4}, {10, 10, 4, 4}});
        QVERIFY(windows.kind == SearchRegion::Kind::Windows);
        QCOMPARE(windows.windows.size(), std::size_t{2});
    }

    void searchParamsDefaultsAreSane() {
        const SearchParams p;
        QVERIFY(p.minScore > 0.0 && p.minScore <= 1.0);
        QVERIFY(p.angleMinDeg <= p.angleMaxDeg);
        QVERIFY(p.maxMatches > 0);
    }

    // 狀態與來源的對應：Predicted 沒有分數，不能拿來排序或顯示。
    void onlyPredictedCandidatesLackAScore() {
        QVERIFY((Candidate{{}, 0.0, CandidateSource::Sample}.hasScore()));
        QVERIFY((Candidate{{}, 0.5, CandidateSource::Seed}.hasScore()));
        QVERIFY((Candidate{{}, 0.5, CandidateSource::Verified}.hasScore()));
        QVERIFY(!(Candidate{{}, 0.0, CandidateSource::Predicted}.hasScore()));
    }

    // 階層：框不單獨儲存，板子保存自己的類型複本，並記得來源（libId）。
    void boardKeepsItsOwnTypeCopies() {
        Board board;
        ComponentType type;
        type.id = "p_8371";
        type.libId = "8371";
        type.partNumber = "8371";
        BoxGroup body;
        body.id = 1;
        body.kind = BoxGroupKind::Body;
        body.widthUm = 5000.0;
        body.heightUm = 3000.0;
        type.groups.push_back(body);
        board.types.push_back(type);
        ComponentInstance u1;
        u1.id = 1;
        u1.name = "U1";
        u1.typeId = "p_8371";
        board.components.push_back(u1);
        QVERIFY(!board.components.front().score.has_value());  // 手動補的沒有分數；預設不偽造
        QCOMPARE(board.components.front().typeId, board.types.front().id);
        QVERIFY(board.components.front().origin == ObjectOrigin::Auto);
        QCOMPARE(board.types.front().libId, std::string("8371"));
    }
};

QTEST_APPLESS_MAIN(TstDomain)
#include "tst_domain.moc"
