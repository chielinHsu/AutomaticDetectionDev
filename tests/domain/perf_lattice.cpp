// S05 的效能量測（label perf）：
//   T3        晶格擬合，1 000 點（預算 perf/budgets.json：p95 ≤ 2 ms）。量 fitAll（use case 用的那一個）。
//   L-*       其他形狀的 1 000 點（髒資料、一排、間距差 20 倍、兩個陣列、隨機點）與規模（4 900、10 000 點）：資訊性。
//   NMS-*     非極大值抑制，1 000 與 10 000 個框（約 25% 是位置差幾個像素的重複匹配）：資訊性。
//   INC-*／FULL-*  漸進重新擬合：每加一個點就重新解一次。INC ＝ 正規方程累加器（O(1)／點），
//             FULL ＝ 每次從頭把所有點加一遍（O(n)／點，總共 O(n²)）。證明 O(1) 的更新值得。
//
// 方法（docs/perf-test-plan.md）：熱機 5 次、正式量 30 次（FULL-10000 太慢：熱機 1 次、量 5 次），回報中位數與 p95；
// 結果寫到 perf/results/lattice_<時間>.json（含機器指紋）。只有 Release／RelWithDebInfo 把 T3 的預算當成失敗條件。
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QSysInfo>
#include <QThread>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <vector>

#include "../ui/MeasurementHygiene.h"
#include "LatticeTestSupport.h"
#include "domain/lattice/lattice_accumulator.h"
#include "domain/lattice/lattice_estimator.h"
#include "domain/lattice/non_max_suppression.h"

using namespace aoi;
using namespace aoi::test;
using Clock = std::chrono::steady_clock;

namespace {

double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const auto rank = static_cast<std::size_t>(std::ceil(p * static_cast<double>(v.size())));
    return v[std::min(v.size(), std::max<std::size_t>(1, rank)) - 1];
}

QString runCommand(const QString& program, const QStringList& args) {
    QProcess p;
    p.setWorkingDirectory(QStringLiteral(AOI_SOURCE_DIR));
    p.start(program, args);
    if (!p.waitForFinished(5000)) return {};
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

QJsonObject machineFingerprint(const QString& hygiene) {
    QJsonObject m;
    QSettings cpu(QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), QSettings::NativeFormat);
    m["cpu"] = cpu.value(QStringLiteral("ProcessorNameString"), QStringLiteral("unknown")).toString().trimmed();
    m["logicalCores"] = QThread::idealThreadCount();
#ifdef Q_OS_WIN
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) m["ramGB"] = std::round(static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024 * 1024) * 10) / 10;
#endif
    m["os"] = QSysInfo::prettyProductName() + QStringLiteral(" (kernel ") + QSysInfo::kernelVersion() + QLatin1Char(')');
    m["qt"] = QString::fromLatin1(qVersion());
#ifdef _MSC_FULL_VER
    m["compiler"] = QStringLiteral("MSVC %1").arg(_MSC_FULL_VER);
#endif
    m["buildType"] = QStringLiteral(AOI_BUILD_TYPE);
    const QString head = runCommand(QStringLiteral("git"), {QStringLiteral("rev-parse"), QStringLiteral("--short"), QStringLiteral("HEAD")});
    const QString dirty = runCommand(QStringLiteral("git"), {QStringLiteral("status"), QStringLiteral("--porcelain")});
    m["commit"] = head.isEmpty() ? QStringLiteral("unknown") : head + (dirty.isEmpty() ? QString() : QStringLiteral("+uncommitted"));
    m["machineName"] = QSysInfo::machineHostName();
    m["measurementHygiene"] = hygiene;
    return m;
}

double budgetOf(const QString& id) {
    QFile f(QStringLiteral(AOI_SOURCE_DIR "/perf/budgets.json"));
    if (!f.open(QIODevice::ReadOnly)) return -1.0;
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).object().value("budgets").toArray();
    for (const QJsonValue& v : arr) {
        if (v.toObject().value("id").toString() == id) return v.toObject().value("limit").toDouble(-1.0);
    }
    return -1.0;
}

// 不讓編譯器把結果丟掉（結果寫進 volatile）。
volatile double g_sink = 0.0;

// 計時 fn：熱機 warmup 次（丟掉），正式量 runs 次，回傳每次的毫秒數。
std::vector<double> timeRuns(const std::function<void()>& fn, int warmup, int runs) {
    for (int k = 0; k < warmup; ++k) fn();
    std::vector<double> out;
    out.reserve(static_cast<std::size_t>(runs));
    for (int k = 0; k < runs; ++k) {
        const auto t0 = Clock::now();
        fn();
        out.push_back(std::chrono::duration<double, std::milli>(Clock::now() - t0).count());
    }
    return out;
}

std::vector<Point2D> gridPoints(int cols, int rows, double pitchX, double pitchY, double angle, double noise, std::uint64_t seed,
                                double missing = 0.0, int outliers = 0) {
    Scenario s;
    s.grid.cols = cols;
    s.grid.rows = rows;
    s.grid.pitchX = pitchX;
    s.grid.pitchY = pitchY;
    s.grid.angleDeg = angle;
    s.grid.origin = {200.0, 200.0};
    s.noisePx = noise;
    s.missingFraction = missing;
    s.outliers = outliers;
    s.seed = seed;
    return generate(s).points;
}

// NMS 的輸入：rows × cols 個元件（3° 斜放），再加上 duplicates 個「同一個元件被找到兩次」（位置差 1–3 像素、分數較低）。
std::vector<ScoredBox> nmsBoxes(int cols, int rows, int duplicates, std::uint64_t seed) {
    Rng rng(seed);
    std::vector<ScoredBox> boxes;
    const double w = 24.0, h = 16.0, pitchX = 30.0, pitchY = 22.0;
    for (int c = 0; c < cols; ++c) {
        for (int r = 0; r < rows; ++r) {
            const Point2D p = rotate({c * pitchX, r * pitchY}, 3.0);
            boxes.push_back({{{100.0 + p.x + rng.symmetric(0.5), 100.0 + p.y + rng.symmetric(0.5), 3.0}, w, h}, 0.8 + 0.19 * rng.uniform()});
        }
    }
    const std::size_t originals = boxes.size();
    for (int k = 0; k < duplicates; ++k) {
        const ScoredBox& o = boxes[rng.below(originals)];
        ScoredBox d = o;
        d.rect.pose.x += rng.symmetric(3.0);
        d.rect.pose.y += rng.symmetric(3.0);
        d.score = o.score - 0.05 - 0.1 * rng.uniform();
        boxes.push_back(d);
    }
    // 打亂順序：NMS 自己排序，輸入順序不能幫它的忙。
    for (std::size_t k = boxes.size() - 1; k > 0; --k) std::swap(boxes[k], boxes[rng.below(k + 1)]);
    return boxes;
}

}  // namespace

class PerfLattice : public QObject {
    Q_OBJECT

    struct Metric {
        QString id;
        QString label;
        QString unit;
        std::vector<double> samples;
        double budget = -1.0;
        QString note;
    };
    std::vector<Metric> metrics_;
    QString hygiene_;

    const Metric& add(const QString& id, const QString& label, const QString& unit, std::vector<double> samples, double budget = -1.0,
                      const QString& note = {}) {
        Metric m{id, label, unit, std::move(samples), budget, note};
        qInfo().noquote() << QStringLiteral("%1  %2: median %3 %4, p95 %5 %4 (n=%6)%7")
                                 .arg(id, label)
                                 .arg(percentile(m.samples, 0.5), 0, 'g', 4)
                                 .arg(unit)
                                 .arg(percentile(m.samples, 0.95), 0, 'g', 4)
                                 .arg(m.samples.size())
                                 .arg(budget >= 0 ? QStringLiteral(" budget %1 %2").arg(budget).arg(unit) : QString());
        metrics_.push_back(std::move(m));
        return metrics_.back();
    }

    void measureFit(const QString& id, const QString& label, const std::vector<Point2D>& points, double budget = -1.0) {
        const LatticeEstimator estimator;
        std::size_t models = 0, inliers = 0;
        const std::vector<double> ms = timeRuns(
            [&] {
                const LatticeFit fit = estimator.fitAll(points);
                models = fit.models.size();
                inliers = fit.models.empty() ? 0 : fit.models.front().inliers.size();
                g_sink = g_sink + static_cast<double>(inliers);
            },
            5, 30);
        add(id, label, QStringLiteral("ms"), ms, budget,
            QStringLiteral("%1 points, %2 lattice(s), %3 inliers in the main one").arg(points.size()).arg(models).arg(inliers));
    }

private slots:
    void initTestCase() { hygiene_ = applyMeasurementHygiene(); }

    // ---- T3：1 000 點的晶格擬合 ----
    void t3_latticeFit1000() {
        const std::vector<Point2D> points = gridPoints(40, 25, 20.0, 16.0, 3.0, 0.5, 11);
        QCOMPARE(points.size(), std::size_t{1000});
        const double budget = budgetOf(QStringLiteral("T3"));
        QVERIFY2(budget > 0.0, "T3 must have a limit in perf/budgets.json");
        measureFit(QStringLiteral("T3"), QStringLiteral("lattice fitAll, 1000 points (40x25, tilt 3 deg, noise +-0.5 px)"), points, budget);
        const LatticeEstimator estimator;
        const std::vector<double> single = timeRuns([&] { g_sink = g_sink + (estimator.fit(points) ? 1.0 : 0.0); }, 5, 30);
        add(QStringLiteral("T3-fit"), QStringLiteral("lattice fit (main lattice only), same 1000 points"), QStringLiteral("ms"), single);
#ifdef AOI_PERF_ENFORCE
        const double p95 = percentile(metrics_[metrics_.size() - 2].samples, 0.95);
        QVERIFY2(p95 <= budget, qPrintable(QStringLiteral("T3 p95 %1 ms exceeds the budget %2 ms").arg(p95).arg(budget)));
#endif
    }

    // ---- 其他形狀與規模（資訊性） ----
    void otherShapesAndSizes() {
        std::vector<Point2D> messy = gridPoints(40, 25, 20.0, 16.0, 3.0, 0.5, 12, 0.1, 100);
        measureFit(QStringLiteral("L-1000-messy"), QStringLiteral("10% missing + 100 outliers (1000 points)"), messy);
        measureFit(QStringLiteral("L-1000-row"), QStringLiteral("one row of 1000 (pitch 12, tilt 3 deg)"),
                   gridPoints(1000, 1, 12.0, 12.0, 3.0, 0.3, 13));
        measureFit(QStringLiteral("L-1000-ratio20"), QStringLiteral("100x10, pitch 10 x 200"), gridPoints(100, 10, 10.0, 200.0, 0.0, 0.3, 14));
        std::vector<Point2D> two = gridPoints(25, 20, 20.0, 16.0, 3.0, 0.5, 15);
        Scenario second;
        second.grid.cols = 25;
        second.grid.rows = 20;
        second.grid.pitchX = 26.0;
        second.grid.pitchY = 18.0;
        second.grid.angleDeg = -8.0;
        second.grid.origin = {1400.0, 300.0};
        second.noisePx = 0.5;
        second.seed = 16;
        const std::vector<Point2D> b = generate(second).points;
        two.insert(two.end(), b.begin(), b.end());
        measureFit(QStringLiteral("L-1000-two"), QStringLiteral("two arrays of 500 (different pitch and angle)"), two);
        Rng rng(17);
        std::vector<Point2D> random;
        for (int k = 0; k < 1000; ++k) random.push_back({rng.uniform() * 1600.0, rng.uniform() * 1200.0});
        measureFit(QStringLiteral("L-1000-random"), QStringLiteral("1000 random points (no lattice: every u candidate is tried)"), random);
        measureFit(QStringLiteral("L-4900"), QStringLiteral("70x70, 3% missing (compare: S03 stand-in 97-134 ms at ~4900)"),
                   gridPoints(70, 70, 20.0, 16.0, 3.0, 0.5, 18, 0.03));
        measureFit(QStringLiteral("L-10000"), QStringLiteral("100x100, 3% missing"), gridPoints(100, 100, 20.0, 16.0, 3.0, 0.5, 19, 0.03));
    }

    // ---- NMS ----
    void nonMaxSuppression() {
        struct Case {
            const char* id;
            int cols, rows, duplicates;
        };
        for (const Case& c : {Case{"NMS-1000", 30, 25, 250}, Case{"NMS-10000", 100, 75, 2500}}) {
            const std::vector<ScoredBox> boxes = nmsBoxes(c.cols, c.rows, c.duplicates, 21);
            const NonMaxSuppression nms;
            std::size_t kept = 0;
            const std::vector<double> ms = timeRuns([&] { kept = nms.keep(boxes).size(); g_sink = g_sink + static_cast<double>(kept); }, 5, 30);
            QCOMPARE(kept, static_cast<std::size_t>(c.cols * c.rows));  // 重複的都被壓掉，原本的都留下
            add(QString::fromLatin1(c.id), QStringLiteral("NMS, %1 boxes (%2 duplicates, rotated 3 deg)").arg(boxes.size()).arg(c.duplicates),
                QStringLiteral("ms"), ms, -1.0, QStringLiteral("%1 kept").arg(kept));
        }
    }

    // ---- 漸進重新擬合：O(1) 的累加器 vs 每次從頭重算 ----
    void incrementalVersusFullRefit() {
        for (const int n : {100, 1000, 10000}) {
            const int cols = n >= 1000 ? 100 : 10;
            const int rows = n / cols;
            Scenario s;
            s.grid.cols = cols;
            s.grid.rows = rows;
            s.noisePx = 0.5;
            s.seed = 31;
            const Observed o = generate(s);
            std::vector<LatticeIndex> cells;
            for (std::size_t k = 0; k < o.points.size(); ++k) cells.push_back({o.col[k], o.row[k]});
            const Point2D ref = o.points.front();

            const auto incremental = [&] {
                LatticeAccumulator acc(ref, {0, 0});
                for (std::size_t k = 0; k < o.points.size(); ++k) {
                    acc.add(cells[k], o.points[k]);
                    if (const auto b = acc.solve()) g_sink = g_sink + b->u.x;
                }
            };
            const auto full = [&] {
                for (std::size_t k = 0; k < o.points.size(); ++k) {
                    LatticeAccumulator acc(ref, {0, 0});
                    for (std::size_t q = 0; q <= k; ++q) acc.add(cells[q], o.points[q]);
                    if (const auto b = acc.solve()) g_sink = g_sink + b->u.x;
                }
            };
            const bool slow = n >= 10000;
            const std::vector<double> inc = timeRuns(incremental, 5, 30);
            const std::vector<double> all = timeRuns(full, slow ? 1 : 5, slow ? 5 : 30);
            add(QStringLiteral("INC-%1").arg(n), QStringLiteral("refit after each of %1 points, O(1) accumulator (total)").arg(n),
                QStringLiteral("ms"), inc, -1.0, QStringLiteral("%1 us per point").arg(percentile(inc, 0.5) * 1000.0 / n, 0, 'g', 3));
            add(QStringLiteral("FULL-%1").arg(n), QStringLiteral("refit after each of %1 points, rebuilt from scratch (total)").arg(n),
                QStringLiteral("ms"), all, -1.0,
                QStringLiteral("%1 us per point%2").arg(percentile(all, 0.5) * 1000.0 / n, 0, 'g', 3).arg(slow ? QStringLiteral("; 1 warm-up, 5 runs") : QString()));
            // 這裡只量時間。O(1) 的更新算得對不對，由 tst_lattice_crosscheck（與 S00 頁面「每找到一個就全部重算」的結果比對）
            // 與 tst_lattice_parts（加點、拿掉點、殘差）驗證。
        }
    }

    void cleanupTestCase() {
        QJsonArray rows;
        for (const Metric& m : metrics_) {
            QJsonObject o;
            o["id"] = m.id;
            o["label"] = m.label;
            o["unit"] = m.unit;
            o["n"] = static_cast<int>(m.samples.size());
            o["median"] = percentile(m.samples, 0.5);
            o["p95"] = percentile(m.samples, 0.95);
            o["max"] = m.samples.empty() ? 0.0 : *std::max_element(m.samples.begin(), m.samples.end());
            if (m.budget >= 0) {
                o["budget"] = m.budget;
                o["pass"] = percentile(m.samples, 0.95) <= m.budget;
            }
            if (!m.note.isEmpty()) o["note"] = m.note;
            rows.append(o);
        }
        QJsonObject root;
        root["schema"] = 1;
        root["step"] = "S05";
        root["note"] = "Lattice estimation, prediction and NMS (domain, standard library only). Numbers compare only on this machine.";
        root["machine"] = machineFingerprint(hygiene_);
        root["metrics"] = rows;
        const QString dir = QStringLiteral(AOI_SOURCE_DIR "/perf/results");
        QDir().mkpath(dir);
        const QString path = dir + QStringLiteral("/lattice_") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")) + QStringLiteral(".json");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson());
            qInfo().noquote() << "wrote" << path;
        }
    }
};

QTEST_GUILESS_MAIN(PerfLattice)
#include "perf_lattice.moc"
