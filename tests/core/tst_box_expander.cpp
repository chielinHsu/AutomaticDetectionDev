// BoxExpander：BoxGroup＋位姿 → 具體的框。純幾何，用精確的數字驗證（座標與角度慣例見 domain/geometry.h）。
#include <QtTest>
#include <cmath>

#include "domain/box_expander.h"

using namespace aoi;

namespace {

BoxGroup body(double w, double h) {
    BoxGroup g;
    g.id = 1;
    g.kind = BoxGroupKind::Body;
    g.widthUm = w;
    g.heightUm = h;
    return g;
}

BoxGroup pins(int id, int rows, int cols, double pitchX, double pitchY, double w, double h, double cx = 0, double cy = 0) {
    BoxGroup g;
    g.id = id;
    g.kind = BoxGroupKind::Pin;
    g.widthUm = w;
    g.heightUm = h;
    g.centerXUm = cx;
    g.centerYUm = cy;
    g.matrix.rows = rows;
    g.matrix.cols = cols;
    g.matrix.pitchXUm = pitchX;
    g.matrix.pitchYUm = pitchY;
    return g;
}

BoxGroup window(int id, int parent, double ox, double oy, double wr, double hr) {
    BoxGroup g;
    g.id = id;
    g.kind = BoxGroupKind::Window;
    g.window.parentGroupId = parent;
    g.window.offsetXRatio = ox;
    g.window.offsetYRatio = oy;
    g.window.widthRatio = wr;
    g.window.heightRatio = hr;
    return g;
}

bool near(double a, double b, double eps = 1e-9) { return std::abs(a - b) <= eps; }

}  // namespace

class TstBoxExpander : public QObject {
    Q_OBJECT

private slots:
    void aBodyOnlyTypeGivesOneBoxAtThePose() {
        ComponentType t;
        t.groups = {body(200, 110)};
        const auto boxes = expandBoxes(t, {300, 400, 0}, 1.0);
        QCOMPARE(boxes.size(), std::size_t{1});
        QVERIFY(boxes[0].kind == BoxGroupKind::Body);
        QCOMPARE(boxes[0].rect.pose.x, 300.0);
        QCOMPARE(boxes[0].rect.pose.y, 400.0);
        QCOMPARE(boxes[0].rect.width, 200.0);
        QCOMPARE(boxes[0].rect.height, 110.0);
        QCOMPARE(boxes[0].groupId, 1);
    }

    void micrometresBecomePixelsThroughTheCalibration() {
        ComponentType t;
        t.groups = {body(200, 100)};
        const auto boxes = expandBoxes(t, {50, 60, 0}, 2.0);  // 2 µm／像素
        QCOMPARE(boxes[0].rect.width, 100.0);
        QCOMPARE(boxes[0].rect.height, 50.0);
    }

    void anUncalibratedImageIsTreatedAsOneMicrometrePerPixelForNow() {
        QCOMPARE(effectiveUmPerPixel(0.0), 1.0);
        QCOMPARE(effectiveUmPerPixel(-3.0), 1.0);
        QCOMPARE(effectiveUmPerPixel(0.5), 0.5);
        ComponentType t;
        t.groups = {body(40, 20)};
        QCOMPARE(expandBoxes(t, {0, 0, 0}, 0.0)[0].rect.width, 40.0);
    }

    void aPinMatrixIsCenteredAndSpacedByThePitch() {
        ComponentType t;
        t.groups = {body(200, 110), pins(2, 2, 3, 50, 140, 20, 30)};
        const auto boxes = expandBoxes(t, {100, 100, 0}, 1.0);
        QCOMPARE(boxes.size(), std::size_t{1 + 6});
        // 列 0 在上（y 較小）、欄 0 在左：x ∈ {−50, 0, 50}，y ∈ {−70, +70}
        const double xs[] = {50, 100, 150, 50, 100, 150};
        const double ys[] = {30, 30, 30, 170, 170, 170};
        for (int i = 0; i < 6; ++i) {
            const ExpandedBox& b = boxes[static_cast<std::size_t>(1 + i)];
            QVERIFY(b.kind == BoxGroupKind::Pin);
            QVERIFY2(near(b.rect.pose.x, xs[i]) && near(b.rect.pose.y, ys[i]),
                     qPrintable(QString("pin %1 at (%2, %3)").arg(i).arg(b.rect.pose.x).arg(b.rect.pose.y)));
            QCOMPARE(b.rect.width, 20.0);
            QCOMPARE(b.rect.height, 30.0);
        }
    }

    void skippedCellsProduceNoBox() {
        ComponentType t;
        BoxGroup g = pins(2, 2, 2, 10, 10, 4, 4);
        g.matrix.skipped = {{0, 1}, {1, 0}};
        t.groups = {body(30, 30), g};
        QCOMPARE(expandBoxes(t, {0, 0, 0}, 1.0).size(), std::size_t{1 + 2});
        QCOMPARE(countBoxes(t), std::size_t{1 + 2});
    }

    void aPinGroupCanBeOffsetFromTheBodyCenter() {
        ComponentType t;
        t.groups = {body(100, 60), pins(2, 1, 1, 0, 0, 10, 10, 0, -50)};  // 一支腳在主體上方 50 µm
        const auto boxes = expandBoxes(t, {200, 200, 0}, 1.0);
        QCOMPARE(boxes[1].rect.pose.x, 200.0);
        QCOMPARE(boxes[1].rect.pose.y, 150.0);
    }

    void aWindowIsARatioOfEachParentBox() {
        ComponentType t;
        // 兩支腳（1 列 2 欄，間距 100），每支腳一個窗：寬 140%、高 200%，中心偏下 0.5 個父框高。
        t.groups = {body(300, 100), pins(2, 1, 2, 100, 0, 20, 40), window(3, 2, 0.0, 0.5, 1.4, 2.0)};
        const auto boxes = expandBoxes(t, {0, 0, 0}, 1.0);
        QCOMPARE(boxes.size(), std::size_t{1 + 2 + 2});
        const ExpandedBox& w0 = boxes[3];
        const ExpandedBox& w1 = boxes[4];
        QVERIFY(w0.kind == BoxGroupKind::Window);
        QCOMPARE(w0.groupId, 3);
        QVERIFY(near(w0.rect.width, 28.0));   // 20 × 1.4
        QVERIFY(near(w0.rect.height, 80.0));  // 40 × 2.0
        QVERIFY(near(w0.rect.pose.x, -50.0)); // 與父框同中心
        QVERIFY(near(w0.rect.pose.y, 20.0));  // 0 + 0.5 × 40
        QVERIFY(near(w1.rect.pose.x, 50.0));
        QCOMPARE(countBoxes(t), std::size_t{5});
    }

    void aWindowFollowsItsParentWhenTheParentChanges() {
        ComponentType t;
        t.groups = {body(100, 100), window(2, 1, 0, 0, 0.8, 0.6)};
        QCOMPARE(expandBoxes(t, {0, 0, 0}, 1.0)[1].rect.width, 80.0);
        t.groups[0].widthUm = 200;  // 改父框
        const auto boxes = expandBoxes(t, {0, 0, 0}, 1.0);
        QCOMPARE(boxes[1].rect.width, 160.0);
        QCOMPARE(boxes[1].rect.height, 60.0);
    }

    void aWindowWithoutAValidParentIsDropped() {
        ComponentType t;
        t.groups = {body(100, 100), window(2, 99, 0, 0, 1, 1), window(3, 2, 0, 0, 1, 1)};  // 找不到父；父是 Window
        QCOMPARE(expandBoxes(t, {0, 0, 0}, 1.0).size(), std::size_t{1});
        QCOMPARE(countBoxes(t), std::size_t{1});
    }

    void rotationFollowsTheScreenCounterClockwiseConvention() {
        // 腳在主體右邊 30 µm。逆時針 90°（螢幕上）：右邊 → 上方，y 變小。
        ComponentType t;
        t.groups = {body(100, 60), pins(2, 1, 1, 0, 0, 10, 10, 30, 0)};
        const auto boxes = expandBoxes(t, {100, 100, 90}, 1.0);
        QVERIFY(near(boxes[1].rect.pose.x, 100.0));
        QVERIFY(near(boxes[1].rect.pose.y, 70.0));
        QCOMPARE(boxes[1].rect.pose.angleDeg, 90.0);  // 框的角度＝元件的角度
        QCOMPARE(boxes[1].rect.width, 10.0);          // 寬高仍是未旋轉的
    }

    void aWholeMatrixRotatesAsOneRigidBody() {
        ComponentType t;
        t.groups = {body(100, 60), pins(2, 1, 3, 20, 0, 4, 4)};
        const auto flat = expandBoxes(t, {50, 50, 0}, 1.0);
        const auto turned = expandBoxes(t, {50, 50, 180}, 1.0);
        for (std::size_t i = 1; i < flat.size(); ++i) {
            // 轉 180°：相對主體中心的位移取反
            QVERIFY(near(turned[i].rect.pose.x - 50.0, -(flat[i].rect.pose.x - 50.0), 1e-9));
            QVERIFY(near(turned[i].rect.pose.y - 50.0, -(flat[i].rect.pose.y - 50.0), 1e-9));
        }
    }

    void the41BoxICFromTheSpecHas41Boxes() {
        // ux-spec 第 2 節：類型 8371 ＝ 1 個 Body ＋ 1 個 Pin 矩陣（2 × 10）＋ 1 組 Solder 窗（每支腳一個）＝ 41 個框。
        ComponentType t;
        t.groups = {body(200, 110), pins(2, 2, 10, 20, 140, 12, 24), window(3, 2, 0, 0, 1.4, 2.0)};
        QCOMPARE(expandBoxes(t, {0, 0, 0}, 1.0).size(), std::size_t{41});
        QCOMPARE(countBoxes(t), std::size_t{41});
    }

    void anEmptyTypeGivesNoBoxes() {
        const ComponentType t;
        QVERIFY(expandBoxes(t, {0, 0, 0}, 1.0).empty());
        QCOMPARE(countBoxes(t), std::size_t{0});
    }
};

QTEST_APPLESS_MAIN(TstBoxExpander)
#include "tst_box_expander.moc"
