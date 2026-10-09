// S09 的 UI 效能量測（-Perf）：A/B 比較視窗。U1 類（「結果準備好 → 第一個畫完的畫面」，預算 U1a 200 個框 33 ms、U1b 5 000 個框 50 ms）
// 套到 A/B 的疊圖；另外量「改一邊的門檻 → 重新比較（不重新搜尋）→ 表格與疊圖畫完」（U3 類，參考值，主畫布的 U3 預算 16 ms）。
// 方法同 S04（perf_ui_s04）：丟掉前 5 次熱機，正式量 30 次，中位數與 p95；offscreen 平台的軟體算繪 CPU 時間。
//
// 比較視窗與 ComparePresenter 照產品的方式接（ICompareView、ICompareController），兩個 back end 是兩個假世界
// （同樣的格子，弱的格子不同，所以有差異），沒有執行緒池（同步跑），量的只有 UI 這一側。
#include <QElapsedTimer>
#include <QTableWidget>
#include <QtTest>

#include "MeasurementHygiene.h"
#include "PerfSupport.h"
#include "TestSupport.h"
#include "application/backend/SearchEngine.h"
#include "application/compare/BackendComparison.h"
#include "infrastructure/backend_fake/FakeBackend.h"
#include "presentation/canvas/ImageCanvas.h"
#include "presentation/compare/CompareOverlayItem.h"
#include "presentation/compare/ComparePresenter.h"
#include "presentation/compare/CompareWindow.h"
#include "presentation/dev/FakeScene.h"
#include "presentation/i18n/Translations.h"
#include "presentation/theme/ThemeTokens.h"

using namespace aoi;
using namespace aoi::ui;
using namespace aoi::test;
using aoi::test::perf::Metric;

namespace {

constexpr int kWarmup = 5;
constexpr int kRuns = 30;

std::vector<Metric>& metrics() {
    static std::vector<Metric> m;
    return m;
}

void record(const QString& id, const QString& label, const std::vector<double>& samples, double budget = -1, const QString& note = {}) {
    Metric m;
    m.id = id;
    m.label = label;
    m.stats = perf::summarize(samples);
    m.budget = budget;
    m.note = note;
    metrics().push_back(m);
    qInfo().noquote() << QStringLiteral("%1  %2  n=%3  median %4 ms  p95 %5 ms  max %6 ms  budget %7  %8")
                             .arg(id, -10).arg(label, -60).arg(m.stats.n)
                             .arg(m.stats.median, 8, 'f', 3).arg(m.stats.p95, 8, 'f', 3).arg(m.stats.max, 8, 'f', 3)
                             .arg(budget < 0 ? QStringLiteral("—") : QString::number(budget))
                             .arg(budget < 0 ? QString() : (m.pass() ? QStringLiteral("PASS") : QStringLiteral("OVER")));
}

double msSince(const QElapsedTimer& t) { return static_cast<double>(t.nsecsElapsed()) / 1e6; }

class StubSource final : public ICompareSource {
public:
    std::optional<CompareRequest> request;
    std::optional<CompareRequest> compareRequest() const override { return request; }
};

class StubEntry final : public ICompareEntryView {
public:
    void setCompareAvailable(bool, const QString&) override {}
};

// 兩個假世界：同樣的格子；B 的弱格子（分數 0.40–0.65）和 A 的不同 → 有「只有 A」「只有 B」。
fake::FakeSpec world(std::uint64_t seed, int halfCols, int halfRows) {
    fake::FakeSpec s;
    s.halfCols = halfCols;
    s.halfRows = halfRows;
    s.weakFraction = 0.2;
    s.seed = seed;
    return s;
}

struct Bench {
    StubSource source;
    StubEntry entry;
    std::unique_ptr<CompareWindow> window;
    std::unique_ptr<ComparePresenter> presenter;
    dev::FakeBoard board;

    // halfCols／halfRows ＝ 0：排滿影像。
    Bench(int width, int height, QRectF sample, int halfCols, int halfRows) {
        board = dev::makeFakeBoard(width, height);
        CompareRequest r;
        r.image = board.view();
        r.sample = {sample.x(), sample.y(), sample.width(), sample.height()};
        r.threshold = 0.70;
        r.imageName = QStringLiteral("perf.png");
        source.request = r;
        window = std::make_unique<CompareWindow>(ThemeTokens::builtin());
        CompareServices services;
        services.backends = {BackendInfo{"a", "A (fake)", true, {}}, BackendInfo{"b", "B (fake)", true, {}}};
        services.create = [halfCols, halfRows](const std::string& id) -> Result<SearchEnginePtr> {
            auto engine = std::make_shared<SearchEngine>();
            engine->backend = fake::makeFakeBackend(world(id == "a" ? 3 : 4, halfCols, halfRows));
            engine->backend.id = id;
            engine->generate = std::make_unique<GenerateObjectsFromSample>(*engine->backend.templateBuilder, *engine->backend.searcher);
            return SearchEnginePtr(std::move(engine));
        };
        services.currentBackend = [] { return std::string("a"); };
        presenter = std::make_unique<ComparePresenter>(entry, *window, source, std::move(services));
        window->setController(presenter.get());
        window->resize(1600, 1000);
        window->show();
        Q_UNUSED(QTest::qWaitForWindowExposed(window.get()));
        QSignalSpy finished(presenter.get(), &ComparePresenter::compareFinished);
        presenter->openCompare();
        if (finished.isEmpty()) finished.wait(30000);
        QCoreApplication::processEvents();
    }

    // 把待重繪的區域畫完（同 perf_ui_s04 的 paintPending）。
    void paintPending() {
        QCoreApplication::sendPostedEvents(window->canvas()->canvasScene(), QEvent::MetaCall);
        QCoreApplication::sendPostedEvents(window.get(), QEvent::UpdateRequest);
        QCoreApplication::sendPostedEvents(window->canvas(), QEvent::UpdateRequest);
    }
};

}  // namespace

class PerfCompareUi : public QObject {
    Q_OBJECT

private:
    QJsonObject machine_;
    QString hygiene_;
    bool failedBudget_ = false;

    // 結果交進來（showResult：表格、清單、疊圖）→ 畫完。每次交一份新的疊圖（新的指標），和真的比較結束一樣。
    // phases（可為空）：交進來（表格、清單、疊圖的資料）與畫完各花多少。
    std::vector<double> resultToPaint(Bench& b, std::vector<double>* fill = nullptr, std::vector<double>* paint = nullptr) {
        std::vector<double> samples;
        const CompareResultView base = b.presenter->lastResult();
        for (int k = 0; k < kWarmup + kRuns; ++k) {
            CompareResultView r = base;
            r.overlay = std::make_shared<CompareOverlay>(*base.overlay);
            QElapsedTimer t;
            t.start();
            b.window->showResult(r);
            const double filled = msSince(t);
            b.paintPending();
            if (k >= kWarmup) {
                samples.push_back(msSince(t));
                if (fill) fill->push_back(filled);
                if (paint) paint->push_back(samples.back() - filled);
            }
        }
        return samples;
    }

private slots:
    void initTestCase() {
        Translations::install(QStringLiteral("zh_TW"));
        hygiene_ = applyMeasurementHygiene();
        machine_ = perf::machineFingerprint(QStringLiteral(AOI_SOURCE_DIR), QStringLiteral(AOI_BUILD_TYPE));
        machine_["measurementHygiene"] = hygiene_;
    }

    // AB-U1a：約 200 個標記（樣本左右各 9 格、上下各 5 格：19 × 11 ＝ 209 格）。
    void abResultToPaint200() {
        Bench b(1600, 1000, QRectF(780, 490, 40, 20), 9, 5);
        QVERIFY(b.presenter->comparison().has_value());
        const std::size_t marks = b.window->overlayItem()->overlay()->marks.size();
        std::vector<double> fill;
        std::vector<double> paint;
        const std::vector<double> s = resultToPaint(b, &fill, &paint);
        record(QStringLiteral("AB-U1a-fill"), QStringLiteral("  of which showResult (tables, list, overlay data)"), fill);
        record(QStringLiteral("AB-U1a-paint"), QStringLiteral("  of which painting"), paint);
        qInfo("differences listed: %d", b.window->differencesTable()->rowCount());
        record(QStringLiteral("AB-U1a"), QStringLiteral("A/B result ready to painted frame, %1 marks").arg(marks), s,
               perf::budgetOf(QStringLiteral("U1a"), QStringLiteral(AOI_SOURCE_DIR)), QStringLiteral("budget of U1a (200 boxes)"));
    }

    // AB-U1b：約 5 000 個標記（5 MP，樣本 24 × 16，格距 1.5 倍，排滿影像：72 × 81 ＝ 5 832 格，弱的不算）。
    void abResultToPaint5000() {
        Bench b(2592, 1944, QRectF(1284, 964, 24, 16), 0, 0);
        QVERIFY(b.presenter->comparison().has_value());
        const std::size_t marks = b.window->overlayItem()->overlay()->marks.size();
        QVERIFY(marks > 4000);
        std::vector<double> fill;
        std::vector<double> paint;
        const std::vector<double> s = resultToPaint(b, &fill, &paint);
        record(QStringLiteral("AB-U1b-fill"), QStringLiteral("  of which showResult (tables, list, overlay data)"), fill);
        record(QStringLiteral("AB-U1b-paint"), QStringLiteral("  of which painting"), paint);
        b.window->canvas()->viewport()->repaint();
        qInfo("differences listed: %d; overlay paint %.2f ms for %d marks, %d outlines", b.window->differencesTable()->rowCount(),
              b.window->overlayItem()->lastPaintStats().paintMs, b.window->overlayItem()->lastPaintStats().marksDrawn,
              b.window->overlayItem()->lastPaintStats().outlinesDrawn);
        record(QStringLiteral("AB-U1b"), QStringLiteral("A/B result ready to painted frame, %1 marks").arg(marks), s,
               perf::budgetOf(QStringLiteral("U1b"), QStringLiteral(AOI_SOURCE_DIR)), QStringLiteral("budget of U1b (5000 boxes)"));

        // AB-U3：改 B 的門檻 → 重新比較（不重新搜尋）→ 表格、清單、疊圖畫完。參考值（U3 的預算是主畫布的）。
        std::vector<double> u3;
        const double thresholds[] = {0.70, 0.75};
        for (int k = 0; k < kWarmup + kRuns; ++k) {
            QElapsedTimer t;
            t.start();
            b.presenter->setThresholds(0.70, thresholds[k % 2]);
            b.paintPending();
            if (k >= kWarmup) u3.push_back(msSince(t));
        }
        record(QStringLiteral("AB-U3"), QStringLiteral("A/B threshold change to painted frame (no new search), %1 items")
                                            .arg(b.presenter->comparison()->items.size()),
               u3, -1, QStringLiteral("reference only; U3 (16 ms) is a budget of the main canvas"));
    }

    // AB-CMP：只算 application 的 compareProposals（配對與分類，沒有 Qt），兩邊各 5 832 個候選（72 × 81 格，B 偏 0.3 px、
    // 每 7 個有一個低於門檻）。AB-U3 扣掉它，就是表格、清單與重畫的成本。
    void compareProposalsAlone() {
        SampleProposal a;
        SampleProposal b;
        a.partSize = b.partSize = {24.0, 16.0};
        for (int r = 0; r < 81; ++r) {
            for (int c = 0; c < 72; ++c) {
                const double x = 18.0 + 36.0 * c;
                const double y = 12.0 + 24.0 * r;
                const int k = r * 72 + c;
                a.candidates.push_back({{x, y, 0.0}, 0.95, CandidateSource::Seed});
                b.candidates.push_back({{x + 0.3, y - 0.2, 0.1}, k % 7 == 0 ? 0.6 : 0.93, CandidateSource::Seed});
            }
        }
        std::vector<double> samples;
        std::size_t both = 0;
        for (int k = 0; k < kWarmup + kRuns; ++k) {
            QElapsedTimer t;
            t.start();
            const BackendComparison c = compareProposals(a, 0.70, b, k % 2 == 0 ? 0.70 : 0.75);
            if (k >= kWarmup) samples.push_back(msSince(t));
            both = c.summary.both;
        }
        QVERIFY(both > 4000);
        record(QStringLiteral("AB-CMP"), QStringLiteral("compareProposals alone, 5832 vs 5832 candidates"), samples, -1,
               QStringLiteral("application only (no Qt): pairing on a uniform grid and the summary"));
    }

    void cleanupTestCase() {
        QJsonArray rows;
        for (const Metric& m : metrics()) rows.append(perf::toJson(m));
        QJsonObject root;
        root["schema"] = 1;
        root["step"] = "S09";
        root["method"] = "warm-up 5, measured 30; median and p95; time = showResult (or a threshold change) until the dirty region is painted";
        root["machine"] = machine_;
        root["metrics"] = rows;
        const QString outDir = qEnvironmentVariable("AOI_PERF_OUT", QDir::current().filePath(QStringLiteral("perf-results")));
        QDir().mkpath(outDir);
        QFile out(QDir(outDir).filePath(QStringLiteral("ui-compare-%1.json").arg(QGuiApplication::platformName())));
        if (out.open(QIODevice::WriteOnly)) out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        qInfo().noquote() << "wrote" << out.fileName();
#ifdef AOI_PERF_ENFORCE
        for (const Metric& m : metrics()) {
            if (m.budget > 0 && !m.pass()) {
                qWarning().noquote() << "OVER BUDGET:" << m.id << "p95" << m.stats.p95 << "ms, budget" << m.budget << "ms";
                failedBudget_ = true;
            }
        }
        QVERIFY2(!failedBudget_, "one or more budgets were exceeded (see the log)");
#endif
    }
};

QTEST_MAIN(PerfCompareUi)
#include "perf_compare_ui.moc"
