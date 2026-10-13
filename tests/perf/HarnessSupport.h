#pragma once

// S10 的效能 harness 共用（tests/perf）：統計、指標、機器指紋、量測當下的環境（CPU 負載與時脈、電源計畫）、
// 峰值記憶體取樣、執行次數的設定、結果檔的寫出。方法見 docs/perf-test-plan.md 與 docs/steps/S10-perf-harness/spec.md。
//
// 和 S02–S09 各自的量測程式（perf_opencv、perf_ui_s04 …）的差別：那些程式各自有一份統計與指紋的程式碼（沒有改動它們，
// 它們的結果由 tools/perf-compare.mjs 的 collect 讀進同一份執行紀錄）；S10 新的執行檔（perf_matrix、perf_e2e）用這一份。
//
// 環境變數（都是選用的；沒設就用各執行檔的預設）：
//   AOI_PERF_PROFILE          quick（預設：基準格 5／30 次，其餘的格子 2／10 次）或 full（整個矩陣，每格 1／3 次）
//   AOI_PERF_WARMUP、AOI_PERF_RUNS   直接指定熱機與正式量測的次數（蓋過 profile）
//   AOI_PERF_INJECT_DELAY_MS  harness 的自我測試：每一次正式量測多加這麼多毫秒的忙等（結果檔會記下 injectedDelayMs，
//                             perf-compare 的 baseline 子命令拒絕把這種結果當基準線）。用來證明閘門抓得到變慢。
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QString>
#include <QSysInfo>
#include <QThread>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numeric>
#include <thread>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <powerbase.h>
#include <powrprof.h>
#include <psapi.h>
#endif

namespace aoi::test::harness {

using Clock = std::chrono::steady_clock;

// 分位數：和 perf_opencv、perf_halcon、perf_ab 同一個定義（ceil(p·n) 名次），所以數字可以和 S07–S09 的基準線比。
inline double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const auto rank = static_cast<std::size_t>(std::ceil(p * static_cast<double>(v.size())));
    return v[std::min(v.size(), std::max<std::size_t>(1, rank)) - 1];
}

inline double round3(double v) { return std::round(v * 1000.0) / 1000.0; }

struct Stats {
    int n = 0;
    double median = 0, p95 = 0, min = 0, max = 0, mean = 0;
};

inline Stats summarize(const std::vector<double>& v) {
    Stats s;
    s.n = static_cast<int>(v.size());
    if (v.empty()) return s;
    s.median = percentile(v, 0.5);
    s.p95 = percentile(v, 0.95);
    s.min = *std::min_element(v.begin(), v.end());
    s.max = *std::max_element(v.begin(), v.end());
    s.mean = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
    return s;
}

// 一個指標。結果 JSON 的 metrics[] 一列（欄位和 S02–S09 的結果檔相同：id、label、unit、n、median、p95、max、budget、pass、note），
// S10 多了 budgetId（對應 perf/budgets.json 的 id，閘門用它查「已知超標」的狀態）、direction（min ＝ 越大越好，例如召回率）、
// known（已知、待修的問題：寫出處，閘門列出但不當失敗）。
struct Metric {
    QString id;
    QString label;
    QString unit = QStringLiteral("ms");
    Stats stats;
    double budget = -1;  // < 0 ＝ 沒有預算（只記錄）
    QString budgetId;    // 預算在 perf/budgets.json 的 id（例如 T5）；空 ＝ 沒有預算
    bool higherIsBetter = false;
    QString known;       // 非空 ＝ 已知的問題（例如 "ADR-004 §5 B1"），超標時不算失敗
    QString note;
    // 越大越好的指標看 p05（最差的 5%），其餘看 p95。
    double gateValue() const { return higherIsBetter ? stats.min : stats.p95; }
    bool pass() const { return budget < 0 || (higherIsBetter ? gateValue() >= budget : gateValue() <= budget); }
};

inline QJsonObject toJson(const Metric& m) {
    QJsonObject o;
    o["id"] = m.id;
    o["label"] = m.label;
    o["unit"] = m.unit;
    o["n"] = m.stats.n;
    o["median"] = round3(m.stats.median);
    o["p95"] = round3(m.stats.p95);
    o["min"] = round3(m.stats.min);
    o["max"] = round3(m.stats.max);
    if (m.budget >= 0) {
        o["budget"] = m.budget;
        o["pass"] = m.pass();
    }
    if (!m.budgetId.isEmpty()) o["budgetId"] = m.budgetId;
    if (m.higherIsBetter) o["direction"] = QStringLiteral("min");
    if (!m.known.isEmpty()) o["known"] = m.known;
    if (!m.note.isEmpty()) o["note"] = m.note;
    return o;
}

// 預算：perf/budgets.json 是唯一來源。
inline QJsonObject budgetEntry(const QString& id) {
    QFile f(QStringLiteral(AOI_SOURCE_DIR "/perf/budgets.json"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).object().value("budgets").toArray();
    for (const QJsonValue& v : arr)
        if (v.toObject().value("id").toString() == id) return v.toObject();
    return {};
}
inline double budgetOf(const QString& id) { return budgetEntry(id).value("limit").toDouble(-1); }

// 執行次數的設定（環境變數見檔頭）。
struct RunConfig {
    QString profile = QStringLiteral("quick");
    int warmup = 5;
    int runs = 30;
    double injectedDelayMs = 0.0;

    static RunConfig fromEnvironment(int defaultWarmup, int defaultRuns) {
        RunConfig c;
        c.warmup = defaultWarmup;
        c.runs = defaultRuns;
        const QString profile = qEnvironmentVariable("AOI_PERF_PROFILE").trimmed().toLower();
        if (profile == QLatin1String("full")) c.profile = QStringLiteral("full");
        bool ok = false;
        const int warmup = qEnvironmentVariable("AOI_PERF_WARMUP").toInt(&ok);
        if (ok && warmup >= 0) c.warmup = warmup;
        const int runs = qEnvironmentVariable("AOI_PERF_RUNS").toInt(&ok);
        if (ok && runs > 0) c.runs = runs;
        const double delay = qEnvironmentVariable("AOI_PERF_INJECT_DELAY_MS").toDouble(&ok);
        if (ok && delay > 0) c.injectedDelayMs = delay;
        return c;
    }
};

// 忙等（不是 sleep：sleep 的解析度是 Windows 計時器的 1–16 ms，而且不佔 CPU，和「程式變慢」不像）。
inline void busyWait(double ms) {
    if (ms <= 0) return;
    const auto until = Clock::now() + std::chrono::duration<double, std::milli>(ms);
    while (Clock::now() < until) {
    }
}

// 熱機 warmup 次（丟掉），正式量 runs 次，回傳每次的毫秒數。injectedDelayMs 加在每次正式量測的計時區間裡（自我測試用）。
inline std::vector<double> timeRuns(const std::function<void()>& fn, int warmup, int runs, double injectedDelayMs = 0.0) {
    for (int k = 0; k < warmup; ++k) fn();
    std::vector<double> out;
    out.reserve(static_cast<std::size_t>(std::max(0, runs)));
    for (int k = 0; k < runs; ++k) {
        const auto t0 = Clock::now();
        fn();
        busyWait(injectedDelayMs);
        out.push_back(std::chrono::duration<double, std::milli>(Clock::now() - t0).count());
    }
    return out;
}

inline QString runCommand(const QString& program, const QStringList& args) {
    QProcess p;
    p.setWorkingDirectory(QStringLiteral(AOI_SOURCE_DIR));
    p.start(program, args);
    if (!p.waitForFinished(5000)) return {};
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

// 機器指紋（和 S02–S09 的結果檔相同的欄位）。hygiene：applyMeasurementHygiene() 的回傳值。
inline QJsonObject machineFingerprint(const QString& hygiene, const QString& buildType) {
    QJsonObject m;
#ifdef Q_OS_WIN
    QSettings cpu(QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), QSettings::NativeFormat);
    m["cpu"] = cpu.value(QStringLiteral("ProcessorNameString"), QStringLiteral("unknown")).toString().trimmed();
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) m["ramGB"] = std::round(static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024 * 1024) * 10) / 10;
#endif
    m["logicalCores"] = QThread::idealThreadCount();
    m["os"] = QSysInfo::prettyProductName() + QStringLiteral(" (kernel ") + QSysInfo::kernelVersion() + QLatin1Char(')');
    m["qt"] = QString::fromLatin1(qVersion());
#ifdef _MSC_FULL_VER
    m["compiler"] = QStringLiteral("MSVC %1").arg(_MSC_FULL_VER);
#endif
    m["buildType"] = buildType;
    if (qGuiApp) m["qpaPlatform"] = QGuiApplication::platformName();
    const QString head = runCommand(QStringLiteral("git"), {QStringLiteral("rev-parse"), QStringLiteral("--short"), QStringLiteral("HEAD")});
    const QString dirty = runCommand(QStringLiteral("git"), {QStringLiteral("status"), QStringLiteral("--porcelain")});
    m["commit"] = head.isEmpty() ? QStringLiteral("unknown") : head + (dirty.isEmpty() ? QString() : QStringLiteral("+uncommitted"));
    m["machineName"] = QSysInfo::machineHostName();
    m["measurementHygiene"] = hygiene;
    return m;
}

#ifdef Q_OS_WIN
inline std::uint64_t fileTimeToU64(const FILETIME& t) { return (static_cast<std::uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime; }
#endif

// 量測當下的環境（docs/plan/S10 的任務 8）：整台機器的 CPU 負載（取樣 sampleMs 毫秒的 idle 比例）、每個邏輯 CPU 目前的時脈（MHz）、
// 電源計畫、是否接電源、本行程以外的行程數。用來判斷一次結果的雜訊：負載高、時脈低的那一次不要拿來當基準。
inline QJsonObject environmentSnapshot(int sampleMs = 250) {
    QJsonObject e;
#ifdef Q_OS_WIN
    FILETIME idle0{}, kernel0{}, user0{}, idle1{}, kernel1{}, user1{};
    if (GetSystemTimes(&idle0, &kernel0, &user0)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sampleMs));
        if (GetSystemTimes(&idle1, &kernel1, &user1)) {
            const double idle = static_cast<double>(fileTimeToU64(idle1) - fileTimeToU64(idle0));
            const double total = static_cast<double>((fileTimeToU64(kernel1) - fileTimeToU64(kernel0)) + (fileTimeToU64(user1) - fileTimeToU64(user0)));
            if (total > 0) e["cpuLoadPercent"] = std::round((1.0 - idle / total) * 1000.0) / 10.0;
        }
    }
    // PROCESSOR_POWER_INFORMATION（文件說在 powerbase.h，這台的 SDK 沒有宣告）：自己照文件的版面宣告。
    struct ProcessorPowerInformation {
        ULONG Number, MaxMhz, CurrentMhz, MhzLimit, MaxIdleState, CurrentIdleState;
    };
    const int cores = std::max(1, QThread::idealThreadCount());
    std::vector<ProcessorPowerInformation> info(static_cast<std::size_t>(cores));
    if (CallNtPowerInformation(ProcessorInformation, nullptr, 0, info.data(), static_cast<ULONG>(info.size() * sizeof(ProcessorPowerInformation))) == 0) {
        QJsonArray mhz;
        double sum = 0, maxMhz = 0;
        for (const ProcessorPowerInformation& p : info) {
            mhz.append(static_cast<int>(p.CurrentMhz));
            sum += p.CurrentMhz;
            maxMhz = std::max(maxMhz, static_cast<double>(p.MaxMhz));
        }
        e["cpuMhz"] = mhz;
        e["cpuMhzMean"] = std::round(sum / static_cast<double>(info.size()));
        e["cpuMhzMax"] = maxMhz;
    }
    GUID* scheme = nullptr;
    if (PowerGetActiveScheme(nullptr, &scheme) == ERROR_SUCCESS && scheme != nullptr) {
        DWORD size = 0;
        PowerReadFriendlyName(nullptr, scheme, nullptr, nullptr, nullptr, &size);
        std::vector<UCHAR> buffer(std::max<DWORD>(size, 2));
        if (size > 0 && PowerReadFriendlyName(nullptr, scheme, nullptr, nullptr, buffer.data(), &size) == ERROR_SUCCESS)
            e["powerScheme"] = QString::fromWCharArray(reinterpret_cast<const wchar_t*>(buffer.data()));
        LocalFree(scheme);
    }
    SYSTEM_POWER_STATUS power{};
    if (GetSystemPowerStatus(&power)) {
        if (power.ACLineStatus == 1) e["acPower"] = true;
        else if (power.ACLineStatus == 0) e["acPower"] = false;
        if (power.BatteryLifePercent != 255) e["batteryPercent"] = static_cast<int>(power.BatteryLifePercent);
    }
    DWORD pids[4096];
    DWORD bytes = 0;
    if (EnumProcesses(pids, sizeof(pids), &bytes)) e["processCount"] = static_cast<int>(bytes / sizeof(DWORD));
#endif
    e["sampledAt"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    return e;
}

// 峰值記憶體：背景執行緒每 1 ms 讀一次行程的 private bytes（和 perf_opencv、perf_halcon、perf_ab 相同）。
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
    // 峰值減開始時的值（MB）。
    double finish() {
        if (thread_.joinable()) {
            stop_.store(true);
            thread_.join();
        }
        return static_cast<double>(peak_.load() - std::min(peak_.load(), baseline_)) / (1024.0 * 1024.0);
    }

private:
    static std::size_t privateBytes() {
#ifdef Q_OS_WIN
        PROCESS_MEMORY_COUNTERS_EX pmc{};
        if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) return 0;
        return pmc.PrivateUsage;
#else
        return 0;
#endif
    }
    std::size_t baseline_;
    std::atomic<std::size_t> peak_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

// 結果檔：perf/results/<prefix>_<yyyyMMdd-HHmmss>.json（不入版控；tools/perf-compare.mjs 的 collect 會讀）。回傳路徑。
inline QString writeResult(const QString& prefix, QJsonObject root) {
    root["measuredAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString dir = QStringLiteral(AOI_SOURCE_DIR "/perf/results");
    QDir().mkpath(dir);
    const QString path = dir + QLatin1Char('/') + prefix + QLatin1Char('_') + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")) +
                         QStringLiteral(".json");
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(root).toJson());
    return path;
}

}  // namespace aoi::test::harness
