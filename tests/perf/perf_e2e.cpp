// S10 的效能量測（-Perf）：T5 端到端——放開滑鼠 → 提案就緒（5 MP、200 個元件；預算 perf/budgets.json 的 T5，300 ms）。
//
// 走產品的接線（app::composeCore → MainWindow → AppPresenters，真的 QThreadPool 與 JobRunner），back end 是組裝根註冊的真的
// back end（opencv、halcon；fake 也量一列，當成「UI ＋ harness 本身的成本」的參照）。影像是 perf_matrix 基準格的合成板子
// （5 MP、64×32 的樣本、200 個元件、雜訊 4、模糊 1）。
//   T5         放開滑鼠（QTest 送 MouseButtonRelease 給畫布）→ Presenter 發出 searchApplied（提案套到畫面上：候選、物件樹、訊息卡）
//   T5-paint   同上，再到畫布第一次把那一幀畫完（offscreen 平台：軟體算繪的 CPU 時間）
//   T5-phases  PerfProbe 的切片（uc.sample.*、job.*、ui.*）：T5 裡各階段的中位數，給 S11 找最大的一塊
// 每個 back end 一列（Qt Test 的資料列）；沒編進來、沒有授權的 SKIPPED 並說原因（不是通過）。HALCON 第一次建立約 2.8 秒
// （另一個行程的授權檢查，不算在 T5 裡）。方法：熱機 3 次、正式量 20 次（AOI_PERF_WARMUP、AOI_PERF_RUNS 可以改）。
// 預算只記錄 pass／fail，不讓測試失敗：閘門在 tools/perf-compare.mjs（它用多次執行的範圍，S04、S08 的提醒）。
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtTest>
#include <memory>
#include <vector>

#include "../ui/MeasurementHygiene.h"
#include "../ui/TestSupport.h"
#include "HarnessSupport.h"

#include "BackendTestSupport.h"
#include "app/CompositionRoot.h"
#include "application/backend/BackendRegistry.h"
#include "application/backend/SearchEngine.h"
#include "infrastructure/perf/PerfProbe.h"
#include "presentation/canvas/ImageCanvas.h"
#include "presentation/i18n/Translations.h"
#include "presentation/presenters/AppPresenters.h"
#include "presentation/shell/MainWindow.h"
#include "presentation/theme/ThemeManager.h"
#include "presentation/theme/ThemeTokens.h"
#include "synthetic_board.h"

using namespace aoi;
using namespace aoi::test;
using namespace aoi::test::harness;

namespace {

// perf_matrix 的基準格：5 MP、64×32（主體 56×24）、200 個元件、雜訊 4、模糊 1。
SyntheticBoardSpec boardSpec() {
    SyntheticBoardSpec s;
    s.width = 2592;
    s.height = 1944;
    s.bodyWidthPx = 56.0;
    s.bodyHeightPx = 24.0;
    s.originXPx = 120.0;
    s.originYPx = 110.0;
    s.pitchXPx = 173.0;  // 可用面積 2352×1724 ÷ 200 ≈ 20 270 px² 一個元件，長寬比 84：60
    s.pitchYPx = 117.0;
    s.cols = 14;
    s.rows = 15;
    while (s.rows * s.cols > 200 && s.rows > 1 && s.cols > 1) {
        if (s.cols >= s.rows * 84 / 60) --s.cols;
        else --s.rows;
    }
    s.noiseAmplitude = 4;
    s.blurRadius = 1;
    s.seed = 17;
    return s;
}

// 和產品一樣的接線（main.cpp）；back end 由 id 指定。成員的順序 ＝ 建立順序，解構反過來：Presenter → 視窗 → 核心。
struct E2eRig {
    ui::ThemeManager theme{*qApp, ui::ThemeTokens::builtin()};
    const BackendRegistry& registry;
    Result<app::CoreServices> core;
    ui::MainWindow window;
    std::unique_ptr<ui::AppPresenters> presenters;

    E2eRig(const BackendRegistry& reg, const std::string& backendId)
        : registry(reg), core(app::composeCore(reg, backendId, true, app::ExecutorKind::QtThreadPool)), window(theme) {
        if (!core) return;
        app::CoreServices& services = core.value();
        ui::PresenterServices presenterServices{useCaseOf(services.engine), *services.runner, services.probe.get()};
        ui::BackendServices backendServices;
        backendServices.backends = registry.list();
        backendServices.current = services.engine;
        backendServices.create = [this, probe = services.probe.get()](const std::string& id) { return makeSearchEngine(registry, id, probe); };
        backendServices.executor = services.executor.get();
        presenters = std::make_unique<ui::AppPresenters>(window, presenterServices, std::move(backendServices), ui::ThemeTokens::builtin());
        window.resize(1280, 800);
        window.show();
        Q_UNUSED(QTest::qWaitForWindowExposed(&window));
    }
    ~E2eRig() {
        if (presenters) presenters->autoRoi().cancelSearch();
    }
    ui::AutoRoiPresenter& presenter() { return presenters->autoRoi(); }
    QWidget* vp() { return window.canvas()->viewport(); }
    QPointF at(double x, double y) { return toView(*window.canvas(), QPointF(x, y)); }
};

// 把待重繪的區域畫完（同 perf_ui_s04 的 paintPending）。
void paintPending(E2eRig& r) {
    QCoreApplication::sendPostedEvents(r.window.canvas()->canvasScene(), QEvent::MetaCall);
    QCoreApplication::sendPostedEvents(&r.window, QEvent::UpdateRequest);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
}

double msSince(const QElapsedTimer& t) { return static_cast<double>(t.nsecsElapsed()) / 1e6; }

}  // namespace

class PerfE2e : public QObject {
    Q_OBJECT

private:
    QString hygiene_;
    RunConfig config_;
    QJsonObject environmentStart_;
    BackendRegistry registry_;
    std::vector<Metric> metrics_;
    QJsonObject phases_;
    std::unique_ptr<SyntheticBoard> board_;

    void add(Metric m) {
        qInfo().noquote() << QStringLiteral("%1  n=%2  median %3  p95 %4  max %5 ms  budget %6  %7")
                                 .arg(m.id, -18).arg(m.stats.n, 3).arg(m.stats.median, 9, 'f', 3).arg(m.stats.p95, 9, 'f', 3).arg(m.stats.max, 9, 'f', 3)
                                 .arg(m.budget < 0 ? QStringLiteral("—") : QString::number(m.budget))
                                 .arg(m.budget < 0 ? QString() : (m.pass() ? QStringLiteral("PASS") : QStringLiteral("OVER")));
        metrics_.push_back(std::move(m));
    }

private slots:
    void initTestCase() {
        ui::Translations::install(QStringLiteral("zh_TW"));
        hygiene_ = applyMeasurementHygiene();
        config_ = RunConfig::fromEnvironment(3, 20);
        environmentStart_ = environmentSnapshot();
        registry_ = app::makeBackendRegistry();
        board_ = std::make_unique<SyntheticBoard>(makeSyntheticBoard(boardSpec()));
        QVERIFY(board_->image.valid());
        qInfo().noquote() << "measurement hygiene:" << hygiene_;
        qInfo().noquote() << "board: 2592x1944, parts" << board_->presentCount() << " warmup/runs:" << config_.warmup << "/" << config_.runs
                          << " injected delay ms:" << config_.injectedDelayMs;
    }

    void t5_data() {
        QTest::addColumn<QString>("backend");
        for (const BackendInfo& info : registry_.list()) QTest::newRow(info.id.c_str()) << QString::fromStdString(info.id);
    }

    void t5() {
        QFETCH(QString, backend);
        const std::string id = backend.toStdString();
        for (const BackendInfo& info : registry_.list())
            if (info.id == id && !info.available) QSKIP(qPrintable(QStringLiteral("%1 is not usable in this build: %2").arg(backend, QString::fromStdString(info.unavailable.message))));
        E2eRig r(registry_, id);
        if (!r.core) {
            const Error& e = r.core.error();
            if (e.code == ErrorCode::BackendUnavailable) QSKIP(qPrintable(QStringLiteral("%1 is not usable here: %2").arg(backend, QString::fromStdString(e.message))));
            QFAIL(qPrintable(QStringLiteral("%1 could not be created: %2").arg(backend, QString::fromStdString(e.message))));
        }
        r.window.setImage(board_->image, QStringLiteral("synthetic-5mp-200.png"));
        QCoreApplication::processEvents();
        paintPending(r);

        const Rect sample = sampleBoxAround(*board_, middlePresentIndex(*board_));
        const QRectF s(sample.x, sample.y, sample.w, sample.h);
        infra::PerfProbe* probe = r.core.value().probe.get();

        std::vector<double> toApplied, toPainted;
        int candidates = 0;
        for (int i = 0; i < config_.warmup + config_.runs; ++i) {
            if (i == config_.warmup && probe) {
                probe->setEnabled(true);
                probe->clear();
            }
            // 每次從 Idle 開始：捨棄上一份提案。
            if (r.presenter().state() == InteractionState::Preview) sendKey(r.window.canvas(), Qt::Key_Escape);
            QVERIFY2(QTest::qWaitFor([&] { return r.presenter().state() == InteractionState::Idle; }, 5000), "the UI did not return to Idle");
            QCoreApplication::processEvents();
            paintPending(r);

            r.window.toolAction(ui::ToolId::Sample)->trigger();
            press(r.vp(), r.at(s.left(), s.top()));
            move(r.vp(), r.at(s.center().x(), s.center().y()));
            move(r.vp(), r.at(s.right(), s.bottom()));

            QEventLoop loop;
            QTimer overall;
            overall.setSingleShot(true);
            QObject::connect(&overall, &QTimer::timeout, &loop, &QEventLoop::quit);
            bool applied = false;
            double appliedMs = 0.0;
            QElapsedTimer t;
            const QMetaObject::Connection c = QObject::connect(&r.presenter(), &ui::AutoRoiPresenter::searchApplied, &loop, [&] {
                appliedMs = msSince(t);
                applied = true;
                loop.quit();
            });
            overall.start(30000);
            const int paintsBefore = r.window.canvas()->paintCount();
            t.start();
            release(r.vp(), r.at(s.right(), s.bottom()));  // 放開滑鼠：onRegionDrawn → startSearch（worker）→ 結果回到 UI 執行緒 → 套用
            if (!applied) loop.exec();
            QObject::disconnect(c);
            QVERIFY2(applied, "the search did not finish within 30 s");
            busyWait(config_.injectedDelayMs);
            appliedMs += config_.injectedDelayMs;
            // 再到畫布把這一幀畫完。
            paintPending(r);
            while (r.window.canvas()->paintCount() == paintsBefore && msSince(t) < 5000.0) {
                QCoreApplication::processEvents();
                paintPending(r);
            }
            const double paintedMs = msSince(t);
            QVERIFY2(r.window.canvas()->paintCount() > paintsBefore, "the canvas did not repaint after the result");
            {
                const InteractionState st = r.presenter().state();
                QVERIFY2(st == InteractionState::Preview, qPrintable(QStringLiteral("state after the search is %1, not Preview").arg(static_cast<int>(st))));
            }
            candidates = static_cast<int>(r.presenter().lastScene().snapshot->components.size());
            if (i >= config_.warmup) {
                toApplied.push_back(appliedMs);
                toPainted.push_back(paintedMs);
            }
        }
        if (probe) probe->setEnabled(false);

        Metric m;
        m.id = QStringLiteral("T5:") + backend;
        m.label = QStringLiteral("mouse release to proposals ready (searchApplied), 5 MP, %1 parts, %2 candidates on screen, %3")
                      .arg(board_->presentCount()).arg(candidates).arg(backend);
        m.stats = summarize(toApplied);
        if (id != "fake") {
            m.budgetId = QStringLiteral("T5");
            m.budget = budgetOf(m.budgetId);
        } else {
            m.note = QStringLiteral("the fake back end has no search cost: this is the UI and harness part of T5");
        }
        add(m);
        Metric p;
        p.id = QStringLiteral("T5-paint:") + backend;
        p.label = QStringLiteral("mouse release to the first painted frame with the proposals, %1").arg(backend);
        p.stats = summarize(toPainted);
        p.note = QStringLiteral("diagnostic (offscreen software raster)");
        add(p);

        if (probe) {
            const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(probe->toJson(false)));
            qInfo().noquote() << "phase                          count   median ms   p95 ms";
            QJsonArray phases;
            for (const QJsonValue& v : doc.object().value("summary").toArray()) {
                const QJsonObject o = v.toObject();
                phases.append(o);
                qInfo().noquote() << QStringLiteral("%1 %2 %3 %4").arg(o.value("name").toString(), -30).arg(o.value("count").toInt(), 6)
                                         .arg(o.value("medianMs").toDouble(), 10, 'f', 3).arg(o.value("p95Ms").toDouble(), 8, 'f', 3);
            }
            phases_[backend] = phases;
        }
    }

    void cleanupTestCase() {
        if (metrics_.empty()) return;
        QJsonArray rows;
        for (const Metric& m : metrics_) rows.append(toJson(m));
        QJsonObject root;
        root["schema"] = 1;
        root["kind"] = QStringLiteral("perf_e2e");
        root["step"] = QStringLiteral("S10");
        root["machine"] = machineFingerprint(hygiene_, QStringLiteral(AOI_BUILD_TYPE));
        root["environment"] = QJsonObject{{"start", environmentStart_}, {"end", environmentSnapshot()}};
        root["warmup"] = config_.warmup;
        root["runs"] = config_.runs;
        root["injectedDelayMs"] = config_.injectedDelayMs;
        root["method"] = QStringLiteral("product wiring (composeCore, MainWindow, AppPresenters); QTest mouse events on the canvas; "
                                        "T5 = release until AutoRoiPresenter::searchApplied; warmup %1, runs %2; one row per registered back end")
                             .arg(config_.warmup).arg(config_.runs);
        root["metrics"] = rows;
        root["phases"] = phases_;
        qInfo("results: %s", qPrintable(writeResult(QStringLiteral("e2e"), root)));
    }
};

QTEST_MAIN(PerfE2e)
#include "perf_e2e.moc"
