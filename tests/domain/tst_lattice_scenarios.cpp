// LatticeEstimator 的情境掃描（S05）：雜訊 × 缺件 × 傾斜 × 離群點，每一格都和產生器的標準答案逐點比對。
//
// 要證明的主張（寫在 docs/steps/S05-lattice/spec.md）：
//   範圍內（每軸均勻雜訊 ≤ 0.1 格距、缺件 ≤ 40%、離群點 ≤ 20 個、任何角度）：一定找到二維晶格，
//     沒有指派錯的點、沒有假的缺格、範圍內真的缺件全部補到。
//   範圍外：可以失敗，但失敗要「安全」：沒有晶格，或補出的假缺格很少，而且標成建議檢查。
//   缺件 ≥ 50% 時，整組二維晶格要補的缺格比證據多（holesAreBacked），不成立；常會退回到其中幾排各自的一維晶格，
//   只補那幾排裡的缺件（總表的 exact 是 0：主晶格不是整個二維陣列）。
// 結果的總表印在 log（qInfo），失敗時看得到是哪一格。
#include <QtTest>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "LatticeTestSupport.h"
#include "domain/lattice/lattice_estimator.h"

using namespace aoi;
using namespace aoi::test;

namespace {

GridSpec sweepGrid(double angleDeg) {
    GridSpec g;
    g.cols = 12;
    g.rows = 8;
    g.pitchX = 30.0;
    g.pitchY = 22.0;
    g.angleDeg = angleDeg;
    g.origin = {300.0, 300.0};
    return g;
}

}  // namespace

class TstLatticeScenarios : public QObject {
    Q_OBJECT

    // 每個（雜訊、缺件）組合：情境數、完全正確的數（主晶格二維、沒有指派錯、沒有假缺格、沒有漏補）、沒有晶格的數。
    struct Tally {
        int runs = 0;
        int exact = 0;
        int none = 0;
    };
    std::map<std::pair<double, double>, Tally> tally_;

private slots:
    void cleanupTestCase() {
        qInfo().noquote() << "summary: exact = main lattice 2-D, no wrong cell, no false hole, no missed hole";
        for (const auto& [key, t] : tally_) {
            qInfo().noquote() << QStringLiteral("noise %1 px  missing %2  exact %3/%4  no-lattice %5")
                                     .arg(key.first, 4, 'f', 1).arg(key.second, 4, 'f', 2).arg(t.exact).arg(t.runs).arg(t.none);
        }
    }

    void sweep_data() {
        QTest::addColumn<double>("noise");
        QTest::addColumn<double>("missing");
        QTest::addColumn<double>("angle");
        QTest::addColumn<int>("outliers");
        for (const double noise : {0.0, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0}) {
            for (const double missing : {0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6}) {
                for (const double angle : {0.0, 5.0, 17.0, 33.0, 45.0, 90.0, -60.0}) {
                    for (const int outliers : {0, 5, 20}) {
                        QTest::addRow("noise=%g missing=%g angle=%g outliers=%d", noise, missing, angle, outliers)
                            << noise << missing << angle << outliers;
                    }
                }
            }
        }
    }

    void sweep() {
        QFETCH(double, noise);
        QFETCH(double, missing);
        QFETCH(double, angle);
        QFETCH(int, outliers);
        Scenario s;
        s.grid = sweepGrid(angle);
        s.noisePx = noise;
        s.missingFraction = missing;
        s.outliers = outliers;
        s.seed = 1000u + static_cast<std::uint64_t>(noise * 10) * 97u + static_cast<std::uint64_t>(missing * 100) * 13u +
                 static_cast<std::uint64_t>(angle + 100.0) * 7u + static_cast<std::uint64_t>(outliers);
        const Observed o = generate(s);
        const LatticeFit fit = LatticeEstimator().fitAll(o.points);
        const double cellTolerance = 0.25 * 22.0;

        Tally& t = tally_[{noise, missing}];
        ++t.runs;
        if (fit.models.empty()) {
            ++t.none;
        } else {
            const Agreement a = compare(fit.models.front(), o, s.grid, cellTolerance);
            const bool exact = fit.models.front().hasV() && a.wrongCells == 0 && a.holes == a.trueHoles && a.missedHoles == 0 &&
                               a.trueInliers == o.points.size() - static_cast<std::size_t>(outliers);
            t.exact += exact ? 1 : 0;
        }

        // 主張的範圍：雜訊 ≤ 0.1 格距（2.2 px）、缺件 ≤ 40%、離群點 ≤ 20。
        const bool claimed = noise <= 2.2 && missing <= 0.4 + 1e-9 && outliers <= 20;
        if (claimed) {
            QVERIFY2(!fit.models.empty(), "a lattice must be found inside the claimed range");
            const LatticeModel& m = fit.models.front();
            const Agreement a = compare(m, o, s.grid, cellTolerance);
            const QByteArray why = describe(m);
            QVERIFY2(m.hasV(), why.constData());
            QCOMPARE(a.wrongCells, std::size_t{0});
            QCOMPARE(a.trueInliers, o.points.size() - static_cast<std::size_t>(outliers));
            QCOMPARE(a.holes, a.trueHoles);  // 沒有假的缺格
            QCOMPARE(a.missedHoles, std::size_t{0});
            QVERIFY2(a.maxHoleErrorPx <= 0.5 + noise, why.constData());
            for (std::size_t k = 1; k < fit.models.size(); ++k) {
                QVERIFY2(LatticePredictor(fit.models[k], {-1e9, -1e9, 2e9, 2e9}).holes().size() <= 1,
                         ("an extra lattice made of outliers: " + describe(fit.models[k])).constData());
            }
            return;
        }
        // 範圍外：失敗要安全。假的缺格（不是真的缺件位置）最多 2 個，或者整組晶格被標成建議檢查。
        for (const LatticeModel& m : fit.models) {
            const Agreement a = compare(m, o, s.grid, cellTolerance);
            const std::size_t falseHoles = a.holes - a.trueHoles;
            const QByteArray why = describe(m) + " falseHoles=" + QByteArray::number(static_cast<qulonglong>(falseHoles));
            QVERIFY2(falseHoles <= 2 || m.suggestCheck, why.constData());
        }
    }
};

QTEST_APPLESS_MAIN(TstLatticeScenarios)
#include "tst_lattice_scenarios.moc"
