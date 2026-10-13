// S10 的效能量測（-Perf）：搜尋矩陣。docs/perf-test-plan.md 第 A 節的測試矩陣，每一格 × 每個會讀像素的 back end：
//   影像大小 1／5／12／25 MP × 樣本 32×16／64×32／128×128 × 角度 0／±5／±180° × 元件數 10／100／1 000 × 難度（乾淨、雜訊、缺件 5%、反光、低對比）
// 每一格跑 use case 端到端（教樣本的整條流程：範本 → 種子搜尋 → 晶格 → 視窗驗證），記錄：
//   時間（中位數、p95）、各階段的切片（範本、種子、擬合、驗證；use case 自己回報的 StageTimings）、峰值記憶體、
//   準確度（召回率、位置誤差、誤判；和標準答案比，同 perf_ab）。
//
// 量多少格（docs/steps/S10-perf-harness/spec.md）：
//   quick（預設）：基準格（5 MP、64×32、±5°、200 個元件、雜訊 4）熱機 5 次、量 30 次；從基準格出發，每個維度各變動一次
//                 （共 15 格），其餘的格子熱機 2 次、量 10 次。預設的建置（OpenCV＋HALCON）約 1–2 分鐘。
//   full（AOI_PERF_PROFILE=full）：整個外積 540 格，每格熱機 1 次、量 3 次；要用 test.ps1 -TimeoutSec 3600。
// 基準格就是 T5 的條件（5 MP、200 個元件；UI 的那一段在 perf_e2e），1 000 個元件的那一格就是 T2 的條件（S07、S08 的 841 個）。
// 這裡的 use case 時間只記錄，不當成失敗條件（預算的閘門在 tools/perf-compare.mjs）；準確度帶 T6a／T6b 的預算。
// （S10 時 ADR-004 §5 的 B1–B3 標成 known；2026-10-13 修好之後拿掉，閘門照預算擋。）
//
// 新增一格（或一個維度的值）：改下面的 dimensions()；新增一種難度：difficulties()。每一格就是一列（Qt Test 的資料列），
// log 裡看得到每一格的數字。結果寫到 perf/results/matrix_<時間>.json。
//
// harness 自己的成本（S10 計畫的「效能檢查」）：H-overhead（量一個空的工作：計時、迴圈、峰值取樣執行緒的成本）、
// H-noise（同一個行程裡基準格連量兩次的中位數之比：行程內的雜訊；跨行程的雜訊由 scripts/dev/perf.ps1 -Runs 與 perf-compare noise 算）。
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <vector>

#include "../ui/MeasurementHygiene.h"
#include "HarnessSupport.h"

#include "BackendTestSupport.h"
#include "ContractSupport.h"
#include "synthetic_board.h"

using namespace aoi;
using namespace aoi::contract;
using namespace aoi::test;
using namespace aoi::test::harness;

namespace {

constexpr double kThreshold = 0.70;  // 預設的分數門檻（InspectionSession::Config）

struct Size {
    int w, h;
    const char* name;
};
struct Sample {
    double bodyW, bodyH;
    const char* name;
};
struct Difficulty {
    const char* name;
    std::function<void(SyntheticBoardSpec&)> apply;
};

// ---- 維度 ----
std::vector<Size> sizes() { return {{1280, 800, "1MP"}, {2592, 1944, "5MP"}, {4000, 3000, "12MP"}, {5800, 4350, "25MP"}}; }
// 樣本 ＝ 主體 ＋ 每邊 4 像素（sampleBoxAround 的 margin）：32×16 ＝ 24×8、64×32 ＝ 56×24、128×128 ＝ 120×120。
std::vector<Sample> samples() { return {{24, 8, "32x16"}, {56, 24, "64x32"}, {120, 120, "128x128"}}; }
std::vector<double> angles() { return {0.0, 5.0, 180.0}; }
std::vector<int> partCounts() { return {10, 100, 1000}; }
std::vector<Difficulty> difficulties() {
    return {
        {"clean", [](SyntheticBoardSpec& s) { s.noiseAmplitude = 0; s.blurRadius = 0; }},
        {"noise4", [](SyntheticBoardSpec&) {}},  // 雜訊 4、模糊 1：S07–S09 的標準板子（T2、T6）
        {"noise12", [](SyntheticBoardSpec& s) { s.noiseAmplitude = 12; s.blurRadius = 2; }},
        {"missing5", [](SyntheticBoardSpec& s) { s.missingFraction = 0.05; }},
        {"glare10", [](SyntheticBoardSpec& s) { s.glareFraction = 0.10; }},
        {"lowContrast", [](SyntheticBoardSpec& s) { s.bodyLevel = 72; s.padLevel = 84; }},
    };
}

struct Cell {
    QString id;
    Size size{};
    Sample sample{};
    double angleHalf = 0;
    int targetParts = 0;
    QString difficulty;
    bool base = false;
};

Cell makeCell(const Size& size, const Sample& sample, double angle, int parts, const char* difficulty) {
    Cell c;
    c.size = size;
    c.sample = sample;
    c.angleHalf = angle;
    c.targetParts = parts;
    c.difficulty = QString::fromLatin1(difficulty);
    c.id = QStringLiteral("%1-%2-a%3-n%4-%5").arg(QLatin1String(size.name), QLatin1String(sample.name)).arg(static_cast<int>(angle)).arg(parts).arg(c.difficulty);
    return c;
}

// 基準格：5 MP、64×32、±5°、200 個元件、雜訊 4（T5 的條件）。
Cell baseCell() {
    Cell c = makeCell(sizes()[1], samples()[1], 5.0, 200, "noise4");
    c.base = true;
    return c;
}

// quick：基準格加上每個維度各變動一次；full：整個外積。
std::vector<Cell> cells(const QString& profile) {
    std::vector<Cell> out;
    const auto add = [&out](Cell c) {
        for (const Cell& have : out)
            if (have.id == c.id) return;
        out.push_back(std::move(c));
    };
    if (profile == QLatin1String("full")) {
        for (const Size& s : sizes())
            for (const Sample& p : samples())
                for (double a : angles())
                    for (int n : partCounts())
                        for (const Difficulty& d : difficulties()) {
                            Cell c = makeCell(s, p, a, n, d.name);
                            c.base = c.id == baseCell().id;
                            add(c);
                        }
        add(baseCell());
        return out;
    }
    const Cell base = baseCell();
    add(base);
    for (const Size& s : sizes()) add(makeCell(s, base.sample, base.angleHalf, base.targetParts, "noise4"));
    for (const Sample& p : samples()) add(makeCell(base.size, p, base.angleHalf, base.targetParts, "noise4"));
    for (double a : angles()) add(makeCell(base.size, base.sample, a, base.targetParts, "noise4"));
    for (int n : partCounts()) add(makeCell(base.size, base.sample, base.angleHalf, n, "noise4"));
    for (const Difficulty& d : difficulties()) add(makeCell(base.size, base.sample, base.angleHalf, base.targetParts, d.name));
    return out;
}

// 格距由目標元件數推：可用面積 ÷ 元件數 ＝ 每個元件的面積，長寬比照 S07 的板子（84：60）；格距至少要放得下樣本框
// （主體 ＋ 每邊 4 像素的範本，再留 25%），放不下目標數量時元件變少（實際的數量寫進結果）。
SyntheticBoardSpec specFor(const Cell& c) {
    SyntheticBoardSpec s;
    s.width = c.size.w;
    s.height = c.size.h;
    s.bodyWidthPx = c.sample.bodyW;
    s.bodyHeightPx = c.sample.bodyH;
    s.originXPx = std::max(120.0, c.sample.bodyW);
    s.originYPx = std::max(110.0, c.sample.bodyH);
    const double usableW = c.size.w - 2 * s.originXPx;
    const double usableH = c.size.h - 2 * s.originYPx;
    const double area = usableW * usableH / static_cast<double>(c.targetParts);
    double pitchX = std::sqrt(area * 84.0 / 60.0);
    double pitchY = area / pitchX;
    pitchX = std::max(pitchX, (c.sample.bodyW + 8.0) * 1.25);
    pitchY = std::max(pitchY, (c.sample.bodyH + 8.0) * 1.25);
    s.pitchXPx = std::round(pitchX);
    s.pitchYPx = std::round(pitchY);
    s.cols = std::max(1, static_cast<int>(usableW / s.pitchXPx) + 1);
    s.rows = std::max(1, static_cast<int>(usableH / s.pitchYPx) + 1);
    // 目標數量比格子少時，依比例砍列與欄（10 個元件 ＝ 2 列 5 欄那樣的小陣列，不是 1 列）。
    while (s.rows * s.cols > c.targetParts && s.rows > 1 && s.cols > 1) {
        if (s.cols >= s.rows * 84 / 60) --s.cols;
        else --s.rows;
    }
    s.noiseAmplitude = 4;
    s.blurRadius = 1;
    s.tiltDeg = 0.0;
    s.seed = 17;
    for (const Difficulty& d : difficulties())
        if (c.difficulty == QLatin1String(d.name)) d.apply(s);
    return s;
}

// 和標準答案比（扣掉樣本框中心的固定偏移；同 perf_ab、tst_contract_end_to_end）。
struct CellAccuracy {
    std::size_t present = 0, found = 0, falseFound = 0;
    std::vector<double> errors;
};

CellAccuracy evaluate(const SyntheticBoard& board, const SampleProposal& p) {
    CellAccuracy a;
    for (const SyntheticPart& part : board.parts) a.present += part.present ? 1u : 0u;
    if (p.candidates.empty()) return a;
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

}  // namespace

class PerfMatrix : public QObject {
    Q_OBJECT

private:
    QString hygiene_;
    RunConfig config_;
    QJsonObject environmentStart_;
    std::vector<std::string> ids_;
    QString missing_;
    std::vector<Metric> metrics_;
    QJsonArray cells_;
    QString cachedCellId_;
    std::unique_ptr<SyntheticBoard> cachedBoard_;

    const SyntheticBoard& boardFor(const Cell& c) {
        if (cachedCellId_ != c.id) {
            cachedBoard_ = std::make_unique<SyntheticBoard>(makeSyntheticBoard(specFor(c)));
            cachedCellId_ = c.id;
        }
        return *cachedBoard_;
    }

    void add(Metric m) {
        qInfo().noquote() << QStringLiteral("%1  n=%2  median %3  p95 %4  max %5 %6  budget %7  %8")
                                 .arg(m.id, -46).arg(m.stats.n, 3).arg(m.stats.median, 9, 'f', 3).arg(m.stats.p95, 9, 'f', 3)
                                 .arg(m.stats.max, 9, 'f', 3).arg(m.unit, -7)
                                 .arg(m.budget < 0 ? QStringLiteral("—") : QString::number(m.budget))
                                 .arg(m.budget < 0 ? QString() : (m.pass() ? QStringLiteral("PASS") : (m.known.isEmpty() ? QStringLiteral("OVER") : QStringLiteral("OVER (known)"))));
        metrics_.push_back(std::move(m));
    }

private slots:
    void initTestCase() {
        hygiene_ = applyMeasurementHygiene();
        config_ = RunConfig::fromEnvironment(5, 30);
        environmentStart_ = environmentSnapshot();
        qInfo().noquote() << "measurement hygiene:" << hygiene_;
        qInfo().noquote() << "profile:" << config_.profile << " warmup/runs (base cell):" << config_.warmup << "/" << config_.runs
                          << " injected delay ms:" << config_.injectedDelayMs;
        qInfo().noquote() << "environment:" << QJsonDocument(environmentStart_).toJson(QJsonDocument::Compact);
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

    // harness 自己的成本：一個空的工作，走同一套計時迴圈與峰值取樣。
    void harnessOverhead() {
        PeakSampler peak;
        const std::vector<double> ms = timeRuns([] {}, 5, 30);
        Metric m;
        m.id = QStringLiteral("H-overhead");
        m.label = QStringLiteral("harness overhead: an empty case through the timing loop with the peak sampler running");
        m.stats = summarize(ms);
        m.note = QStringLiteral("peak sampler added %1 MB").arg(peak.finish(), 0, 'f', 3);
        add(m);
    }

    void matrix_data() {
        QTest::addColumn<int>("cellIndex");
        QTest::addColumn<QString>("backend");
        const std::vector<Cell> all = cells(config_.profile);
        for (int i = 0; i < static_cast<int>(all.size()); ++i)
            for (const std::string& id : ids_)
                QTest::newRow(qPrintable(QString::fromStdString(id) + QLatin1Char('|') + all[static_cast<std::size_t>(i)].id)) << i << QString::fromStdString(id);
    }

    void matrix() {
        QFETCH(int, cellIndex);
        QFETCH(QString, backend);
        const Cell cell = cells(config_.profile)[static_cast<std::size_t>(cellIndex)];
        const std::string id = backend.toStdString();
        const SyntheticBoard& board = boardFor(cell);
        QVERIFY2(board.image.valid(), "the synthetic board could not be made");
        const SearchEngine& engine = *engineFor(id).engine;

        SampleRequest request;
        request.image = board.image;
        request.sample = sampleBoxAround(board, middlePresentIndex(board));
        request.search.angleMinDeg = -cell.angleHalf;
        request.search.angleMaxDeg = cell.angleHalf;

        const int warmup = cell.base ? config_.warmup : (config_.profile == QLatin1String("full") ? 1 : 2);
        const int runs = cell.base ? config_.runs : (config_.profile == QLatin1String("full") ? 3 : 10);
        SampleProposal last;
        std::vector<double> templateMs, seedMs, fitMs, verifyMs;
        bool failed = false;
        std::string failure;
        const auto once = [&] {
            Result<SampleProposal> r = engine.generate->run(request, CancelToken());
            if (!r.ok()) {
                failed = true;
                failure = r.error().message;
                return;
            }
            last = std::move(r.value());
            templateMs.push_back(last.timings.templateMs);
            seedMs.push_back(last.timings.seedMs);
            fitMs.push_back(last.timings.fitMs);
            verifyMs.push_back(last.timings.verifyMs);
        };
        // 熱機在峰值取樣之前（同 perf_ab）：HALCON 第一次處理某個大小的影像會配置工作緩衝（實測 5 MP 約 1.1 GB、12 MP 約 1.9 GB），
        // 那是一次性的；峰值要量的是穩定狀態下每次搜尋多用多少。
        for (int k = 0; k < warmup; ++k) once();
        templateMs.clear();
        seedMs.clear();
        fitMs.clear();
        verifyMs.clear();
        PeakSampler peak;
        const std::vector<double> total = timeRuns(once, 0, runs, config_.injectedDelayMs);
        const double peakMb = peak.finish();
        QVERIFY2(!failed, qPrintable(QStringLiteral("%1: the use case failed: %2").arg(backend, QString::fromStdString(failure))));
        const CellAccuracy a = evaluate(board, last);
        const double recall = a.present == 0 ? 0.0 : 100.0 * static_cast<double>(a.found) / static_cast<double>(a.present);
        const QString prefix = backend + QLatin1Char(':') + cell.id + QLatin1Char(':');
        const QString where = QStringLiteral("%1, %2 sample, +-%3 deg, %4 parts, %5, %6")
                                  .arg(QLatin1String(cell.size.name), QLatin1String(cell.sample.name)).arg(cell.angleHalf)
                                  .arg(a.present).arg(cell.difficulty, backend);

        Metric t;
        t.id = prefix + QStringLiteral("total");
        t.label = QStringLiteral("use case end to end: ") + where;
        t.stats = summarize(total);
        // 1 000 個元件的基準條件就是 T2（種子搜尋，S07／S08 的 841 個）；use case 的整條沒有預算（T5 是 UI 的那一段，perf_e2e）。
        t.note = QStringLiteral("warmup %1, runs %2; lattice %3").arg(warmup).arg(runs).arg(last.grid.found ? QStringLiteral("found") : QStringLiteral("none"));
        add(t);
        const auto stage = [&](const char* name, const std::vector<double>& ms) {
            Metric s;
            s.id = prefix + QLatin1String(name);
            s.label = QStringLiteral("stage %1: %2").arg(QLatin1String(name), where);
            s.stats = summarize(ms);
            if (QLatin1String(name) == QLatin1String("seed") && cell.difficulty == QLatin1String("noise4") && cell.sample.bodyW == 56.0 &&
                cell.angleHalf == 5.0 && cell.targetParts == 1000 && cell.size.w == 2592) {
                s.budgetId = id == "halcon" ? QStringLiteral("T2b") : QStringLiteral("T2a");
                s.budget = budgetOf(s.budgetId);
            }
            add(s);
        };
        stage("template", templateMs);
        stage("seed", seedMs);
        stage("fit", fitMs);
        stage("verify", verifyMs);

        Metric mem;
        mem.id = prefix + QStringLiteral("peakMb");
        mem.label = QStringLiteral("peak private bytes over the runs: ") + where;
        mem.unit = QStringLiteral("MB");
        mem.stats = summarize({peakMb});
        add(mem);

        Metric rec;
        rec.id = prefix + QStringLiteral("recall");
        rec.label = QStringLiteral("recall at threshold 0.70: ") + where;
        rec.unit = QStringLiteral("percent");
        rec.stats = summarize({recall});
        rec.higherIsBetter = true;
        rec.budgetId = QStringLiteral("T6a");
        rec.budget = budgetOf(rec.budgetId);
        rec.note = QStringLiteral("%1 of %2 parts; %3 false at 0.70").arg(a.found).arg(a.present).arg(a.falseFound);
        add(rec);

        Metric err;
        err.id = prefix + QStringLiteral("errorPx");
        err.label = QStringLiteral("position error of the found parts: ") + where;
        err.unit = QStringLiteral("px");
        err.stats = summarize(a.errors);
        err.budgetId = QStringLiteral("T6b");
        err.budget = budgetOf(err.budgetId);
        add(err);

        QJsonObject row;
        row["cell"] = cell.id;
        row["backend"] = backend;
        row["base"] = cell.base;
        row["imageSize"] = QLatin1String(cell.size.name);
        row["imagePixels"] = QJsonArray{cell.size.w, cell.size.h};
        row["sample"] = QLatin1String(cell.sample.name);
        row["angleHalfDeg"] = cell.angleHalf;
        row["targetParts"] = cell.targetParts;
        row["difficulty"] = cell.difficulty;
        row["present"] = static_cast<int>(a.present);
        row["warmup"] = warmup;
        row["runs"] = runs;
        row["totalMedianMs"] = round3(percentile(total, 0.5));
        row["totalP95Ms"] = round3(percentile(total, 0.95));
        row["templateMedianMs"] = round3(percentile(templateMs, 0.5));
        row["seedMedianMs"] = round3(percentile(seedMs, 0.5));
        row["fitMedianMs"] = round3(percentile(fitMs, 0.5));
        row["verifyMedianMs"] = round3(percentile(verifyMs, 0.5));
        row["peakMb"] = round3(peakMb);
        row["found"] = static_cast<int>(a.found);
        row["falseFound"] = static_cast<int>(a.falseFound);
        row["recallPercent"] = round3(recall);
        row["errorP95Px"] = round3(percentile(a.errors, 0.95));
        row["errorMaxPx"] = round3(a.errors.empty() ? 0.0 : *std::max_element(a.errors.begin(), a.errors.end()));
        row["lattice"] = last.grid.found;
        cells_.append(row);
    }

    // 行程內的雜訊：基準格再量一次（每個 back end），和第一次的中位數比。
    void withinProcessNoise() {
        const Cell base = baseCell();
        const SyntheticBoard& board = boardFor(base);
        for (const std::string& id : ids_) {
            const SearchEngine& engine = *engineFor(id).engine;
            SampleRequest request;
            request.image = board.image;
            request.sample = sampleBoxAround(board, middlePresentIndex(board));
            request.search.angleMinDeg = -base.angleHalf;
            request.search.angleMaxDeg = base.angleHalf;
            const auto run = [&] {
                std::vector<double> ms = timeRuns([&] { (void)engine.generate->run(request, CancelToken()); }, 2, 10, config_.injectedDelayMs);
                return percentile(ms, 0.5);
            };
            const double first = run();
            const double second = run();
            Metric m;
            m.id = QStringLiteral("H-noise:") + QString::fromStdString(id);
            m.label = QStringLiteral("within-process noise: ratio of two medians (10 runs each) of the base cell, %1").arg(QString::fromStdString(id));
            m.unit = QStringLiteral("ratio");
            m.stats = summarize({std::max(first, second) / std::max(1e-9, std::min(first, second))});
            m.note = QStringLiteral("medians %1 and %2 ms").arg(first, 0, 'f', 2).arg(second, 0, 'f', 2);
            add(m);
        }
    }

    void cleanupTestCase() {
        if (metrics_.empty()) return;
        QJsonArray rows;
        for (const Metric& m : metrics_) rows.append(toJson(m));
        QJsonObject root;
        root["schema"] = 1;
        root["kind"] = QStringLiteral("perf_matrix");
        root["step"] = QStringLiteral("S10");
        root["machine"] = machineFingerprint(hygiene_, QStringLiteral(AOI_BUILD_TYPE));
        root["environment"] = QJsonObject{{"start", environmentStart_}, {"end", environmentSnapshot()}};
        root["profile"] = config_.profile;
        root["warmup"] = config_.warmup;
        root["runs"] = config_.runs;
        root["injectedDelayMs"] = config_.injectedDelayMs;
        root["method"] = QStringLiteral("use case end to end per cell; base cell warmup %1 runs %2, other cells %3; "
                                        "stage times from SampleProposal::timings; peak = private bytes over the runs; accuracy against the ground truth at threshold 0.70")
                             .arg(config_.warmup).arg(config_.runs)
                             .arg(config_.profile == QLatin1String("full") ? QStringLiteral("1/3") : QStringLiteral("2/10"));
        root["backends"] = [&] {
            QJsonArray a;
            for (const std::string& id : ids_) a.append(QString::fromStdString(id));
            return a;
        }();
        if (!missing_.isEmpty()) root["skippedBackends"] = missing_.trimmed();
        root["metrics"] = rows;
        root["cells"] = cells_;
        qInfo("results: %s", qPrintable(writeResult(QStringLiteral("matrix"), root)));
    }
};

QTEST_GUILESS_MAIN(PerfMatrix)
#include "perf_matrix.moc"
