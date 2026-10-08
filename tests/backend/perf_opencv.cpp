// S07 的效能量測（label perf）：OpenCV back end。預算在 perf/budgets.json。
//   T1     範本建立，64×32 的樣本、±5°
//   T2a    種子搜尋（整張影像），5 MP（2592×1944）、64×32、±5°、約 870 個元件
//   T4     視窗驗證，200 個預測的位置（視窗 ＝ 範本 × 1.5，分數下限 0.30，和 use case 一樣）
//   T6a    召回率、T6b 位置誤差：合成板子組（5 MP，8 種情況），use case 端到端
//   A-*    角度範圍對種子搜尋時間（0、±2、±5、±10、±20、±45、±90、±180）
//   S-*    影像大小（1、5、12、25 MP）；P-* 樣本大小（32×16、64×32、128×128）
//   TH-*   OpenCV 的執行緒數（1、2、4、8、核心數 − 1）
//   UC-*   use case 的各階段（5 MP、10% 缺件）
//   MEM-*  峰值記憶體（背景執行緒每 1 ms 讀一次行程的 private bytes，取最大值減開始時的值）
//   T7-N   樣本庫比對（流程 B，OpenCvLibraryMatcher），N ＝ 1、10、100、1000 個候選類型
// 方法（docs/perf-test-plan.md）：熱機 5 次、正式量 30 次（慢的項目較少，note 有寫），中位數與 p95；
// 結果寫到 perf/results/opencv_<時間>.json（含機器指紋與曲線）。時間的預算只記錄是否通過，不讓測試失敗
// （S07 校準預算，見 perf.md）；準確度（T6）的預算是失敗條件。
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QSysInfo>
#include <QThread>
#include <QtTest>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <thread>
#include <vector>

#include "../ui/MeasurementHygiene.h"  // 會 include <windows.h>，所以 psapi.h 在它後面
#include <psapi.h>

#include "BackendTestSupport.h"
#include "app/CompositionRoot.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "infrastructure/backend_opencv/OpenCvBackend.h"
#include "infrastructure/backend_opencv/OpenCvLibraryMatcher.h"
#include "infrastructure/backend_opencv/OpenCvPatternSearcher.h"
#include "infrastructure/backend_opencv/OpenCvTemplateBuilder.h"
#include "synthetic_board.h"

using namespace aoi;
using namespace aoi::infra;
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
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) m["ramGB"] = std::round(static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024 * 1024) * 10) / 10;
    m["os"] = QSysInfo::prettyProductName() + QStringLiteral(" (kernel ") + QSysInfo::kernelVersion() + QLatin1Char(')');
    m["qt"] = QString::fromLatin1(qVersion());
    m["opencv"] = QStringLiteral(CV_VERSION);
    m["opencvThreads"] = cv::getNumThreads();
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

// 峰值記憶體：背景執行緒每 1 ms 讀一次行程的 private bytes。Windows 的峰值工作集不能歸零，所以自己取樣。
class PeakSampler {
public:
    PeakSampler() : baseline_(privateBytes()), peak_(baseline_) {
        thread_ = std::thread([this] {
            while (!stop_.load()) {
                const std::size_t now = privateBytes();
                std::size_t seen = peak_.load();
                while (now > seen && !peak_.compare_exchange_weak(seen, now)) {
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    ~PeakSampler() { finish(); }
    double finish() {
        if (thread_.joinable()) {
            stop_.store(true);
            thread_.join();
        }
        return static_cast<double>(peak_.load() - std::min(peak_.load(), baseline_)) / (1024.0 * 1024.0);
    }

private:
    static std::size_t privateBytes() {
        PROCESS_MEMORY_COUNTERS_EX pmc{};
        if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) return 0;
        return pmc.PrivateUsage;
    }
    std::size_t baseline_;
    std::atomic<std::size_t> peak_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

// 64×32 的樣本：主體 56×24 ＋ 每邊 4 像素。
SyntheticBoardSpec boardSpec(int width, int height, double bodyW = 56.0, double bodyH = 24.0) {
    SyntheticBoardSpec s;
    s.width = width;
    s.height = height;
    s.bodyWidthPx = bodyW;
    s.bodyHeightPx = bodyH;
    s.pitchXPx = std::max(80.0, bodyW * 1.5);
    s.pitchYPx = std::max(60.0, bodyH * 2.2);
    s.originXPx = 120.0;
    s.originYPx = 110.0;
    s.cols = static_cast<int>((width - 2 * s.originXPx) / s.pitchXPx) + 1;
    s.rows = static_cast<int>((height - 2 * s.originYPx) / s.pitchYPx) + 1;
    s.noiseAmplitude = 4;
    s.blurRadius = 1;
    s.seed = 17;
    return s;
}

SearchParams anglesParams(double halfRange, double minScore = 0.70) {
    SearchParams p;
    p.minScore = minScore;
    p.angleMinDeg = -halfRange;
    p.angleMaxDeg = halfRange;
    return p;
}

struct Metric {
    QString id;
    QString label;
    QString unit;
    std::vector<double> samples;
    double budget = -1.0;
    bool higherIsBetter = false;
    QString note;
    double median() const { return percentile(samples, 0.5); }
    double p95() const { return higherIsBetter ? percentile(samples, 0.05) : percentile(samples, 0.95); }
    bool pass() const { return budget < 0 || (higherIsBetter ? p95() >= budget : p95() <= budget); }
};

}  // namespace

class PerfOpenCv : public QObject {
    Q_OBJECT

private:
    QString hygiene_;
    std::vector<Metric> metrics_;
    QJsonObject curves_;

    void add(const QString& id, const QString& label, const QString& unit, std::vector<double> samples, double budget = -1.0,
             const QString& note = {}, bool higherIsBetter = false) {
        Metric m{id, label, unit, std::move(samples), budget, higherIsBetter, note};
        qInfo().noquote() << QStringLiteral("%1 %2: median %3 %4, p95 %5%6%7")
                                 .arg(id, label)
                                 .arg(m.median(), 0, 'f', 3)
                                 .arg(unit)
                                 .arg(m.p95(), 0, 'f', 3)
                                 .arg(budget >= 0 ? QStringLiteral(" (budget %1, %2)").arg(budget).arg(m.pass() ? "pass" : "OVER") : QString())
                                 .arg(note.isEmpty() ? QString() : QStringLiteral(" — ") + note);
        metrics_.push_back(std::move(m));
    }

    // 一次種子搜尋的時間（毫秒），與最後一次的內部數字。
    std::vector<double> timeSeed(const SyntheticBoard& board, const ITemplate& t, const SearchParams& p, int warmup, int runs,
                                 OpenCvSearchStats* stats = nullptr) {
        OpenCvPatternSearcher searcher;
        return timeRuns(
            [&] {
                const Result<std::vector<Match>> r = searcher.find(board.image, t, SearchRegion::wholeImage(), p, CancelToken(), stats);
                QVERIFY(r.ok());
            },
            warmup, runs);
    }

private slots:
    void initTestCase() {
        hygiene_ = applyMeasurementHygiene();
        setOpenCvThreads(static_cast<int>(app::workerThreads()));  // 和產品一樣（組裝根的政策）
    }

    void t1TemplateBuild() {
        const SyntheticBoard board = makeSyntheticBoard(boardSpec(2592, 1944));
        const Rect sample = sampleBoxAround(board, middlePresentIndex(board));
        QCOMPARE(sample.w, 64.0);
        QCOMPARE(sample.h, 32.0);
        OpenCvTemplateBuilder builder;
        const std::vector<double> t = timeRuns([&] { QVERIFY(builder.build(board.image, sample, anglesParams(5.0), CancelToken()).ok()); }, 5, 30);
        add(QStringLiteral("T1"), QStringLiteral("template build, 64x32 sample, +-5 deg"), QStringLiteral("ms"), t, budgetOf(QStringLiteral("T1")));
        const std::vector<double> wide = timeRuns([&] { QVERIFY(builder.build(board.image, sample, anglesParams(180.0), CancelToken()).ok()); }, 2, 10);
        add(QStringLiteral("T1-360"), QStringLiteral("template build, 64x32 sample, +-180 deg"), QStringLiteral("ms"), wide, -1.0,
            QStringLiteral("2 warm-up, 10 runs"));
    }

    void t2aSeedSearchFiveMegapixels() {
        const SyntheticBoard board = makeSyntheticBoard(boardSpec(2592, 1944));
        const TemplatePtr t = OpenCvTemplateBuilder().build(board.image, sampleBoxAround(board, middlePresentIndex(board)),
                                                            anglesParams(5.0), CancelToken()).value();
        OpenCvSearchStats stats;
        const std::vector<double> times = timeSeed(board, *t, anglesParams(5.0), 5, 30, &stats);
        add(QStringLiteral("T2a"), QStringLiteral("seed search, 5 MP, 64x32, +-5 deg, %1 parts").arg(board.presentCount()),
            QStringLiteral("ms"), times, budgetOf(QStringLiteral("T2a")),
            QStringLiteral("last run: convert %1, pyramid %2, coarse %3 (%4 angles at level %5, %6 candidates), refine %7, nms %8 ms; %9 matches")
                .arg(stats.convertMs, 0, 'f', 2).arg(stats.pyramidMs, 0, 'f', 2).arg(stats.coarseMs, 0, 'f', 2)
                .arg(stats.topAngles).arg(stats.topLevel).arg(stats.coarseCandidates).arg(stats.refineMs, 0, 'f', 2)
                .arg(stats.nmsMs, 0, 'f', 2).arg(stats.matches));
        QCOMPARE(stats.matches, board.presentCount());
    }

    void t4WindowedVerify() {
        const SyntheticBoard board = makeSyntheticBoard(boardSpec(2592, 1944));
        const TemplatePtr t = OpenCvTemplateBuilder().build(board.image, sampleBoxAround(board, middlePresentIndex(board)),
                                                            anglesParams(5.0), CancelToken()).value();
        std::vector<Rect> windows;
        for (std::size_t k = 0; k < board.parts.size() && windows.size() < 200; k += 4) {
            windows.push_back(rectFromCenter(board.parts[k].pose.position(), {t->size().w * 1.5, t->size().h * 1.5}));
        }
        QCOMPARE(windows.size(), std::size_t{200});
        OpenCvPatternSearcher searcher;
        OpenCvSearchStats stats;
        std::size_t found = 0;
        const std::vector<double> times = timeRuns(
            [&] {
                const Result<std::vector<Match>> r =
                    searcher.find(board.image, *t, SearchRegion::inWindows(windows), anglesParams(5.0, 0.30), CancelToken(), &stats);
                QVERIFY(r.ok());
                found = r.value().size();
            },
            5, 30);
        add(QStringLiteral("T4"), QStringLiteral("windowed verify, 200 windows (1.5x template), floor 0.30"), QStringLiteral("ms"), times,
            budgetOf(QStringLiteral("T4")), QStringLiteral("%1 matches; per-window sums: convert %2, pyramid %3, coarse %4, refine %5 ms")
                                                .arg(found).arg(stats.convertMs, 0, 'f', 2).arg(stats.pyramidMs, 0, 'f', 2)
                                                .arg(stats.coarseMs, 0, 'f', 2).arg(stats.refineMs, 0, 'f', 2));
        QVERIFY(found >= 200);
    }

    // T6：合成板子組，use case 端到端（組裝根的 OpenCV back end）。
    void t6AccuracyOnTheSyntheticSet() {
        Result<app::CoreServices> core = app::composeCore(app::makeBackendRegistry(), "opencv", false, app::ExecutorKind::StdThreads);
        QVERIFY(core.ok());
        struct Case {
            const char* name;
            std::function<void(SyntheticBoardSpec&)> tweak;
        };
        const std::vector<Case> cases = {
            {"clean", [](SyntheticBoardSpec& s) { s.noiseAmplitude = 0; s.blurRadius = 0; }},
            {"noise 4, blur 1", [](SyntheticBoardSpec&) {}},
            {"noise 12, blur 2", [](SyntheticBoardSpec& s) { s.noiseAmplitude = 12; s.blurRadius = 2; }},
            {"5% missing", [](SyntheticBoardSpec& s) { s.missingFraction = 0.05; }},
            {"10% glare", [](SyntheticBoardSpec& s) { s.glareFraction = 0.10; }},
            {"tilt 3 deg", [](SyntheticBoardSpec& s) { s.tiltDeg = 3.0; }},
            {"jitter 3 px", [](SyntheticBoardSpec& s) { s.positionJitterPx = 3.0; }},
            {"12 bit", [](SyntheticBoardSpec& s) { s.format = PixelFormat::Gray16; s.grayBits = 12; }},
        };
        std::size_t present = 0;
        std::size_t found = 0;
        std::vector<double> errors;
        QJsonArray rows;
        for (const Case& c : cases) {
            SyntheticBoardSpec spec = boardSpec(2592, 1944);
            c.tweak(spec);
            const SyntheticBoard board = makeSyntheticBoard(spec);
            SampleRequest request;
            request.image = board.image;
            request.sample = sampleBoxAround(board, middlePresentIndex(board));
            const Result<SampleProposal> r = core.value().engine->generate->run(request, CancelToken());
            QVERIFY(r.ok());
            // 參考點：樣本框的中心（見 tst_opencv_end_to_end 的 sampleOffset）。
            const Candidate& s = r.value().candidates.front();
            Point2D offset{};
            double nearestSample = std::numeric_limits<double>::max();
            for (const SyntheticPart& p : board.parts) {
                const double d = distance(s.pose.position(), p.pose.position());
                if (p.present && d < nearestSample) {
                    nearestSample = d;
                    offset = {s.pose.x - p.pose.x, s.pose.y - p.pose.y};
                }
            }
            std::size_t casePresent = 0;
            std::size_t caseFound = 0;
            double caseMax = 0.0;
            for (const SyntheticPart& p : board.parts) {
                if (!p.present) continue;
                ++casePresent;
                const Point2D expected{p.pose.x + offset.x, p.pose.y + offset.y};
                double best = std::numeric_limits<double>::max();
                for (const Candidate& cand : r.value().candidates) {
                    if (cand.hasScore()) best = std::min(best, distance(cand.pose.position(), expected));
                }
                if (best <= 3.0) {
                    ++caseFound;
                    errors.push_back(best);
                    caseMax = std::max(caseMax, best);
                }
            }
            present += casePresent;
            found += caseFound;
            qInfo("T6 %-16s present %zu found %zu max error %.3f px, total %.1f ms", c.name, casePresent, caseFound, caseMax,
                  r.value().timings.totalMs);
            QJsonObject row;
            row["case"] = QString::fromLatin1(c.name);
            row["present"] = static_cast<int>(casePresent);
            row["found"] = static_cast<int>(caseFound);
            row["maxErrorPx"] = std::round(caseMax * 1000.0) / 1000.0;
            rows.append(row);
        }
        curves_["t6Cases"] = rows;
        const double recall = 100.0 * static_cast<double>(found) / static_cast<double>(present);
        add(QStringLiteral("T6a"), QStringLiteral("recall on the synthetic set (8 boards, 5 MP)"), QStringLiteral("percent"), {recall},
            budgetOf(QStringLiteral("T6a")), QStringLiteral("%1 of %2 parts").arg(found).arg(present), true);
        add(QStringLiteral("T6b"), QStringLiteral("position error on the synthetic set"), QStringLiteral("px"), errors,
            budgetOf(QStringLiteral("T6b")), QStringLiteral("p95 of all found parts; max %1 px").arg(*std::max_element(errors.begin(), errors.end()), 0, 'f', 3));
        QVERIFY2(recall >= budgetOf(QStringLiteral("T6a")), qPrintable(QString::number(recall)));
        QVERIFY2(percentile(errors, 0.95) <= budgetOf(QStringLiteral("T6b")), qPrintable(QString::number(percentile(errors, 0.95))));
    }

    void angleRangeCurve() {
        const SyntheticBoard board = makeSyntheticBoard(boardSpec(2592, 1944));
        const Rect sample = sampleBoxAround(board, middlePresentIndex(board));
        QJsonArray points;
        for (const double half : {0.0, 2.0, 5.0, 10.0, 20.0, 45.0, 90.0, 180.0}) {
            const TemplatePtr t = OpenCvTemplateBuilder().build(board.image, sample, anglesParams(half), CancelToken()).value();
            OpenCvSearchStats stats;
            const std::vector<double> times = timeSeed(board, *t, anglesParams(half), 1, 5, &stats);
            add(QStringLiteral("A-%1").arg(half), QStringLiteral("seed search, 5 MP, 64x32, +-%1 deg").arg(half), QStringLiteral("ms"), times,
                -1.0, QStringLiteral("1 warm-up, 5 runs; %1 angles at level %2").arg(stats.topAngles).arg(stats.topLevel));
            QJsonObject p;
            p["halfRangeDeg"] = half;
            p["medianMs"] = std::round(percentile(times, 0.5) * 10.0) / 10.0;
            p["topAngles"] = static_cast<int>(stats.topAngles);
            points.append(p);
        }
        curves_["angleRange"] = points;
    }

    void imageSizeCurve() {
        QJsonArray points;
        struct Size { int w; int h; const char* name; };
        for (const Size s : {Size{1280, 800, "1 MP"}, Size{2592, 1944, "5 MP"}, Size{4000, 3000, "12 MP"}, Size{5800, 4350, "25 MP"}}) {
            const SyntheticBoard board = makeSyntheticBoard(boardSpec(s.w, s.h));
            const TemplatePtr t = OpenCvTemplateBuilder().build(board.image, sampleBoxAround(board, middlePresentIndex(board)),
                                                                anglesParams(5.0), CancelToken()).value();
            OpenCvSearchStats stats;
            const std::vector<double> times = timeSeed(board, *t, anglesParams(5.0), 1, 5, &stats);
            add(QStringLiteral("S-%1").arg(QString::fromLatin1(s.name).remove(QLatin1Char(' '))),
                QStringLiteral("seed search, %1 (%2x%3), 64x32, +-5 deg, %4 parts").arg(QString::fromLatin1(s.name)).arg(s.w).arg(s.h).arg(board.presentCount()),
                QStringLiteral("ms"), times, -1.0, QStringLiteral("1 warm-up, 5 runs"));
            QJsonObject p;
            p["megapixels"] = std::round(s.w * s.h / 1e5) / 10.0;
            p["parts"] = static_cast<int>(board.presentCount());
            p["medianMs"] = std::round(percentile(times, 0.5) * 10.0) / 10.0;
            points.append(p);
        }
        curves_["imageSize"] = points;
    }

    void sampleSizeCurve() {
        QJsonArray points;
        struct Body { double w; double h; };
        for (const Body b : {Body{24, 8}, Body{56, 24}, Body{120, 120}}) {
            const SyntheticBoard board = makeSyntheticBoard(boardSpec(2592, 1944, b.w, b.h));
            const Rect sample = sampleBoxAround(board, middlePresentIndex(board));
            const TemplatePtr t = OpenCvTemplateBuilder().build(board.image, sample, anglesParams(5.0), CancelToken()).value();
            OpenCvSearchStats stats;
            const std::vector<double> times = timeSeed(board, *t, anglesParams(5.0), 1, 5, &stats);
            const QString name = QStringLiteral("%1x%2").arg(sample.w).arg(sample.h);
            add(QStringLiteral("P-%1").arg(name), QStringLiteral("seed search, 5 MP, %1 sample, +-5 deg, %2 parts").arg(name).arg(board.presentCount()),
                QStringLiteral("ms"), times, -1.0, QStringLiteral("1 warm-up, 5 runs; top level %1").arg(stats.topLevel));
            QJsonObject p;
            p["sample"] = name;
            p["parts"] = static_cast<int>(board.presentCount());
            p["topLevel"] = stats.topLevel;
            p["medianMs"] = std::round(percentile(times, 0.5) * 10.0) / 10.0;
            points.append(p);
        }
        curves_["sampleSize"] = points;
    }

    void threadCount() {
        const SyntheticBoard board = makeSyntheticBoard(boardSpec(2592, 1944));
        const TemplatePtr t = OpenCvTemplateBuilder().build(board.image, sampleBoxAround(board, middlePresentIndex(board)),
                                                            anglesParams(5.0), CancelToken()).value();
        const int policy = static_cast<int>(app::workerThreads());
        QJsonArray points;
        for (const int n : {1, 2, 4, 8, policy}) {
            setOpenCvThreads(n);
            const std::vector<double> times = timeSeed(board, *t, anglesParams(5.0), 2, 10);
            add(QStringLiteral("TH-%1").arg(n), QStringLiteral("seed search, 5 MP, OpenCV threads %1").arg(n), QStringLiteral("ms"), times, -1.0,
                QStringLiteral("2 warm-up, 10 runs"));
            QJsonObject p;
            p["threads"] = n;
            p["medianMs"] = std::round(percentile(times, 0.5) * 10.0) / 10.0;
            points.append(p);
        }
        setOpenCvThreads(policy);
        curves_["threads"] = points;
    }

    void useCaseStages() {
        Result<app::CoreServices> core = app::composeCore(app::makeBackendRegistry(), "opencv", false, app::ExecutorKind::StdThreads);
        QVERIFY(core.ok());
        SyntheticBoardSpec spec = boardSpec(2592, 1944);
        spec.missingFraction = 0.10;
        const SyntheticBoard board = makeSyntheticBoard(spec);
        SampleRequest request;
        request.image = board.image;
        request.sample = sampleBoxAround(board, middlePresentIndex(board));
        std::vector<double> stage[5];
        std::size_t holes = 0;
        for (int k = 0; k < 25; ++k) {
            const Result<SampleProposal> r = core.value().engine->generate->run(request, CancelToken());
            QVERIFY(r.ok());
            if (k < 5) continue;
            const StageTimings& s = r.value().timings;
            stage[0].push_back(s.templateMs);
            stage[1].push_back(s.seedMs);
            stage[2].push_back(s.fitMs);
            stage[3].push_back(s.verifyMs);
            stage[4].push_back(s.totalMs);
            holes = r.value().grid.holes;
        }
        const char* names[5] = {"template", "seed", "fit", "verify", "total"};
        for (int i = 0; i < 5; ++i) {
            add(QStringLiteral("UC-%1").arg(QString::fromLatin1(names[i])),
                QStringLiteral("use case stage '%1', 5 MP, %2 parts, 10% missing (%3 holes verified)").arg(QString::fromLatin1(names[i])).arg(board.presentCount()).arg(holes),
                QStringLiteral("ms"), stage[i], -1.0, QStringLiteral("5 warm-up, 20 runs"));
        }
    }

    void peakMemory() {
        for (const auto& [w, h, name] : {std::tuple{2592, 1944, "5MP"}, std::tuple{5800, 4350, "25MP"}}) {
            for (const PixelFormat f : {PixelFormat::Gray8, PixelFormat::Gray16}) {
                SyntheticBoardSpec spec = boardSpec(w, h);
                spec.format = f;
                const SyntheticBoard board = makeSyntheticBoard(spec);
                const TemplatePtr t = OpenCvTemplateBuilder().build(board.image, sampleBoxAround(board, middlePresentIndex(board)),
                                                                    anglesParams(5.0), CancelToken()).value();
                OpenCvPatternSearcher searcher;
                (void)searcher.find(board.image, *t, SearchRegion::wholeImage(), anglesParams(5.0), CancelToken());  // 熱機
                std::vector<double> peaks;
                for (int k = 0; k < 3; ++k) {
                    PeakSampler sampler;
                    (void)searcher.find(board.image, *t, SearchRegion::wholeImage(), anglesParams(5.0), CancelToken());
                    peaks.push_back(sampler.finish());
                }
                const double imageMb = static_cast<double>(board.image.sizeBytes()) / (1024.0 * 1024.0);
                add(QStringLiteral("MEM-%1-%2").arg(QString::fromLatin1(name), QString::fromLatin1(toString(f).data())),
                    QStringLiteral("peak extra memory of one seed search, %1 %2 (image itself %3 MB)").arg(QString::fromLatin1(name))
                        .arg(QString::fromLatin1(toString(f).data())).arg(imageMb, 0, 'f', 1),
                    QStringLiteral("MB"), peaks, -1.0, QStringLiteral("3 runs; sampled every 1 ms"));
            }
        }
    }

    // T7：流程 B 的樣本庫比對，N 個候選類型。每個類型的參考影像是一個合成元件（大小、亮度各不同），區域 640×480、約 40 個元件。
    void t7LibraryMatch() {
        SyntheticBoardSpec boardSpecB = boardSpec(1280, 960);
        boardSpecB.noiseAmplitude = 0;
        boardSpecB.blurRadius = 0;
        const SyntheticBoard board = makeSyntheticBoard(boardSpecB);
        const Rect region{100.0, 100.0, 640.0, 480.0};
        // 類型 k：第 0 個和板上的元件一樣，其他的主體大小與亮度依 k 變化（不會比對到，或分數很低）。
        std::vector<ComponentType> library;
        for (int k = 0; k < 1000; ++k) {
            SyntheticBoardSpec part;
            part.width = 96;
            part.height = 64;
            part.rows = 1;
            part.cols = 1;
            part.originXPx = 48.0;
            part.originYPx = 32.0;
            part.bodyWidthPx = k == 0 ? boardSpecB.bodyWidthPx : 30.0 + (k * 7) % 40;
            part.bodyHeightPx = k == 0 ? boardSpecB.bodyHeightPx : 12.0 + (k * 3) % 16;
            part.bodyLevel = k == 0 ? boardSpecB.bodyLevel : 90 + (k * 11) % 120;
            part.padLevel = k == 0 ? boardSpecB.padLevel : 160 + (k * 13) % 90;
            part.backgroundLevel = boardSpecB.backgroundLevel;
            const SyntheticBoard ref = makeSyntheticBoard(part);
            const Rect box = sampleBoxAround(ref);
            ComponentType type;
            type.id = "t" + std::to_string(k);
            type.referenceImage = subView(ref.image, static_cast<int>(box.x), static_cast<int>(box.y), static_cast<int>(box.w),
                                          static_cast<int>(box.h));
            QVERIFY(type.referenceImage.valid());
            library.push_back(std::move(type));
        }
        OpenCvLibraryMatcher matcher;
        QJsonArray points;
        for (const std::size_t n : {std::size_t{1}, std::size_t{10}, std::size_t{100}, std::size_t{1000}}) {
            const std::vector<ComponentType> candidates(library.begin(), library.begin() + static_cast<std::ptrdiff_t>(n));
            std::size_t components = 0;
            std::size_t compared = 0;
            const int runs = n >= 1000 ? 3 : 10;
            const std::vector<double> times = timeRuns(
                [&] {
                    const Result<LibraryMatchResult> r = matcher.match(board.image, region, candidates, anglesParams(5.0), CancelToken(), nullptr);
                    QVERIFY(r.ok());
                    components = r.value().components.size();
                    compared = r.value().samplesCompared;
                },
                1, runs);
            add(QStringLiteral("T7-%1").arg(n), QStringLiteral("library match, region 640x480, %1 candidate types").arg(n), QStringLiteral("ms"), times,
                -1.0, QStringLiteral("1 warm-up, %1 runs; %2 components, %3 samples compared; %4 ms per type")
                          .arg(runs).arg(components).arg(compared).arg(percentile(times, 0.5) / static_cast<double>(n), 0, 'f', 2));
            QVERIFY(components >= 30);
            QJsonObject p;
            p["types"] = static_cast<int>(n);
            p["medianMs"] = std::round(percentile(times, 0.5) * 10.0) / 10.0;
            points.append(p);
        }
        curves_["libraryTypes"] = points;
    }

    void cleanupTestCase() {
        QJsonArray rows;
        for (const Metric& m : metrics_) {
            QJsonObject o;
            o["id"] = m.id;
            o["label"] = m.label;
            o["unit"] = m.unit;
            o["n"] = static_cast<int>(m.samples.size());
            o["median"] = std::round(m.median() * 1000.0) / 1000.0;
            o[m.higherIsBetter ? "p05" : "p95"] = std::round(m.p95() * 1000.0) / 1000.0;
            if (m.budget >= 0) {
                o["budget"] = m.budget;
                o["pass"] = m.pass();
            }
            if (!m.note.isEmpty()) o["note"] = m.note;
            rows.append(o);
        }
        QJsonObject root;
        root["schema"] = 1;
        root["step"] = "S07";
        root["note"] = "OpenCV back end on synthetic boards. Numbers compare only on this machine.";
        root["machine"] = machineFingerprint(hygiene_);
        root["metrics"] = rows;
        root["curves"] = curves_;
        const QString dir = QStringLiteral(AOI_SOURCE_DIR "/perf/results");
        QDir().mkpath(dir);
        const QString path = dir + QStringLiteral("/opencv_") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")) + QStringLiteral(".json");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson());
            qInfo().noquote() << "wrote" << path;
        }
    }
};

QTEST_GUILESS_MAIN(PerfOpenCv)
#include "perf_opencv.moc"
