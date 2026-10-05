// S03 的效能量測（label perf）：
//   P1  PerfProbe／ScopedTimer 的單次成本（沒有 probe、probe 關閉、probe 開啟）
//   P2  工作提交 → 開始執行的延遲，以及 submit() 在呼叫端執行緒花多久
//   P3  取消旗標生效的延遲（cancel() → 工作看到旗標）
//   P4  use case 本身的成本（假 back end 沒有搜尋成本，所以量到的是擬合、指派與打包；不是真實搜尋時間）
//   另外把四組「真的跑出來的」候選（位置、分數、來源）寫進 JSON 的 demo 欄位，給 explainer.html 嵌入（不是量測，是範例輸出）。
//
// 方法：熱機後重複量測，回報中位數與 p95；結果寫到 perf/results/core_<時間>.json（含機器指紋）。
// 唯一的失敗條件是 S03 計畫寫的目標：「PerfProbe 關閉時單次成本 < 1 µs」，而且只在非 Debug 組態強制。
// 其餘都是資訊性的量測，數字見 docs/steps/S03-core/perf.md。
#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QSysInfo>
#include <QThread>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX  // windows.h 的 min／max 巨集會弄壞 std::min／std::max
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "../ui/MeasurementHygiene.h"
#include "CoreTestSupport.h"
#include "application/jobs/JobRunner.h"
#include "infrastructure/perf/PerfProbe.h"
#include "infrastructure/threading/ThreadPoolExecutor.h"

using namespace aoi;
using namespace aoi::test;
using Clock = std::chrono::steady_clock;

namespace {

double ms(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b - a).count(); }

double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const auto rank = static_cast<std::size_t>(std::ceil(p * static_cast<double>(v.size())));
    return v[std::min(v.size(), std::max<std::size_t>(1, rank)) - 1];
}

struct Metric {
    QString id;
    QString label;
    QString unit;
    std::vector<double> samples;
    QString note;
};

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

// 讓編譯器不能把整個迴圈消掉，也不能在編譯期看穿 probe 是不是空的。
IPerfProbe* volatile g_probe = nullptr;
volatile int g_sink = 0;

// 每次呼叫的平均奈秒數：固定次數的迴圈，重複 kReps 次，回傳每一次重複的結果。
template <typename Fn>
std::vector<double> nsPerCall(Fn fn, int iterations, int reps) {
    std::vector<double> perCall;
    for (int rep = 0; rep < reps + 1; ++rep) {  // 第一次是熱機，丟掉
        const auto t0 = Clock::now();
        for (int i = 0; i < iterations; ++i) fn();
        const auto t1 = Clock::now();
        if (rep > 0) perCall.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / iterations);
    }
    return perCall;
}

}  // namespace

class PerfCore : public QObject {
    Q_OBJECT

private:
    std::vector<Metric> metrics_;
    QJsonArray demoRuns_;
    QString hygiene_;

    void add(const QString& id, const QString& label, const QString& unit, std::vector<double> samples, const QString& note = {}) {
        Metric m{id, label, unit, std::move(samples), note};
        qInfo().noquote() << QStringLiteral("%1  %2: median %3 %4, p95 %5 %4 (n=%6)")
                                 .arg(id, label)
                                 .arg(percentile(m.samples, 0.5), 0, 'g', 4)
                                 .arg(unit)
                                 .arg(percentile(m.samples, 0.95), 0, 'g', 4)
                                 .arg(m.samples.size());
        metrics_.push_back(std::move(m));
    }

private slots:
    void initTestCase() { hygiene_ = applyMeasurementHygiene(); }

    // ---- P1：PerfProbe 單次成本 ----
    void p1_perfProbeCost() {
        constexpr int kIter = 2'000'000;
        constexpr int kReps = 9;

        g_probe = nullptr;
        const auto baseline = nsPerCall([] { g_sink = g_sink + 1; }, kIter, kReps);
        add("P1-baseline", "empty loop body (volatile increment)", "ns", baseline, "reference only");

        g_probe = nullptr;
        const auto none = nsPerCall([] { ScopedTimer t(g_probe, "x"); g_sink = g_sink + 1; }, kIter, kReps);
        add("P1-null", "ScopedTimer with a null probe", "ns", none, "includes the volatile loop body above");

        infra::PerfProbe off(65536, false);
        g_probe = &off;
        const auto disabled = nsPerCall([] { ScopedTimer t(g_probe, "x"); g_sink = g_sink + 1; }, kIter, kReps);
        add("P1-off", "ScopedTimer with a disabled PerfProbe", "ns", disabled, "includes the volatile loop body above");

        infra::PerfProbe on(65536, true);
        g_probe = &on;
        const auto enabled = nsPerCall([] { ScopedTimer t(g_probe, "x"); g_sink = g_sink + 1; }, 500'000, kReps);
        add("P1-on", "ScopedTimer with an enabled PerfProbe (ring buffer, mutex)", "ns", enabled, "two clock reads plus one record()");

        const auto clockPair = nsPerCall([] {
            const auto a = Clock::now();
            const auto b = Clock::now();
            g_sink = g_sink + static_cast<int>((b - a).count() & 1);
        }, 500'000, kReps);
        add("P1-clock", "two steady_clock::now() calls", "ns", clockPair, "the floor for any enabled timer");

        g_probe = nullptr;
        // 關閉時的額外成本 ＝ 關閉的中位數 − 空迴圈的中位數。目標 < 1 µs（S03 計畫；實測才是結論）。
        const double extra = percentile(disabled, 0.5) - percentile(baseline, 0.5);
        qInfo().noquote() << QStringLiteral("P1  disabled-probe extra cost over the empty loop: %1 ns per call (target < 1000 ns)").arg(extra, 0, 'f', 2);
#ifdef AOI_PERF_ENFORCE
        QVERIFY2(percentile(disabled, 0.95) < 1000.0, "a disabled PerfProbe must cost under 1 microsecond per call");
        QVERIFY2(percentile(none, 0.95) < 1000.0, "a null probe must cost under 1 microsecond per call");
#endif
    }

    // ---- P2：工作提交 → 開始執行 ----
    void p2_jobSubmitLatency() {
        infra::ThreadPoolExecutor executor(2);
        JobRunner runner(executor);
        std::vector<double> queued;
        std::vector<double> submitCall;
        std::mutex mutex;
        std::atomic<int> done{0};
        constexpr int kRuns = 300;
        for (int i = 0; i < kRuns; ++i) {
            const int expectedDone = done.load() + 1;
            const auto t0 = Clock::now();
            runner.submit([](const CancelToken&) {}, [&](const JobReport& r) {
                std::lock_guard lock(mutex);
                queued.push_back(r.queuedMs);
                ++done;
            });
            submitCall.push_back(ms(t0, Clock::now()));
            while (done.load() < expectedDone) std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));  // 讓 worker 回到睡眠：量的是「喚醒」，不是「一直在忙」
        }
        queued.erase(queued.begin(), queued.begin() + 20);  // 前 20 次熱機
        submitCall.erase(submitCall.begin(), submitCall.begin() + 20);
        add("P2-queue", "job submit → start of work (idle pool, 2 workers)", "ms", queued, "worker wake-up included");
        add("P2-submit", "submit() duration on the calling thread", "ms", submitCall, "what the UI thread pays");
    }

    // ---- P3：取消旗標生效 ----
    void p3_cancelLatency() {
        for (const int pollMs : {0, 1}) {
            infra::ThreadPoolExecutor executor(1);
            JobRunner runner(executor);
            std::vector<double> latencies;
            constexpr int kRuns = 100;
            for (int i = 0; i < kRuns; ++i) {
                std::atomic<bool> started{false};
                std::atomic<bool> exited{false};
                std::atomic<std::int64_t> exitTicks{0};
                runner.submit(
                    [&](const CancelToken& token) {
                        started = true;
                        while (!token.cancelled()) {
                            if (pollMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
                        }
                        exitTicks = Clock::now().time_since_epoch().count();
                        exited = true;
                    },
                    {});
                while (!started) std::this_thread::yield();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));  // 讓工作確實在迴圈裡
                const auto t0 = Clock::now();
                runner.cancel();
                while (!exited) std::this_thread::yield();
                const auto exitTime = Clock::time_point(Clock::duration(exitTicks.load()));
                latencies.push_back(ms(t0, exitTime));
            }
            latencies.erase(latencies.begin(), latencies.begin() + 10);
            add(pollMs == 0 ? "P3-spin" : "P3-poll1ms",
                pollMs == 0 ? "cancel() → work sees the flag (work spins on the flag)" : "cancel() → work sees the flag (work checks every 1 ms, like the fake back end)",
                "ms", latencies, pollMs == 0 ? "flag visibility only" : "polling interval dominates; Windows sleep granularity applies");
        }
    }

    // ---- P4：use case 本身的成本（假 back end） ----
    void p4_useCaseOverhead() {
        const TestImage image(2592, 1944);  // 5 MP
        struct Scenario {
            const char* id;
            const char* label;
            fake::FakeSpec spec;
            Rect sample;
        };
        std::vector<Scenario> scenarios;
        {
            fake::FakeSpec small;  // 約 225 個零件（T5 的「200 個零件」）
            small.halfCols = 7;
            small.halfRows = 7;
            small.pitchFactorX = 7.5;
            small.pitchFactorY = 8.0;
            small.weakFraction = 0.05;
            small.missingFraction = 0.03;
            small.seed = 5;
            scenarios.push_back({"P4-225", "use case on a 5 MP image, ~225 parts", small, {1284.0, 964.0, 24.0, 16.0}});
            fake::FakeSpec large;  // 約 4 900 個零件
            large.halfCols = 40;
            large.halfRows = 30;
            large.pitchFactorX = 1.25;
            large.pitchFactorY = 1.25;
            large.weakFraction = 0.05;
            large.missingFraction = 0.03;
            large.seed = 5;
            scenarios.push_back({"P4-4900", "use case on a 5 MP image, ~4 900 parts", large, {1284.0, 964.0, 24.0, 16.0}});
        }
        for (const Scenario& s : scenarios) {
            for (const bool probeOn : {false, true}) {
                CountingBuilder builder(s.spec);
                CountingSearcher searcher(s.spec);
                infra::PerfProbe probe(4096, probeOn);
                GenerateObjectsFromSample useCase(builder, searcher, &probe);
                const SampleRequest request = makeRequest(image, s.sample);
                std::vector<double> total, fit, verify, seed;
                std::size_t candidates = 0;
                for (int i = 0; i < 23; ++i) {
                    const auto t0 = Clock::now();
                    const Result<SampleProposal> r = useCase.run(request, CancelToken());
                    const double elapsed = ms(t0, Clock::now());
                    QVERIFY(r.ok());
                    candidates = r.value().candidates.size();
                    probe.clear();
                    if (i < 3) continue;  // 熱機
                    total.push_back(elapsed);
                    seed.push_back(r.value().timings.seedMs);
                    fit.push_back(r.value().timings.fitMs);
                    verify.push_back(r.value().timings.verifyMs);
                }
                const QString suffix = probeOn ? QStringLiteral(" (probe on)") : QStringLiteral(" (probe off)");
                const QString tag = probeOn ? QStringLiteral("-on") : QStringLiteral("-off");
                const QString note = QStringLiteral("%1 candidates; the fake search costs almost nothing, so this is the use case's own cost").arg(candidates);
                add(QString::fromLatin1(s.id) + tag, QString::fromLatin1(s.label) + suffix, "ms", total, note);
                if (!probeOn) {
                    add(QString::fromLatin1(s.id) + "-seed", QString::fromLatin1(s.label) + ": seed stage", "ms", seed);
                    add(QString::fromLatin1(s.id) + "-fit", QString::fromLatin1(s.label) + ": fit stage (simple stand-in)", "ms", fit);
                    add(QString::fromLatin1(s.id) + "-verify", QString::fromLatin1(s.label) + ": verify stage", "ms", verify);
                }
            }
        }
    }

    // ---- 範例輸出：四組真的跑出來的候選，給 explainer.html 用 ----
    void p5_demoRunsForTheExplainer() {
        const TestImage image(800, 600);
        const Rect sample{380.0, 285.0, 40.0, 30.0};
        struct Variant {
            const char* id;
            const char* label;
            fake::FakeSpec spec;
        };
        std::vector<Variant> variants;
        {
            fake::FakeSpec base;
            base.halfCols = 4;
            base.halfRows = 2;
            variants.push_back({"clean", "clean grid: every part found by the seed search", base});
            fake::FakeSpec weak = base;
            weak.weakFraction = 0.25;
            weak.seed = 7;
            variants.push_back({"weak", "25% weak parts: only the window search finds them", weak});
            fake::FakeSpec missing = base;
            missing.missingFraction = 0.2;
            missing.seed = 3;
            variants.push_back({"missing", "20% missing parts: the grid predicts them (inferred)", missing});
            fake::FakeSpec mixed = base;
            mixed.gridAngleDeg = 3.0;
            mixed.jitterPx = 0.8;
            mixed.weakFraction = 0.12;
            mixed.missingFraction = 0.12;
            mixed.seed = 11;
            variants.push_back({"mixed", "rotated 3 degrees, noisy, weak and missing parts together", mixed});
        }
        for (const Variant& v : variants) {
            CountingBuilder builder(v.spec);
            CountingSearcher searcher(v.spec);
            GenerateObjectsFromSample useCase(builder, searcher);
            const Result<SampleProposal> r = useCase.run(makeRequest(image, sample), CancelToken());
            QVERIFY(r.ok());
            QJsonArray rows;
            for (const Candidate& c : r.value().candidates) {
                const char* source = c.source == CandidateSource::Sample ? "sample" : c.source == CandidateSource::Seed ? "seed" : c.source == CandidateSource::Verified ? "verified" : "predicted";
                QJsonObject o;
                o["source"] = QString::fromLatin1(source);
                o["x"] = std::round(c.pose.x * 100.0) / 100.0;
                o["y"] = std::round(c.pose.y * 100.0) / 100.0;
                o["score"] = c.hasScore() ? QJsonValue(std::round(c.score * 1000.0) / 1000.0) : QJsonValue();
                rows.append(o);
            }
            QJsonObject run;
            run["id"] = QString::fromLatin1(v.id);
            run["label"] = QString::fromLatin1(v.label);
            run["image"] = QJsonArray{800, 600};
            run["part"] = QJsonArray{r.value().partSize.w, r.value().partSize.h};
            run["candidates"] = rows;
            demoRuns_.append(run);
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
            if (!m.note.isEmpty()) o["note"] = m.note;
            rows.append(o);
        }
        QJsonObject root;
        root["schema"] = 1;
        root["step"] = "S03";
        root["note"] = "Core baseline recorded in S03. Numbers compare only on this machine. The fake back end has no search cost: P4 measures the use case itself, not a real search.";
        root["machine"] = machineFingerprint(hygiene_);
        root["metrics"] = rows;
        root["demo"] = demoRuns_;

        const QString dir = QStringLiteral(AOI_SOURCE_DIR "/perf/results");
        QDir().mkpath(dir);
        const QString path = dir + QStringLiteral("/core_") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")) + QStringLiteral(".json");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson());
            qInfo().noquote() << "wrote" << path;
        }
    }
};

QTEST_GUILESS_MAIN(PerfCore)
#include "perf_core.moc"
