// S09 的效能量測（-Perf）：A/B 並排。組裝根註冊的每一個會讀像素的 back end（沒編進來或沒有授權的跳過並說原因），
// 同一組合成板子（5 MP；T6 的情況，加上光照不均、低對比、重模糊、大片反光），跑 use case 端到端：
//   時間（中位數、p95）、準確度（召回率、位置誤差 p95／最大、門檻 0.70 時的誤判）、分數的分布（兩邊的分數不是同一把尺）、峰值記憶體。
// 兩個 back end 都能用時，每一種情況再用 application 的 compareProposals 比一次（兩邊都用預設的門檻 0.70）：
// 兩邊都找到、只有 A、只有 B 與原因——和 A/B 比較畫面同一個定義。
// 結果寫到 perf/results/ab_<時間>.json；每一種情況的 A/B 比較另外寫到 <建置>/test-work/ab-<情況>.json（explainer.html 可以載入）。
// 方法（docs/perf-test-plan.md）：熱機 1 次、正式量 5 次（每次是整條 use case，5 MP），中位數與 p95。時間只記錄，不當成失敗條件；
// 這裡沒有新的預算（S09 計畫：並排比較，不是新的閘門）。
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
#include "ContractSupport.h"
#include "application/compare/BackendComparison.h"
#include "domain/image_buffer.h"
#include "synthetic_board.h"

using namespace aoi;
using namespace aoi::contract;
using namespace aoi::test;
using Clock = std::chrono::steady_clock;

namespace {

constexpr double kThreshold = 0.70;  // 預設的分數門檻（InspectionSession::Config），兩邊一樣
constexpr int kWarmup = 1;
constexpr int kRuns = 5;

double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const auto rank = static_cast<std::size_t>(std::ceil(p * static_cast<double>(v.size())));
    return v[std::min(v.size(), std::max<std::size_t>(1, rank)) - 1];
}

double round3(double v) { return std::round(v * 1000.0) / 1000.0; }

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

// 峰值記憶體：背景執行緒每 1 ms 讀一次行程的 private bytes（和 perf_opencv、perf_halcon 相同）。
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

// 5 MP 的板子，64×32 的樣本（主體 56×24 ＋ 每邊 4 像素）：和 perf_opencv、perf_halcon 的 T2、T6 同一塊。
SyntheticBoardSpec boardSpec() {
    SyntheticBoardSpec s;
    s.width = 2592;
    s.height = 1944;
    s.bodyWidthPx = 56.0;
    s.bodyHeightPx = 24.0;
    s.pitchXPx = 84.0;
    s.pitchYPx = 60.0;
    s.originXPx = 120.0;
    s.originYPx = 110.0;
    s.cols = static_cast<int>((s.width - 2 * s.originXPx) / s.pitchXPx) + 1;
    s.rows = static_cast<int>((s.height - 2 * s.originYPx) / s.pitchYPx) + 1;
    s.noiseAmplitude = 4;
    s.blurRadius = 1;
    s.seed = 17;
    return s;
}

// 光照不均：亮度從左邊的 leftGain 線性變到右邊的 1.0（相機的照明不均、鏡頭暗角）。Gray8 才做。
SyntheticBoard withGradient(SyntheticBoard board, double leftGain) {
    ImageBuffer out(board.image.width, board.image.height, PixelFormat::Gray8);
    for (int y = 0; y < board.image.height; ++y) {
        const std::uint8_t* src = board.image.data + static_cast<std::size_t>(y) * static_cast<std::size_t>(board.image.strideBytes);
        std::uint8_t* dst = out.row(y);
        for (int x = 0; x < board.image.width; ++x) {
            const double gain = leftGain + (1.0 - leftGain) * static_cast<double>(x) / static_cast<double>(board.image.width - 1);
            dst[x] = static_cast<std::uint8_t>(std::clamp(std::lround(src[x] * gain), 0L, 255L));
        }
    }
    board.image = out.view(board.image.umPerPixel);
    return board;
}

struct Scenario {
    QString id;      // 檔名用
    QString label;
    std::function<SyntheticBoard()> make;
};

std::vector<Scenario> scenarios() {
    const auto spec = [](const std::function<void(SyntheticBoardSpec&)>& tweak) {
        return [tweak] {
            SyntheticBoardSpec s = boardSpec();
            tweak(s);
            return makeSyntheticBoard(s);
        };
    };
    return {
        {"clean", "clean (no noise, no blur)", spec([](SyntheticBoardSpec& s) { s.noiseAmplitude = 0; s.blurRadius = 0; })},
        {"noise4", "noise 4, blur 1 (T2, T6)", spec([](SyntheticBoardSpec&) {})},
        {"noise12", "noise 12, blur 2", spec([](SyntheticBoardSpec& s) { s.noiseAmplitude = 12; s.blurRadius = 2; })},
        {"noise25", "noise 25, blur 1", spec([](SyntheticBoardSpec& s) { s.noiseAmplitude = 25; })},
        {"blur4", "heavy blur (radius 4)", spec([](SyntheticBoardSpec& s) { s.blurRadius = 4; })},
        {"missing10", "10% missing parts", spec([](SyntheticBoardSpec& s) { s.missingFraction = 0.10; })},
        {"glare10", "10% glare", spec([](SyntheticBoardSpec& s) { s.glareFraction = 0.10; })},
        {"glare40", "40% glare", spec([](SyntheticBoardSpec& s) { s.glareFraction = 0.40; })},
        {"lowContrast", "low contrast (background 60, body 72, pads 84)",
         spec([](SyntheticBoardSpec& s) { s.bodyLevel = 72; s.padLevel = 84; })},
        {"gradient", "uneven light (gain 0.35 at the left edge to 1.0 at the right)",
         [] { return withGradient(makeSyntheticBoard(boardSpec()), 0.35); }},
        {"tilt3", "tilt 3 deg", spec([](SyntheticBoardSpec& s) { s.tiltDeg = 3.0; })},
        {"bits12", "12 bit (Gray16)", spec([](SyntheticBoardSpec& s) { s.format = PixelFormat::Gray16; s.grayBits = 12; })},
    };
}

// 和標準答案比（扣掉樣本框中心的固定偏移，同 tst_contract_end_to_end）。
struct AbAccuracy {
    std::size_t present = 0;
    std::size_t scored = 0;     // 有分數的候選在附近（不論門檻）
    std::size_t found = 0;      // 其中 Sample 或分數 ≥ 門檻
    std::size_t falseFound = 0; // 分數 ≥ 門檻的候選，附近沒有元件
    std::vector<double> errors; // found 的位置誤差
    std::vector<double> scores; // found 與 scored 的分數（Sample 除外）
};

AbAccuracy evaluate(const SyntheticBoard& board, const SampleProposal& p) {
    AbAccuracy a;
    if (p.candidates.empty()) {
        for (const SyntheticPart& part : board.parts) a.present += part.present ? 1u : 0u;
        return a;
    }
    const Candidate& sample = p.candidates.front();
    Point2D offset{};
    double nearest = std::numeric_limits<double>::max();
    for (const SyntheticPart& part : board.parts) {
        const double d = distance(sample.pose.position(), part.pose.position());
        if (part.present && d < nearest) {
            nearest = d;
            offset = {sample.pose.x - part.pose.x, sample.pose.y - part.pose.y};
        }
    }
    for (const SyntheticPart& part : board.parts) {
        if (!part.present) continue;
        ++a.present;
        const Point2D expected{part.pose.x + offset.x, part.pose.y + offset.y};
        const Candidate* best = nullptr;
        double bestDistance = std::numeric_limits<double>::max();
        for (const Candidate& c : p.candidates) {
            if (!c.hasScore()) continue;
            const double d = distance(c.pose.position(), expected);
            if (d < bestDistance) {
                bestDistance = d;
                best = &c;
            }
        }
        if (best == nullptr || bestDistance > 3.0) continue;
        ++a.scored;
        if (best->source != CandidateSource::Sample) a.scores.push_back(best->score);
        if (best->source == CandidateSource::Sample || best->score >= kThreshold) {
            ++a.found;
            a.errors.push_back(bestDistance);
        }
    }
    for (const Candidate& c : p.candidates) {
        if (!c.hasScore() || (c.source != CandidateSource::Sample && c.score < kThreshold)) continue;
        const bool nearPart = std::any_of(board.parts.begin(), board.parts.end(), [&](const SyntheticPart& part) {
            return part.present && distance(c.pose.position(), {part.pose.x + offset.x, part.pose.y + offset.y}) <= 3.0;
        });
        if (!nearPart) ++a.falseFound;
    }
    return a;
}

const char* kindName(CompareKind k) {
    switch (k) {
        case CompareKind::Both: return "both";
        case CompareKind::OnlyA: return "onlyA";
        case CompareKind::OnlyB: return "onlyB";
    }
    return "?";
}

const char* gapName(CompareGap g) {
    switch (g) {
        case CompareGap::None: return "none";
        case CompareGap::BelowThreshold: return "belowThreshold";
        case CompareGap::InferredOnly: return "inferredOnly";
        case CompareGap::NotFound: return "notFound";
    }
    return "?";
}

QJsonValue scoreJson(const CompareHit& hit) {
    if (!hit.present) return QJsonValue(QJsonValue::Null);
    if (!hit.score) return QStringLiteral("inferred");
    return round3(*hit.score);
}

struct SideRun {
    std::string id;
    SampleProposal proposal;  // 最後一次的結果
    std::vector<double> ms;
    double peakMb = 0.0;
    AbAccuracy accuracy;
};

}  // namespace

class PerfAb : public QObject {
    Q_OBJECT

private:
    QString hygiene_;
    QJsonArray rows_;
    QJsonArray comparisons_;
    std::vector<std::string> ids_;
    QString missing_;  // 不能用的 back end 與原因（A/B 比較跳過時寫進 SKIP 的訊息）

    SideRun runSide(const std::string& id, const SyntheticBoard& board) {
        SideRun side;
        side.id = id;
        const SearchEngine& engine = *engineFor(id).engine;
        SampleRequest request;
        request.image = board.image;
        request.sample = sampleBoxAround(board, middlePresentIndex(board));
        for (int k = 0; k < kWarmup; ++k) (void)engine.generate->run(request, CancelToken());
        PeakSampler peak;
        for (int k = 0; k < kRuns; ++k) {
            const auto t0 = Clock::now();
            Result<SampleProposal> r = engine.generate->run(request, CancelToken());
            side.ms.push_back(std::chrono::duration<double, std::milli>(Clock::now() - t0).count());
            if (r.ok()) side.proposal = std::move(r.value());
            else qWarning("%s: %s", id.c_str(), r.error().message.c_str());
        }
        side.peakMb = peak.finish();
        side.accuracy = evaluate(board, side.proposal);
        return side;
    }

private slots:
    void initTestCase() {
        hygiene_ = applyMeasurementHygiene();
        for (const std::string& id : backendIds(Rows::Vision)) {
            const ContractEngine& e = engineFor(id);
            if (e.engine) ids_.push_back(id);
            else {
                const std::string reason = e.skipReason.empty() ? e.failure : e.skipReason;
                qInfo("%s SKIPPED: %s", id.c_str(), reason.c_str());
                missing_ += QString::fromStdString(reason) + QLatin1Char(' ');
            }
        }
        if (ids_.empty()) QSKIP("no back end that reads pixels is usable in this build");
    }

    void sideBySide_data() {
        QTest::addColumn<int>("index");
        const std::vector<Scenario> all = scenarios();
        for (int i = 0; i < static_cast<int>(all.size()); ++i) QTest::newRow(qPrintable(all[static_cast<std::size_t>(i)].id)) << i;
    }

    void sideBySide() {
        QFETCH(int, index);
        const Scenario scenario = scenarios()[static_cast<std::size_t>(index)];
        const SyntheticBoard board = scenario.make();
        QVERIFY(board.image.valid());
        std::vector<SideRun> sides;
        for (const std::string& id : ids_) sides.push_back(runSide(id, board));

        for (const SideRun& s : sides) {
            const AbAccuracy& a = s.accuracy;
            const double recall = a.present == 0 ? 0.0 : 100.0 * static_cast<double>(a.found) / static_cast<double>(a.present);
            qInfo().noquote() << QStringLiteral("%1 | %2: total median %3 ms p95 %4 ms | recall@0.70 %5% (%6/%7, scored %8) false %9 | "
                                                "error p95 %10 max %11 px | score median %12 min %13 | peak +%14 MB")
                                     .arg(scenario.id, -12)
                                     .arg(QString::fromStdString(s.id), -6)
                                     .arg(percentile(s.ms, 0.5), 0, 'f', 1)
                                     .arg(percentile(s.ms, 0.95), 0, 'f', 1)
                                     .arg(recall, 0, 'f', 1)
                                     .arg(a.found)
                                     .arg(a.present)
                                     .arg(a.scored)
                                     .arg(a.falseFound)
                                     .arg(percentile(a.errors, 0.95), 0, 'f', 3)
                                     .arg(a.errors.empty() ? 0.0 : *std::max_element(a.errors.begin(), a.errors.end()), 0, 'f', 3)
                                     .arg(percentile(a.scores, 0.5), 0, 'f', 3)
                                     .arg(a.scores.empty() ? 0.0 : *std::min_element(a.scores.begin(), a.scores.end()), 0, 'f', 3)
                                     .arg(s.peakMb, 0, 'f', 1);
            QJsonObject row;
            row["scenario"] = scenario.id;
            row["label"] = scenario.label;
            row["backend"] = QString::fromStdString(s.id);
            row["n"] = static_cast<int>(s.ms.size());
            row["totalMedianMs"] = round3(percentile(s.ms, 0.5));
            row["totalP95Ms"] = round3(percentile(s.ms, 0.95));
            row["templateMs"] = round3(s.proposal.timings.templateMs);
            row["seedMs"] = round3(s.proposal.timings.seedMs);
            row["fitMs"] = round3(s.proposal.timings.fitMs);
            row["verifyMs"] = round3(s.proposal.timings.verifyMs);
            row["present"] = static_cast<int>(a.present);
            row["found"] = static_cast<int>(a.found);
            row["scored"] = static_cast<int>(a.scored);
            row["falseFound"] = static_cast<int>(a.falseFound);
            row["recallPercent"] = round3(recall);
            row["errorP95Px"] = round3(percentile(a.errors, 0.95));
            row["errorMaxPx"] = round3(a.errors.empty() ? 0.0 : *std::max_element(a.errors.begin(), a.errors.end()));
            row["scoreMedian"] = round3(percentile(a.scores, 0.5));
            row["scoreP05"] = round3(percentile(a.scores, 0.05));
            row["lattice"] = s.proposal.grid.found;
            row["peakMb"] = round3(s.peakMb);
            rows_.append(row);
        }

        // 只有一個 back end 能用：它的數字照樣記下（上面），A/B 的比較這一列明確標示 SKIPPED（不是 PASSED）。
        if (sides.size() < 2) QSKIP(qPrintable(QStringLiteral("the A/B comparison needs two back ends that read pixels; ") + missing_));
        const SideRun& a = sides[0];
        const SideRun& b = sides[1];
        const BackendComparison c = compareProposals(a.proposal, kThreshold, b.proposal, kThreshold);
        const CompareSummary& s = c.summary;
        qInfo().noquote() << QStringLiteral("%1 | A/B (%2 vs %3, both 0.70): both %4, only %2 %5 (B below threshold %6, inferred %7, none %8), "
                                            "only %3 %9 (A below threshold %10, inferred %11, none %12) | same part: mean %13 max %14 px, angle max %15 deg")
                                 .arg(scenario.id, -12)
                                 .arg(QString::fromStdString(a.id), QString::fromStdString(b.id))
                                 .arg(s.both)
                                 .arg(s.onlyA)
                                 .arg(s.onlyABelowThreshold)
                                 .arg(s.onlyAInferredOnly)
                                 .arg(s.onlyANotFound)
                                 .arg(s.onlyB)
                                 .arg(s.onlyBBelowThreshold)
                                 .arg(s.onlyBInferredOnly)
                                 .arg(s.onlyBNotFound)
                                 .arg(s.meanDistancePx, 0, 'f', 3)
                                 .arg(s.maxDistancePx, 0, 'f', 3)
                                 .arg(s.maxAngleDiffDeg, 0, 'f', 2);
        QJsonObject summary;
        summary["scenario"] = scenario.id;
        summary["a"] = QString::fromStdString(a.id);
        summary["b"] = QString::fromStdString(b.id);
        summary["both"] = static_cast<int>(s.both);
        summary["onlyA"] = static_cast<int>(s.onlyA);
        summary["onlyABelowThreshold"] = static_cast<int>(s.onlyABelowThreshold);
        summary["onlyAInferredOnly"] = static_cast<int>(s.onlyAInferredOnly);
        summary["onlyANotFound"] = static_cast<int>(s.onlyANotFound);
        summary["onlyB"] = static_cast<int>(s.onlyB);
        summary["onlyBBelowThreshold"] = static_cast<int>(s.onlyBBelowThreshold);
        summary["onlyBInferredOnly"] = static_cast<int>(s.onlyBInferredOnly);
        summary["onlyBNotFound"] = static_cast<int>(s.onlyBNotFound);
        summary["meanDistancePx"] = round3(s.meanDistancePx);
        summary["maxDistancePx"] = round3(s.maxDistancePx);
        summary["maxAngleDiffDeg"] = round3(s.maxAngleDiffDeg);
        comparisons_.append(summary);

        // explainer.html 可以載入的 A/B 結果：每個比較過的位置一筆 [kind, x, y, scoreA, scoreB, gap]。
        QJsonObject out;
        out["schema"] = QStringLiteral("aoi-ab-comparison/1");
        out["scenario"] = scenario.id;
        out["label"] = scenario.label;
        out["image"] = QJsonArray{board.image.width, board.image.height};
        out["partSize"] = QJsonArray{round3(a.proposal.partSize.w), round3(a.proposal.partSize.h)};
        out["threshold"] = QJsonArray{kThreshold, kThreshold};
        out["a"] = QJsonObject{{"id", QString::fromStdString(a.id)}, {"totalMs", round3(percentile(a.ms, 0.5))},
                               {"found", static_cast<int>(s.a.found)}, {"inferred", static_cast<int>(s.a.inferred)},
                               {"rejected", static_cast<int>(s.a.rejected)}};
        out["b"] = QJsonObject{{"id", QString::fromStdString(b.id)}, {"totalMs", round3(percentile(b.ms, 0.5))},
                               {"found", static_cast<int>(s.b.found)}, {"inferred", static_cast<int>(s.b.inferred)},
                               {"rejected", static_cast<int>(s.b.rejected)}};
        out["summary"] = summary;
        QJsonArray items;
        for (const ComparedItem& item : c.items) {
            const Pose2D p = item.pose();
            items.append(QJsonArray{QString::fromLatin1(kindName(item.kind)), round3(p.x), round3(p.y), scoreJson(item.a), scoreJson(item.b),
                                    QString::fromLatin1(gapName(item.gap))});
        }
        out["items"] = items;
        // 測試的工作資料夾（scripts/dev/test.ps1 是 <建置>/test-work；直接執行時是目前的資料夾）。
        QFile f(QDir::current().filePath(QStringLiteral("ab-%1.json").arg(scenario.id)));
        if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(out).toJson(QJsonDocument::Compact));
    }

    void cleanupTestCase() {
        if (rows_.isEmpty()) return;
        QJsonObject root;
        root["schema"] = 1;
        root["kind"] = QStringLiteral("perf_ab");
        root["step"] = QStringLiteral("S09");
        root["measuredAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        root["machine"] = machineFingerprint(hygiene_);
        root["method"] = QStringLiteral("use case end to end on 5 MP synthetic boards; warmup %1, runs %2 per back end and scenario; "
                                        "threshold 0.70 on both sides; peak = private bytes over the runs")
                             .arg(kWarmup)
                             .arg(kRuns);
        root["runs"] = rows_;
        root["comparisons"] = comparisons_;
        QDir().mkpath(QStringLiteral(AOI_SOURCE_DIR "/perf/results"));
        const QString path = QStringLiteral(AOI_SOURCE_DIR "/perf/results/ab_%1.json").arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss"));
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(root).toJson());
        qInfo("results: %s", qPrintable(path));
    }
};

QTEST_GUILESS_MAIN(PerfAb)
#include "perf_ab.moc"
