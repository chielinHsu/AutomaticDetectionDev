// 與 S00 示範頁的演算法交叉驗證（S05 完成定義的一項）。
//
// fixture（tests/domain/fixtures/s00-demo-lattice.json）是 tools/s00-demo-fixture.mjs 直接執行 S00 頁面「原本的」
// JavaScript 產生的：36 組參數（缺件 0／2／5、雜訊 0／3／6 px、傾斜 0／8°、邊驗證邊重新擬合 開／關）。
// 同樣的點，C++ 要得到同樣的結果：
//   1. 種子的最小平方法（已知索引）：LatticeAccumulator 的解 ＝ 頁面的 seedModel。
//   2. 漸進驗證：依頁面的順序走訪，每個格子「找到／推算」的判斷完全相同，最後的模型與推算的位置相同。
//      頁面每找到一個就把所有點重新算一遍；C++ 用正規方程累加器每次 O(1) 更新——結果要一樣，這也證明 O(1) 的更新是對的。
//   3. 估計器（不知道索引）：LatticeEstimator 只拿到點，要自己找出晶格；每個點的格點位置 ＝ 頁面用已知索引擬合的結果。
// CTest 的 s00_demo_fixture_is_current 另外檢查 fixture 與 S00 頁面一致（頁面改了，fixture 就過期）。
#include <QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <vector>

#include "domain/geometry.h"
#include "domain/lattice/lattice_accumulator.h"
#include "domain/lattice/lattice_estimator.h"

using namespace aoi;

namespace {

Point2D pointOf(const QJsonArray& a) { return {a.at(0).toDouble(), a.at(1).toDouble()}; }

LatticeBasis basisOf(const QJsonObject& m) {
    LatticeBasis b;
    b.origin = pointOf(m.value("o").toArray());
    b.u = pointOf(m.value("u").toArray());
    b.v = pointOf(m.value("v").toArray());
    b.hasV = true;
    return b;
}

bool sameBasis(const LatticeBasis& a, const LatticeBasis& b, double tol) {
    return distance(a.origin, b.origin) <= tol && distance(a.u, b.u) <= tol && distance(a.v, b.v) <= tol;
}

QByteArray describe(const QJsonObject& c) {
    return QStringLiteral("miss=%1 noise=%2 tilt=%3 prog=%4")
        .arg(c.value("miss").toInt()).arg(c.value("noise").toDouble()).arg(c.value("tilt").toDouble()).arg(c.value("prog").toBool())
        .toUtf8();
}

}  // namespace

class TstLatticeCrosscheck : public QObject {
    Q_OBJECT

    QJsonArray cases_;

private slots:
    void initTestCase() {
        QFile f(QStringLiteral(AOI_DOMAIN_FIXTURES "/s00-demo-lattice.json"));
        QVERIFY2(f.open(QIODevice::ReadOnly), "the S00 fixture is missing: run node tools/s00-demo-fixture.mjs");
        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &error);
        QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));
        cases_ = doc.object().value("cases").toArray();
        QCOMPARE(cases_.size(), 36);
    }

    void crosscheck_data() {
        QTest::addColumn<int>("index");
        for (int k = 0; k < 36; ++k) QTest::addRow("case %d", k) << k;
    }

    void crosscheck() {
        QFETCH(int, index);
        const QJsonObject c = cases_.at(index).toObject();
        const QByteArray name = describe(c);

        // 格子 → 點（i、j、x、y、在不在）
        struct Pin {
            int i, j;
            Point2D p;
            bool present;
        };
        std::vector<Pin> pins;
        for (const QJsonValue& v : c.value("pins").toArray()) {
            const QJsonArray a = v.toArray();
            pins.push_back({a.at(0).toInt(), a.at(1).toInt(), {a.at(2).toDouble(), a.at(3).toDouble()}, a.at(4).toInt() == 1});
        }
        const auto pinAt = [&](int i, int j) -> const Pin& {
            for (const Pin& p : pins) {
                if (p.i == i && p.j == j) return p;
            }
            return pins.front();
        };

        // ---- 1. 種子的最小平方法 ----
        LatticeAccumulator acc(pins.front().p, {0, 0});
        for (const QJsonValue& v : c.value("seeds").toArray()) {
            const QJsonArray s = v.toArray();
            const Pin& pin = pinAt(s.at(0).toInt(), s.at(1).toInt());
            acc.add({pin.i, pin.j}, pin.p);
        }
        std::optional<LatticeBasis> model = acc.solve();
        QVERIFY2(model.has_value() && model->hasV, name.constData());
        QVERIFY2(sameBasis(*model, basisOf(c.value("seedModel").toObject()), 1e-9), name.constData());

        // ---- 2. 漸進驗證：同樣的判斷、同樣的最後模型、同樣的推算位置 ----
        const QJsonArray window = c.value("window").toArray();
        const double wx = window.at(0).toDouble();
        const double wy = window.at(1).toDouble();
        const bool progressive = c.value("prog").toBool();
        const QJsonArray results = c.value("results").toArray();
        for (const QJsonValue& v : results) {
            const QJsonArray r = v.toArray();
            const Pin& pin = pinAt(r.at(0).toInt(), r.at(1).toInt());
            const Point2D predicted = model->at(pin.i, pin.j);
            const bool hit = pin.present && std::abs(pin.p.x - predicted.x) <= wx && std::abs(pin.p.y - predicted.y) <= wy;
            QVERIFY2(hit == (r.at(2).toInt() == 1), (name + " cell " + QByteArray::number(pin.i) + "," + QByteArray::number(pin.j)).constData());
            if (!hit) continue;
            acc.add({pin.i, pin.j}, pin.p);
            if (progressive) model = acc.solve();
        }
        QVERIFY2(sameBasis(*model, basisOf(c.value("finalModel").toObject()), 1e-9), name.constData());
        for (const QJsonValue& v : results) {
            const QJsonArray r = v.toArray();
            if (r.at(2).toInt() == 1) continue;
            const Point2D inferred = model->at(r.at(0).toInt(), r.at(1).toInt());
            QVERIFY2(distance(inferred, {r.at(3).toDouble(), r.at(4).toDouble()}) <= 1e-9, name.constData());
        }

        // ---- 3. 估計器：只給點，要自己找出同一組晶格 ----
        std::vector<Point2D> present;
        std::vector<const Pin*> owner;
        for (const Pin& p : pins) {
            if (!p.present) continue;
            present.push_back(p.p);
            owner.push_back(&p);
        }
        const auto fitted = LatticeEstimator().fit(present);
        QVERIFY2(fitted.has_value(), name.constData());
        QVERIFY2(fitted->hasV(), name.constData());
        QCOMPARE(fitted->inliers.size(), present.size());
        const LatticeBasis truth = basisOf(c.value("fullFit").toObject());
        for (std::size_t k = 0; k < fitted->inliers.size(); ++k) {
            const Pin& pin = *owner[fitted->inliers[k]];
            const Point2D mine = fitted->basis.at(fitted->cells[k]);
            const Point2D theirs = truth.at(pin.i, pin.j);
            QVERIFY2(distance(mine, theirs) <= 1e-6, (name + " estimator vs the demo's known-index fit").constData());
        }
    }
};

QTEST_APPLESS_MAIN(TstLatticeCrosscheck)
#include "tst_lattice_crosscheck.moc"
