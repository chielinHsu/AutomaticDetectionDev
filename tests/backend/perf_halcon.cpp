// S08 的效能量測（label perf）：HALCON back end。預算在 perf/budgets.json。和 perf_opencv 同一組情境（合成板子、參數一樣），
// OpenCV 也編進程式時（AOI_WITH_OPENCV=ON，預設），同一個行程、同一組情境把 OpenCV 也量一次，id 加 "CV-" 前綴：並排比較。
//   L-*    HALCON 的程式庫載入與授權檢查（第一次在行程裡呼叫 HALCON 的成本，與之後每次 get_system 的成本）
//   T1     範本建立（create_shape_model），64×32 的樣本、±5°；T1-360 是 ±180°
//   T2b    種子搜尋（整張影像），5 MP（2592×1944）、64×32、±5°（預算 80 ms）
//   T2b-16 同上，12 bit 的 Gray16：換成 8 bit（預設）與直接當 uint2（nativeGray16）
//   T4     視窗驗證，200 個預測的位置（視窗 ＝ 範本 × 1.5，分數下限 0.30，和 use case 一樣）
//   T6a／T6b  合成板子組（5 MP，8 種情況），use case 端到端（組裝根的 back end）
//   SUB-*  次像素的兩種做法（interpolation、least_squares）：時間與 T6b 的位置誤差
//   A-*、S-*、P-*、TH-*、UC-*、MEM-*  角度範圍、影像大小、樣本大小、HALCON 的執行緒數、use case 各階段、峰值記憶體
//   C-*    取消的延遲：搜尋開始 20 ms 後取消，到 find 回來的時間（interrupt_operator）
//   T7-N   樣本庫比對（流程 B），N ＝ 1、10、100、1000 個候選類型；一次 find_shape_models 與每個類型一次兩種
// 方法（docs/perf-test-plan.md）：熱機 5 次、正式量 30 次（慢的項目較少，note 有寫），中位數與 p95；
// 結果寫到 perf/results/halcon_<時間>.json（含機器指紋與曲線）。時間的預算只記錄是否通過，不讓測試失敗；準確度（T6）的預算是失敗條件。
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
#include "infrastructure/backend_halcon/HalconBackend.h"
#include "infrastructure/backend_halcon/HalconLibraryMatcher.h"
#include "infrastructure/backend_halcon/HalconLicense.h"
#include "infrastructure/backend_halcon/HalconPatternSearcher.h"
#include "infrastructure/backend_halcon/HalconRuntime.h"
#include "infrastructure/backend_halcon/HalconTemplateBuilder.h"
#include "synthetic_board.h"
#ifdef AOI_WITH_OPENCV
#include "infrastructure/backend_opencv/OpenCvBackend.h"
#include "infrastructure/backend_opencv/OpenCvLibraryMatcher.h"
#include "infrastructure/backend_opencv/OpenCvPatternSearcher.h"
#include "infrastructure/backend_opencv/OpenCvTemplateBuilder.h"
#endif

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

double msSince(Clock::time_point t0) { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); }

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
    m["halcon"] = QString::fromStdString(halconSdkVersion());
    const HalconLicenseInfo& lic = halconLicense();
    m["halconLicense"] = QString::fromStdString(lic.kind + " " + lic.hostId + " (" + lic.edition + ")");
    m["halconThreads"] = setHalconThreads(0);
#ifdef AOI_WITH_OPENCV
    m["opencv"] = QStringLiteral(CV_VERSION);
    m["opencvThreads"] = setOpenCvThreads(0);
#endif
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
        out.push_back(msSince(t0));
    }
    return out;
}

// 峰值記憶體：背景執行緒每 1 ms 讀一次行程的 private bytes（和 perf_opencv 相同）。
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

// 64×32 的樣本：主體 56×24 ＋ 每邊 4 像素（和 perf_opencv 相同）。
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

// T6 的一種情況：use case 端到端，扣掉樣本框中心的固定偏移之後的位置誤差（見 tst_opencv_end_to_end 的 sampleOffset）。
struct T6Outcome {
    std::size_t present = 0;
    std::size_t found = 0;
    double maxError = 0.0;
    double totalMs = 0.0;
};

T6Outcome runT6(GenerateObjectsFromSample& useCase, const SyntheticBoard& board, std::vector<double>* errors) {
    SampleRequest request;
    request.image = board.image;
    request.sample = sampleBoxAround(board, middlePresentIndex(board));
    const Result<SampleProposal> r = useCase.run(request, CancelToken());
    T6Outcome o;
    if (!r.ok() || r.value().candidates.empty()) return o;
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
    for (const SyntheticPart& p : board.parts) {
        if (!p.present) continue;
        ++o.present;
        const Point2D expected{p.pose.x + offset.x, p.pose.y + offset.y};
        double best = std::numeric_limits<double>::max();
        for (const Candidate& cand : r.value().candidates) {
            if (cand.hasScore()) best = std::min(best, distance(cand.pose.position(), expected));
        }
        if (best <= 3.0) {
            ++o.found;
            errors->push_back(best);
            o.maxError = std::max(o.maxError, best);
        }
    }
    o.totalMs = r.value().timings.totalMs;
    return o;
}

struct T6Case {
    const char* name;
    std::function<void(SyntheticBoardSpec&)> tweak;
};

std::vector<T6Case> t6Cases() {
    return {
        {"clean", [](SyntheticBoardSpec& s) { s.noiseAmplitude = 0; s.blurRadius = 0; }},
        {"noise 4, blur 1", [](SyntheticBoardSpec&) {}},
        {"noise 12, blur 2", [](SyntheticBoardSpec& s) { s.noiseAmplitude = 12; s.blurRadius = 2; }},
        {"5% missing", [](SyntheticBoardSpec& s) { s.missingFraction = 0.05; }},
        {"10% glare", [](SyntheticBoardSpec& s) { s.glareFraction = 0.10; }},
        {"tilt 3 deg", [](SyntheticBoardSpec& s) { s.tiltDeg = 3.0; }},
        {"jitter 3 px", [](SyntheticBoardSpec& s) { s.positionJitterPx = 3.0; }},
        {"12 bit", [](SyntheticBoardSpec& s) { s.format = PixelFormat::Gray16; s.grayBits = 12; }},
    };
}

// T7 的樣本庫：類型 k，第 0 個和板上的元件一樣，其他的主體大小與亮度依 k 變化（和 perf_opencv 相同）。
std::vector<ComponentType> t7Library(const SyntheticBoardSpec& boardSpecB, std::vector<SyntheticBoard>* keep) {
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
        keep->push_back(makeSyntheticBoard(part));
        const SyntheticBoard& ref = keep->back();
        const Rect box = sampleBoxAround(ref);
        ComponentType type;
        type.id = "t" + std::to_string(k);
        type.referenceImage = subView(ref.image, static_cast<int>(box.x), static_cast<int>(box.y), static_cast<int>(box.w),
                                      static_cast<int>(box.h));
        library.push_back(std::move(type));
    }
    return library;
}

}  // namespace

class PerfHalcon : public QObject {
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

    static TemplatePtr halconTemplate(const SyntheticBoard& board, const Rect& sample, const SearchParams& p, HalconSearchOptions o = {}) {
        Result<TemplatePtr> t = HalconTemplateBuilder(std::move(o)).build(board.image, sample, p, CancelToken());
        return t.ok() ? t.value() : TemplatePtr();
    }

    std::vector<double> timeSeed(const SyntheticBoard& board, const ITemplate& t, const SearchParams& p, int warmup, int runs,
                                 HalconSearchStats* stats = nullptr, HalconSearchOptions o = {}) {
        HalconPatternSearcher searcher(std::move(o));
        return timeRuns(
            [&] {
                const Result<std::vector<Match>> r = searcher.find(board.image, t, SearchRegion::wholeImage(), p, CancelToken(), stats);
                QVERIFY(r.ok());
            },
            warmup, runs);
    }

private slots:
    // 授權與程式庫：這是行程裡第一次碰 HALCON（延遲載入），所以量到的是「冷啟動」的成本。
    void initTestCase() {
        hygiene_ = applyMeasurementHygiene();
        Clock::time_point t0 = Clock::now();
        const std::optional<std::string> problem = halconRuntimeProblem();
        const double loadMs = msSince(t0);
        if (problem) QSKIP(problem->c_str());
        t0 = Clock::now();
        const HalconLicenseInfo& first = halconLicense();
        const double licenseMs = msSince(t0);
        const HalconLicenseVerdict verdict = judgeHalconLicense(first);
        if (!verdict.usable) QSKIP(verdict.reason.c_str());
        add(QStringLiteral("L-load"), QStringLiteral("load halconcpp.dll and bind its imports (first time in the process)"), QStringLiteral("ms"),
            {loadMs}, -1.0, QStringLiteral("1 sample"));
        add(QStringLiteral("L-first"), QStringLiteral("first HALCON operator + license check + get_system queries"), QStringLiteral("ms"),
            {licenseMs}, -1.0, QStringLiteral("1 sample; %1 license %2").arg(QString::fromStdString(first.kind), QString::fromStdString(first.hostId)));
        add(QStringLiteral("L-again"), QStringLiteral("license probe again (HALCON already initialized)"), QStringLiteral("ms"),
            timeRuns([] { (void)probeHalconLicense(); }, 2, 10), -1.0, QStringLiteral("2 warm-up, 10 runs"));
        setHalconThreads(static_cast<int>(app::workerThreads()));  // 和產品一樣（組裝根的政策）
#ifdef AOI_WITH_OPENCV
        setOpenCvThreads(static_cast<int>(app::workerThreads()));
#endif
    }

    void t1TemplateBuild() {
        const SyntheticBoard board = makeSyntheticBoard(boardSpec(2592, 1944));
        const Rect sample = sampleBoxAround(board, middlePresentIndex(board));
        QCOMPARE(sample.w, 64.0);
        QCOMPARE(sample.h, 32.0);
        HalconTemplateBuilder builder;
        add(QStringLiteral("T1"), QStringLiteral("template build (create_shape_model), 64x32 sample, +-5 deg"), QStringLiteral("ms"),
            timeRuns([&] { QVERIFY(builder.build(board.image, sample, anglesParams(5.0), CancelToken()).ok()); }, 5, 30), budgetOf(QStringLiteral("T1")));
        add(QStringLiteral("T1-360"), QStringLiteral("template build, 64x32 sample, +-180 deg"), QStringLiteral("ms"),
            timeRuns([&] { QVERIFY(builder.build(board.image, sample, anglesParams(180.0), CancelToken()).ok()); }, 2, 10), -1.0,
            QStringLiteral("2 warm-up, 10 runs"));
#ifdef AOI_WITH_OPENCV
        OpenCvTemplateBuilder cv;
        add(QStringLiteral("CV-T1"), QStringLiteral("OpenCV template build, 64x32 sample, +-5 deg"), QStringLiteral("ms"),
            timeRuns([&] { QVERIFY(cv.build(board.image, sample, anglesParams(5.0), CancelToken()).ok()); }, 5, 30), budgetOf(QStringLiteral("T1")));
#endif
    }

    void t2bSeedSearchFiveMegapixels() {
        const SyntheticBoard board = makeSyntheticBoard(boardSpec(2592, 1944));
        const Rect sample = sampleBoxAround(board, middlePresentIndex(board));
        const TemplatePtr t = halconTemplate(board, sample, anglesParams(5.0));
        QVERIFY(t);
        HalconSearchStats stats;
        const std::vector<double> times = timeSeed(board, *t, anglesParams(5.0), 5, 30, &stats);
        add(QStringLiteral("T2b"), QStringLiteral("seed search (find_shape_model), 5 MP, 64x32, +-5 deg, %1 parts").arg(board.presentCount()),
            QStringLiteral("ms"), times, budgetOf(QStringLiteral("T2b")),
            QStringLiteral("last run: convert %1, find %2, nms %3 ms; %4 raw, %5 matches; %6 levels")
                .arg(stats.convertMs, 0, 'f', 2).arg(stats.findMs, 0, 'f', 2).arg(stats.nmsMs, 0, 'f', 2)
                .arg(stats.rawMatches).arg(stats.matches).arg(stats.numLevels));
        QCOMPARE(stats.matches, board.presentCount());
#ifdef AOI_WITH_OPENCV
        const TemplatePtr ct = OpenCvTemplateBuilder().build(board.image, sample, anglesParams(5.0), CancelToken()).value();
        OpenCvPatternSearcher cv;
        add(QStringLiteral("CV-T2a"), QStringLiteral("OpenCV seed search, same board"), QStringLiteral("ms"),
            timeRuns([&] { QVERIFY(cv.find(board.image, *ct, SearchRegion::wholeImage(), anglesParams(5.0), CancelToken()).ok()); }, 5, 30),
            budgetOf(QStringLiteral("T2a")));
#endif
    }

    // 12 bit 的 Gray16：預設換成 8 bit（一次複製，和 OpenCV 同一個輸入），或直接當 uint2（零複製）。
    void t2bTwelveBit() {
        SyntheticBoardSpec spec = boardSpec(2592, 1944);
        spec.format = PixelFormat::Gray16;
        spec.grayBits = 12;
        const SyntheticBoard board = makeSyntheticBoard(spec);
        const Rect sample = sampleBoxAround(board, middlePresentIndex(board));
        HalconSearchOptions native;
        native.nativeGray16 = true;
        for (const bool n : {false, true}) {
            const HalconSearchOptions o = n ? native : HalconSearchOptions{};
            const TemplatePtr t = halconTemplate(board, sample, anglesParams(5.0), o);
            HalconSearchStats stats;
            const std::vector<double> times = timeSeed(board, *t, anglesParams(5.0), 3, 20, &stats, o);
            add(n ? QStringLiteral("T2b-16native") : QStringLiteral("T2b-16"),
                QStringLiteral("seed search, 5 MP 12 bit Gray16, %1").arg(n ? QStringLiteral("native uint2") : QStringLiteral("scaled to 8 bit")),
                QStringLiteral("ms"), times, -1.0,
                QStringLiteral("3 warm-up, 20 runs; convert %1, find %2 ms; %3 matches").arg(stats.convertMs, 0, 'f', 2)
                    .arg(stats.findMs, 0, 'f', 2).arg(stats.matches));
            QCOMPARE(stats.matches, board.presentCount());
        }
    }

    void t4WindowedVerify() {
        const SyntheticBoard board = makeSyntheticBoard(boardSpec(2592, 1944));
        const Rect sample = sampleBoxAround(board, middlePresentIndex(board));
        const TemplatePtr t = halconTemplate(board, sample, anglesParams(5.0));
        std::vector<Rect> windows;
        for (std::size_t k = 0; k < board.parts.size() && windows.size() < 200; k += 4) {
            windows.push_back(rectFromCenter(board.parts[k].pose.position(), {t->size().w * 1.5, t->size().h * 1.5}));
        }
        QCOMPARE(windows.size(), std::size_t{200});
        HalconPatternSearcher searcher;
        HalconSearchStats stats;
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
            budgetOf(QStringLiteral("T4")), QStringLiteral("%1 matches; convert (domain) %2, find %3, nms %4 ms").arg(found)
                                                .arg(stats.convertMs, 0, 'f', 2).arg(stats.findMs, 0, 'f', 2).arg(stats.nmsMs, 0, 'f', 2));
        QVERIFY(found >= 200);
#ifdef AOI_WITH_OPENCV
        const TemplatePtr ct = OpenCvTemplateBuilder().build(board.image, sample, anglesParams(5.0), CancelToken()).value();
        OpenCvPatternSearcher cv;
        add(QStringLiteral("CV-T4"), QStringLiteral("OpenCV windowed verify, same windows"), QStringLiteral("ms"),
            timeRuns([&] { QVERIFY(cv.find(board.image, *ct, SearchRegion::inWindows(windows), anglesParams(5.0, 0.30), CancelToken()).ok()); }, 5, 30),
            budgetOf(QStringLiteral("T4")));
#endif
    }

    // T6：合成板子組，use case 端到端（組裝根的 back end）。OpenCV 也編進來時同一組板子並排。
    void t6AccuracyOnTheSyntheticSet() {
        std::vector<QString> backends{QStringLiteral("halcon")};
#ifdef AOI_WITH_OPENCV
        backends.push_back(QStringLiteral("opencv"));
#endif
        for (const QString& id : backends) {
            Result<app::CoreServices> core = app::composeCore(app::makeBackendRegistry(), id.toStdString(), false, app::ExecutorKind::StdThreads);
            QVERIFY2(core.ok(), core.ok() ? "" : core.error().message.c_str());
            std::size_t present = 0;
            std::size_t found = 0;
            std::vector<double> errors;
            QJsonArray rows;
            for (const T6Case& c : t6Cases()) {
                SyntheticBoardSpec spec = boardSpec(2592, 1944);
                c.tweak(spec);
                const SyntheticBoard board = makeSyntheticBoard(spec);
                const T6Outcome o = runT6(*core.value().generateObjects, board, &errors);
                present += o.present;
                found += o.found;
                qInfo("T6 %s %-16s present %zu found %zu max error %.3f px, total %.1f ms", qPrintable(id), c.name, o.present, o.found,
                      o.maxError, o.totalMs);
                QJsonObject row;
                row["case"] = QString::fromLatin1(c.name);
                row["present"] = static_cast<int>(o.present);
                row["found"] = static_cast<int>(o.found);
                row["maxErrorPx"] = std::round(o.maxError * 1000.0) / 1000.0;
                row["totalMs"] = std::round(o.totalMs * 10.0) / 10.0;
                rows.append(row);
            }
            const QString prefix = id == QLatin1String("halcon") ? QString() : QStringLiteral("CV-");
            curves_[prefix + QStringLiteral("t6Cases")] = rows;
            const double recall = 100.0 * static_cast<double>(found) / static_cast<double>(std::max<std::size_t>(1, present));
            add(prefix + QStringLiteral("T6a"), QStringLiteral("%1 recall on the synthetic set (8 boards, 5 MP)").arg(id), QStringLiteral("percent"),
                {recall}, budgetOf(QStringLiteral("T6a")), QStringLiteral("%1 of %2 parts").arg(found).arg(present), true);
            add(prefix + QStringLiteral("T6b"), QStringLiteral("%1 position error on the synthetic set").arg(id), QStringLiteral("px"), errors,
                budgetOf(QStringLiteral("T6b")),
                QStringLiteral("p95 of all found parts; median %1, max %2 px").arg(percentile(errors, 0.5), 0, 'f', 3)
                    .arg(errors.empty() ? 0.0 : *std::max_element(errors.begin(), errors.end()), 0, 'f', 3));
            QVERIFY2(recall >= budgetOf(QStringLiteral("T6a")), qPrintable(id + QStringLiteral(": ") + QString::number(recall)));
            QVERIFY2(percentile(errors, 0.95) <= budgetOf(QStringLiteral("T6b")), qPrintable(id + QStringLiteral(": ") + QString::number(percentile(errors, 0.95))));
        }
    }

    // 次像素的兩種做法：時間（種子搜尋）與位置誤差（noise 4、blur 1 的板子，搜尋器直接和標準答案比）。
    void subPixelModes() {
        const SyntheticBoard board = makeSyntheticBoard(boardSpec(2592, 1944));
        const Rect sample = sampleBoxAround(board, middlePresentIndex(board));
        for (const char* mode : {"interpolation", "least_squares", "least_squares_high"}) {
            HalconSearchOptions o;
            o.subPixel = mode;
            const TemplatePtr t = halconTemplate(board, sample, anglesParams(5.0), o);
            HalconPatternSearcher searcher(o);
            std::vector<Match> found;
            const std::vector<double> times = timeRuns(
                [&] { found = searcher.find(board.image, *t, SearchRegion::wholeImage(), anglesParams(5.0), CancelToken()).value(); }, 2, 10);
            // 參考點是樣本框的中心：扣掉它和元件中心的固定偏移（同 T6）。
            const Rect box = sampleBoxAround(board, middlePresentIndex(board));
            std::size_t i = 0;
            Point2D truthCenter{};
            for (const SyntheticPart& p : board.parts) {
                if (p.present && i++ == middlePresentIndex(board)) truthCenter = p.pose.position();
            }
            const Point2D offset{box.center().x - truthCenter.x, box.center().y - truthCenter.y};
            std::vector<Pose2D> shifted;
            for (const Match& m : found) shifted.push_back({m.pose.x - offset.x, m.pose.y - offset.y, m.pose.angleDeg});
            const Accuracy a = compareWithTruth(board, shifted, 0.0);
            add(QStringLiteral("SUB-%1").arg(QString::fromLatin1(mode)), QStringLiteral("seed search with SubPixel '%1'").arg(QString::fromLatin1(mode)),
                QStringLiteral("ms"), times, -1.0,
                QStringLiteral("2 warm-up, 10 runs; mean error %1 px, max %2 px, angle max %3 deg, %4/%5 found")
                    .arg(a.meanErrorPx, 0, 'f', 3).arg(a.maxErrorPx, 0, 'f', 3).arg(a.maxAngleErrorDeg, 0, 'f', 3).arg(a.matched).arg(a.present));
        }
    }

    // 角度步進（角落每步移動幾個像素）與模型角度範圍的餘量：在 −5°..+5°（每 0.5°）傾斜的板子上，角度與位置的誤差、每張的搜尋時間。
    // 這組數字是 HalconSearchOptions 預設值（cornerShiftPx 0.5、angleMarginSteps 2）的根據；'auto' 是 HALCON 自己選的步進。
    void angleSettings() {
        const SyntheticBoard straight = makeSyntheticBoard(boardSpec(1024, 768));
        const Rect sample = sampleBoxAround(straight, middlePresentIndex(straight));
        struct Setting {
            const char* name;
            double shift;
            double margin;
        };
        QJsonArray points;
        for (const Setting s : {Setting{"auto-m0", 0.0, 0.0}, Setting{"1.0-m0", 1.0, 0.0}, Setting{"0.5-m0", 0.5, 0.0},
                                Setting{"1.0-m2", 1.0, 2.0}, Setting{"0.5-m2", 0.5, 2.0}, Setting{"0.25-m2", 0.25, 2.0}}) {
            HalconSearchOptions o;
            o.cornerShiftPx = s.shift;
            o.angleMarginSteps = s.margin;
            const TemplatePtr t = halconTemplate(straight, sample, anglesParams(5.0), o);
            QVERIFY(t);
            HalconPatternSearcher searcher(o);
            std::vector<double> times;
            double angleMax = 0.0;
            double angleSum = 0.0;
            double pxMax = 0.0;
            std::size_t n = 0;
            for (int k = -10; k <= 10; ++k) {
                SyntheticBoardSpec spec = boardSpec(1024, 768);
                spec.tiltDeg = 0.5 * k;
                const SyntheticBoard board = makeSyntheticBoard(spec);
                const auto t0 = Clock::now();
                const std::vector<Match> found = searcher.find(board.image, *t, SearchRegion::wholeImage(), anglesParams(5.0), CancelToken()).value();
                times.push_back(msSince(t0));
                const Accuracy a = compareWithTruth(board, posesOf(found), spec.tiltDeg);
                QCOMPARE(a.matched, a.present);
                angleMax = std::max(angleMax, a.maxAngleErrorDeg);
                pxMax = std::max(pxMax, a.maxErrorPx);
                for (const Match& m : found) {
                    angleSum += std::abs(m.pose.angleDeg - spec.tiltDeg);
                    ++n;
                }
            }
            const double angleMean = angleSum / static_cast<double>(std::max<std::size_t>(1, n));
            add(QStringLiteral("AS-%1").arg(QString::fromLatin1(s.name)),
                QStringLiteral("seed search on 21 boards tilted -5..5 deg, corner shift %1 px, margin %2 steps").arg(s.shift).arg(s.margin),
                QStringLiteral("ms"), times, -1.0,
                QStringLiteral("angle error max %1, mean %2 deg; position error max %3 px").arg(angleMax, 0, 'f', 3).arg(angleMean, 0, 'f', 3).arg(pxMax, 0, 'f', 3));
            QJsonObject p;
            p["setting"] = QString::fromLatin1(s.name);
            p["angleMaxDeg"] = std::round(angleMax * 1000.0) / 1000.0;
            p["angleMeanDeg"] = std::round(angleMean * 1000.0) / 1000.0;
            p["positionMaxPx"] = std::round(pxMax * 1000.0) / 1000.0;
            p["medianMs"] = std::round(percentile(times, 0.5) * 100.0) / 100.0;
            points.append(p);
        }
        curves_["angleSettings"] = points;
    }

    void angleRangeCurve() {
        const SyntheticBoard board = makeSyntheticBoard(boardSpec(2592, 1944));
        const Rect sample = sampleBoxAround(board, middlePresentIndex(board));
        QJsonArray points;
        for (const double half : {0.0, 2.0, 5.0, 10.0, 20.0, 45.0, 90.0, 180.0}) {
            const auto b0 = Clock::now();
            const TemplatePtr t = halconTemplate(board, sample, anglesParams(half));
            const double buildMs = msSince(b0);
            HalconSearchStats stats;
            const std::vector<double> times = timeSeed(board, *t, anglesParams(half), 1, 5, &stats);
            add(QStringLiteral("A-%1").arg(half), QStringLiteral("seed search, 5 MP, 64x32, +-%1 deg").arg(half), QStringLiteral("ms"), times,
                -1.0, QStringLiteral("1 warm-up, 5 runs; build %1 ms; %2 matches").arg(buildMs, 0, 'f', 1).arg(stats.matches));
            QJsonObject p;
            p["halfRangeDeg"] = half;
            p["medianMs"] = std::round(percentile(times, 0.5) * 10.0) / 10.0;
            p["buildMs"] = std::round(buildMs * 10.0) / 10.0;
            points.append(p);
        }
        curves_["angleRange"] = points;
    }

    void imageSizeCurve() {
        QJsonArray points;
        struct Size { int w; int h; const char* name; };
        for (const Size s : {Size{1280, 800, "1 MP"}, Size{2592, 1944, "5 MP"}, Size{4000, 3000, "12 MP"}, Size{5800, 4350, "25 MP"}}) {
            const SyntheticBoard board = makeSyntheticBoard(boardSpec(s.w, s.h));
            const TemplatePtr t = halconTemplate(board, sampleBoxAround(board, middlePresentIndex(board)), anglesParams(5.0));
            const std::vector<double> times = timeSeed(board, *t, anglesParams(5.0), 1, 5);
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
            const TemplatePtr t = halconTemplate(board, sample, anglesParams(5.0));
            HalconSearchStats stats;
            const std::vector<double> times = timeSeed(board, *t, anglesParams(5.0), 1, 5, &stats);
            const QString name = QStringLiteral("%1x%2").arg(sample.w).arg(sample.h);
            add(QStringLiteral("P-%1").arg(name), QStringLiteral("seed search, 5 MP, %1 sample, +-5 deg, %2 parts").arg(name).arg(board.presentCount()),
                QStringLiteral("ms"), times, -1.0, QStringLiteral("1 warm-up, 5 runs; %1 levels, %2 matches").arg(stats.numLevels).arg(stats.matches));
            QJsonObject p;
            p["sample"] = name;
            p["parts"] = static_cast<int>(board.presentCount());
            p["levels"] = stats.numLevels;
            p["matches"] = static_cast<int>(stats.matches);
            p["medianMs"] = std::round(percentile(times, 0.5) * 10.0) / 10.0;
            points.append(p);
        }
        curves_["sampleSize"] = points;
    }

    void threadCount() {
        const SyntheticBoard board = makeSyntheticBoard(boardSpec(2592, 1944));
        const TemplatePtr t = halconTemplate(board, sampleBoxAround(board, middlePresentIndex(board)), anglesParams(5.0));
        const int policy = static_cast<int>(app::workerThreads());
        QJsonArray points;
        for (const int n : {1, 2, 4, 8, policy}) {
            setHalconThreads(n);
            const std::vector<double> times = timeSeed(board, *t, anglesParams(5.0), 2, 10);
            add(QStringLiteral("TH-%1").arg(n), QStringLiteral("seed search, 5 MP, HALCON thread_num %1").arg(n), QStringLiteral("ms"), times, -1.0,
                QStringLiteral("2 warm-up, 10 runs"));
            QJsonObject p;
            p["threads"] = n;
            p["medianMs"] = std::round(percentile(times, 0.5) * 10.0) / 10.0;
            points.append(p);
        }
        setHalconThreads(policy);
        curves_["threads"] = points;
    }

    void useCaseStages() {
        Result<app::CoreServices> core = app::composeCore(app::makeBackendRegistry(), "halcon", false, app::ExecutorKind::StdThreads);
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
            const Result<SampleProposal> r = core.value().generateObjects->run(request, CancelToken());
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

    // 取消的延遲：一次長的搜尋（12 MP、±180°）開始 20 ms 後取消，量「取消 → find 回來」。interrupt_operator 的效果。
    void cancelLatency() {
        const SyntheticBoard board = makeSyntheticBoard(boardSpec(4000, 3000));
        const TemplatePtr t = halconTemplate(board, sampleBoxAround(board, middlePresentIndex(board)), anglesParams(180.0));
        HalconPatternSearcher searcher;
        HalconSearchStats full;
        QVERIFY(searcher.find(board.image, *t, SearchRegion::wholeImage(), anglesParams(180.0), CancelToken(), &full).ok());
        std::vector<double> latency;
        for (int k = 0; k < 10; ++k) {
            CancelSource cancel;
            Clock::time_point cancelledAt;
            std::thread canceller([&] {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                cancelledAt = Clock::now();
                cancel.cancel();
            });
            const Result<std::vector<Match>> r = searcher.find(board.image, *t, SearchRegion::wholeImage(), anglesParams(180.0), cancel.token());
            const Clock::time_point returned = Clock::now();
            canceller.join();
            QVERIFY(!r.ok() && r.error().code == ErrorCode::Cancelled);
            latency.push_back(std::chrono::duration<double, std::milli>(returned - cancelledAt).count());
        }
        add(QStringLiteral("C-search"), QStringLiteral("cancel latency during find_shape_model (12 MP, +-180 deg)"), QStringLiteral("ms"), latency,
            -1.0, QStringLiteral("10 runs; the full search takes %1 ms").arg(full.totalMs, 0, 'f', 1));
    }

    void peakMemory() {
        for (const auto& [w, h, name] : {std::tuple{2592, 1944, "5MP"}, std::tuple{5800, 4350, "25MP"}}) {
            for (const PixelFormat f : {PixelFormat::Gray8, PixelFormat::Gray16}) {
                SyntheticBoardSpec spec = boardSpec(w, h);
                spec.format = f;
                const SyntheticBoard board = makeSyntheticBoard(spec);
                const TemplatePtr t = halconTemplate(board, sampleBoxAround(board, middlePresentIndex(board)), anglesParams(5.0));
                HalconPatternSearcher searcher;
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

    // T7：流程 B 的樣本庫比對，N 個候選類型（和 perf_opencv 同一個樣本庫與區域）。HALCON 兩種做法並排。
    void t7LibraryMatch() {
        SyntheticBoardSpec boardSpecB = boardSpec(1280, 960);
        boardSpecB.noiseAmplitude = 0;
        boardSpecB.blurRadius = 0;
        const SyntheticBoard board = makeSyntheticBoard(boardSpecB);
        const Rect region{100.0, 100.0, 640.0, 480.0};
        std::vector<SyntheticBoard> references;
        references.reserve(1000);
        const std::vector<ComponentType> library = t7Library(boardSpecB, &references);
        HalconSearchOptions perType;
        perType.libraryOneCall = false;
        struct Mode {
            QString prefix;
            std::unique_ptr<ILibraryMatcher> matcher;
        };
        std::vector<Mode> modes;
        modes.push_back({QStringLiteral("T7-"), std::make_unique<HalconLibraryMatcher>()});
        modes.push_back({QStringLiteral("T7each-"), std::make_unique<HalconLibraryMatcher>(perType)});
#ifdef AOI_WITH_OPENCV
        modes.push_back({QStringLiteral("CV-T7-"), std::make_unique<OpenCvLibraryMatcher>()});
#endif
        for (Mode& mode : modes) {
            QJsonArray points;
            for (const std::size_t n : {std::size_t{1}, std::size_t{10}, std::size_t{100}, std::size_t{1000}}) {
                const std::vector<ComponentType> candidates(library.begin(), library.begin() + static_cast<std::ptrdiff_t>(n));
                std::size_t components = 0;
                std::size_t compared = 0;
                const int runs = n >= 1000 ? 3 : (n >= 100 ? 5 : 10);
                const std::vector<double> times = timeRuns(
                    [&] {
                        const Result<LibraryMatchResult> r = mode.matcher->match(board.image, region, candidates, anglesParams(5.0), CancelToken(), nullptr);
                        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
                        components = r.value().components.size();
                        compared = r.value().samplesCompared;
                    },
                    1, runs);
                add(mode.prefix + QString::number(n), QStringLiteral("library match, region 640x480, %1 candidate types").arg(n), QStringLiteral("ms"),
                    times, -1.0, QStringLiteral("1 warm-up, %1 runs; %2 components, %3 samples compared; %4 ms per type")
                                     .arg(runs).arg(components).arg(compared).arg(percentile(times, 0.5) / static_cast<double>(n), 0, 'f', 2));
                QVERIFY(components >= 30);
                QJsonObject p;
                p["types"] = static_cast<int>(n);
                p["medianMs"] = std::round(percentile(times, 0.5) * 10.0) / 10.0;
                points.append(p);
            }
            curves_[QStringLiteral("libraryTypes-") + mode.prefix.chopped(1)] = points;
        }
    }

    void cleanupTestCase() {
        if (metrics_.empty()) return;
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
        root["step"] = "S08";
        root["note"] = "HALCON back end on synthetic boards (CV-* = OpenCV in the same run). Numbers compare only on this machine.";
        root["machine"] = machineFingerprint(hygiene_);
        root["metrics"] = rows;
        root["curves"] = curves_;
        const QString dir = QStringLiteral(AOI_SOURCE_DIR "/perf/results");
        QDir().mkpath(dir);
        const QString path = dir + QStringLiteral("/halcon_") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")) + QStringLiteral(".json");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson());
            qInfo().noquote() << "wrote" << path;
        }
    }
};

QTEST_GUILESS_MAIN(PerfHalcon)
#include "perf_halcon.moc"
