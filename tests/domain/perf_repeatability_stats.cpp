// R05.4 的效能量測（label perf；ADR-031 的 before／after）：重複性的樣本統計（domain/repeatability 的 summarize）。
//   RS-SUM-5x2812     703 Box × 4 指標 × 5 次量測：一份資料夾的分析對 summarize 的呼叫（逐次呼叫的成本：rust 建置每次跨 FFI）。
//   RS-SUM-30x2812    同上，30 次量測（perf_repeatability 的 RS-ANALYZE 那一份資料的大小）。
//   RS-SUM-1000000    一次 100 萬個值（迴圈本身：Welford 的平均與平方和、最小、最大）。
// 全部是資訊性指標（沒有預算）。方法和 perf_match_region 相同：熱機 5 次、正式量 30 次，回報中位數與 p95；
// 結果寫到 perf/results/repeatability_stats_<時間>.json（tools/perf-compare.mjs 的來源 repeatability_stats）。
// 為什麼是新的執行檔：perf_repeatability（tests/ui）在 AiInspTool/ 底下有原地建置的 .moc，加新的測試函式不會執行（ADR-031 的後續）；
// 而且它量的是 application 的 analyze（map 的建立占大部分），看不出 summarize 本身。
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <span>
#include <vector>

#include "../ui/MeasurementHygiene.h"
#include "LatticeTestSupport.h"
#include "domain/repeatability.h"

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

volatile double g_sink = 0.0;

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

// 像真實的量測：µm、小數三位、平均在 ±60 之間、每個 Box 的離散不同。
std::vector<double> measurements(std::size_t count, std::uint64_t seed) {
    Rng rng(seed);
    std::vector<double> v(count);
    double center = rng.symmetric(60.0), spread = 0.5 + rng.uniform() * 8.0;
    for (std::size_t i = 0; i < count; ++i) {
        if (i % 50 == 0) center = rng.symmetric(60.0), spread = 0.5 + rng.uniform() * 8.0;
        v[i] = std::round((center + rng.symmetric(spread)) * 1000.0) / 1000.0;
    }
    return v;
}

}  // namespace

class PerfRepeatabilityStats : public QObject {
    Q_OBJECT

    struct Metric {
        QString id;
        QString label;
        std::vector<double> samples;
        QString note;
    };
    std::vector<Metric> metrics_;
    QString hygiene_;

    void add(const QString& id, const QString& label, std::vector<double> samples, const QString& note = {}) {
        qInfo().noquote() << QStringLiteral("%1  %2: median %3 ms, p95 %4 ms (n=%5)%6")
                                 .arg(id, label)
                                 .arg(percentile(samples, 0.5), 0, 'g', 4)
                                 .arg(percentile(samples, 0.95), 0, 'g', 4)
                                 .arg(samples.size())
                                 .arg(note.isEmpty() ? QString() : QStringLiteral("  [%1]").arg(note));
        metrics_.push_back({id, label, std::move(samples), note});
    }

private slots:
    void initTestCase() { hygiene_ = applyMeasurementHygiene(); }

    void perBoxCalls() {
        constexpr std::size_t kCalls = 703 * 4;
        for (const std::size_t runs : {std::size_t(5), std::size_t(30)}) {
            const std::vector<double> values = measurements(kCalls * runs, 41 + runs);
            std::size_t exceeded = 0;
            const std::vector<double> ms = timeRuns(
                [&] {
                    exceeded = 0;
                    for (std::size_t c = 0; c < kCalls; ++c) {
                        const auto s = repeatability::summarize(std::span<const double>(values).subspan(c * runs, runs), runs, 200.0);
                        if (s.pt && *s.pt > 0.20) ++exceeded;
                    }
                    g_sink = g_sink + static_cast<double>(exceeded);
                },
                5, 30);
            add(QStringLiteral("RS-SUM-%1x%2").arg(runs).arg(kCalls),
                QStringLiteral("summarize, %1 calls of %2 values (703 Boxes x 4 metrics)").arg(kCalls).arg(runs), ms,
                QStringLiteral("%1 P/T over 20%").arg(exceeded));
        }
    }

    void oneLargeSpan() {
        const std::vector<double> values = measurements(1'000'000, 97);
        double deviation = 0.0;
        const std::vector<double> ms = timeRuns(
            [&] {
                const auto s = repeatability::summarize(values, values.size(), 350.0);
                deviation = s.deviation.value_or(-1.0);
                g_sink = g_sink + deviation;
            },
            5, 30);
        QVERIFY(deviation > 0.0);
        add(QStringLiteral("RS-SUM-1000000"), QStringLiteral("summarize, one call of 1 000 000 values"), ms,
            QStringLiteral("deviation %1").arg(deviation, 0, 'g', 6));
    }

    void cleanupTestCase() {
        QJsonArray rows;
        for (const Metric& m : metrics_) {
            QJsonObject o;
            o["id"] = m.id;
            o["label"] = m.label;
            o["unit"] = "ms";
            o["n"] = static_cast<int>(m.samples.size());
            o["median"] = percentile(m.samples, 0.5);
            o["p95"] = percentile(m.samples, 0.95);
            o["max"] = m.samples.empty() ? 0.0 : *std::max_element(m.samples.begin(), m.samples.end());
            if (!m.note.isEmpty()) o["note"] = m.note;
            rows.append(o);
        }
        QJsonObject root;
        root["schema"] = 1;
        root["step"] = "R05.4";
        root["domainImpl"] = AOI_DOMAIN_IMPL_NAME;  // ADR-031：cpp 或 rust（before／after）
        root["note"] = "Repeatability sample statistics (domain, standard library only). Numbers compare only on this machine.";
        root["hygiene"] = hygiene_;
        root["metrics"] = rows;
        const QString dir = QStringLiteral(AOI_SOURCE_DIR "/perf/results");
        QDir().mkpath(dir);
        const QString path = dir + QStringLiteral("/repeatability_stats_") +
                             QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")) + QStringLiteral(".json");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson());
            qInfo().noquote() << "wrote" << path;
        }
    }
};

QTEST_GUILESS_MAIN(PerfRepeatabilityStats)
#include "perf_repeatability_stats.moc"
