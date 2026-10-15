// S04 的 UI 效能量測：U2、U3、I3、I4、I6（預算在 perf/budgets.json；定義在 docs/ux/ux-spec.md 第 13 節）。
// 方法同 S02：丟掉前 5 次熱機，正式量 30 次，回報中位數與 p95；結果記錄機器指紋與量測衛生（釘核心、關電源節流）。
//
// 量測的是 Qt 軟體算繪的 CPU 時間（offscreen 平台）：一個操作 → 把待重繪的區域畫完。不含顯示器的垂直同步與合成器。
// 全部走產品的接線（core → MainWindow → AppPresenters，真的 QThreadPool），不是只量其中一個類別。
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QTreeView>
#include <QtTest>
#include <chrono>

#include "MeasurementHygiene.h"
#include "PerfSupport.h"
#include "PresenterRig.h"
#include "infrastructure/perf/PerfProbe.h"
#include "ProposalTestSupport.h"
#include "QtTestPrinters.h"

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

void record(const QString& id, const QString& label, const std::vector<double>& samples, double budget = -1,
            const QString& note = QString()) {
    Metric m;
    m.id = id;
    m.label = label;
    m.stats = perf::summarize(samples);
    m.budget = budget;
    m.note = note;
    metrics().push_back(m);
    qInfo().noquote() << QStringLiteral("%1  %2  n=%3  median %4 ms  p95 %5 ms  max %6 ms  budget %7  %8")
                             .arg(id, -8).arg(label, -58).arg(m.stats.n)
                             .arg(m.stats.median, 8, 'f', 3).arg(m.stats.p95, 8, 'f', 3).arg(m.stats.max, 8, 'f', 3)
                             .arg(budget < 0 ? QStringLiteral("—") : QString::number(budget))
                             .arg(budget < 0 ? QString() : (m.pass() ? QStringLiteral("PASS") : QStringLiteral("OVER")));
}

// 把待重繪的區域畫完（和使用者「看到這一幀」同義）：先處理 scene 排入佇列的更新通知，再處理視窗的 UpdateRequest。
void paintPending(Rig& r) {
    QCoreApplication::sendPostedEvents(r.canvas()->canvasScene(), QEvent::MetaCall);
    QCoreApplication::sendPostedEvents(&r.window, QEvent::UpdateRequest);
    QCoreApplication::sendPostedEvents(r.canvas(), QEvent::UpdateRequest);
}

double msSince(const QElapsedTimer& t) { return static_cast<double>(t.nsecsElapsed()) / 1e6; }

// 5 MP 板子上約 5 800 格：樣本 24 × 16，格距 1.5 倍。
fake::FakeSpec bigWorld() {
    fake::FakeSpec spec;
    spec.weakFraction = 0.2;
    spec.missingFraction = 0.05;
    spec.seed = 7;
    return spec;
}

constexpr int kBoardW = 2592;
constexpr int kBoardH = 1944;
const QRectF kSample5000(1200, 900, 24, 16);

// 已記載、超出預算的項目：照樣量測、記錄、超標時在 log 裡警告，但不當成測試失敗（和 S02 的 I1 同一個做法）。
// 原因與選項見 docs/steps/S04-presenter/perf.md；等 S11 優化後，從這裡移除。
//   U3  5 832 個元件的門檻拖動：S04 p95 約 46 ms（預算 16 ms）；S11 之後 p95 約 8–11 ms（字形快取、延後格式化、文件快取）。
// S11 之前 U3 是已知的超標（knownOver 讓它不擋住其他退步的偵測）；S11 把它做進預算，現在沒有已知超標的項目。
bool knownOver(const QString&) { return false; }

}  // namespace

class PerfUiS04 : public QObject {
    Q_OBJECT
    QJsonObject machine_;
    QString hygiene_;
    QJsonArray rejectedTable_;
    QJsonArray scalingTable_;
    QJsonArray phaseTable_;

private slots:
    void initTestCase() {
        Translations::install(QStringLiteral("zh_TW"));
        hygiene_ = applyMeasurementHygiene();
        machine_ = perf::machineFingerprint(QStringLiteral(AOI_SOURCE_DIR), QStringLiteral(AOI_BUILD_TYPE));
        machine_["hygiene"] = hygiene_;
        qInfo().noquote() << "measurement hygiene:" << hygiene_;
    }

    // ---- U2：物件表填 5 000 列 ----
    // 從「沒有資料」到「5 000 列畫出來」：model 換資料＋樹的版面＋第一次繪製（只畫看得到的幾十列）。
    void u2_fillObjectTable() {
        InspectionSession session;
        session.imageOpened("board.png", kBoardW, kBoardH, 0.0);
        // ADR-025：S 放開只建立樣本（回到 Idle），搜尋另外按；提案的類型 t1 就是這個樣本的類型。
        QVERIFY(session.beginRegion(RegionMode::Sample));
        QVERIFY(session.endRegion(Rect{0, 0, 60, 40}, RegionMode::Sample) == InspectionSession::Release::SampleAdded);
        QVERIFY(session.startSearch());
        QVERIFY(session.searchSucceeded(makeBigProposal(5000)));
        SceneLabels labels;
        labels.sample = QStringLiteral("樣本");
        labels.inferred = QStringLiteral("推算");
        labels.manual = QStringLiteral("手動");
        labels.board = QStringLiteral("板子 board.png");
        const Scene scene = buildScene(session, labels);
        QCOMPARE(scene.rows->rows.size(), std::size_t{5001});

        ObjectsPanel panel;
        panel.resize(300, 420);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        const ObjectRowsPtr empty = std::make_shared<ObjectRows>();

        std::vector<double> samples;
        for (int i = 0; i < kWarmup + kRuns; ++i) {
            panel.setRows(empty);
            QCoreApplication::processEvents();
            QElapsedTimer t;
            t.start();
            panel.setRows(scene.rows);
            QCoreApplication::sendPostedEvents(&panel, QEvent::LayoutRequest);
            panel.view()->viewport()->repaint();  // 同步畫出看得到的列
            const double ms = msSince(t);
            if (i >= kWarmup) samples.push_back(ms);
        }
        QCOMPARE(panel.model()->visibleCount(), 5001);
        record("U2", "fill object table, 5000 rows (model + layout + first paint)", samples,
               perf::budgetOf("U2", QStringLiteral(AOI_SOURCE_DIR)), "5 001 rows = 5 000 candidates + the sample");

        // 診斷：只換資料、不畫。
        std::vector<double> modelOnly;
        for (int i = 0; i < kWarmup + kRuns; ++i) {
            panel.setRows(empty);
            QElapsedTimer t;
            t.start();
            panel.model()->setRows(scene.rows);
            if (i >= kWarmup) modelOnly.push_back(msSince(t));
        }
        record("U2-model", "setRows only (no layout, no paint)", modelOnly, -1, "diagnostic");
    }

    // ---- U3：分數門檻滑桿 → 重繪 ----
    void u3_thresholdToRepaint() {
        Rig r(bigWorld(), false);
        r.window.resize(1280, 800);
        r.open(kBoardW, kBoardH);
        QVERIFY(r.teachSample(kSample5000));
        QCOMPARE_STATE(r.state(), InteractionState::Preview);
        const int components = r.componentCount();
        QVERIFY2(components > 4500, qPrintable(QString::number(components)));
        QCoreApplication::processEvents();
        paintPending(r);

        const int values[] = {50, 70, 92, 97, 60, 85};
        std::vector<double> samples, sceneOnly;
        QSlider* slider = r.window.paramsPanel()->scoreSlider();
        int stepIndex = 0;
        for (int i = 0; i < kWarmup + kRuns; ++i) {
            const int v = values[stepIndex++ % 6];
            const int paintsBefore = r.canvas()->paintCount();
            QElapsedTimer t;
            t.start();
            slider->setValue(v);       // → 門檻 → 重新過濾 → 場景 → 畫布、物件樹、訊息卡、狀態列
            paintPending(r);
            const double ms = msSince(t);
            QVERIFY2(r.canvas()->paintCount() > paintsBefore, "the canvas did not repaint: the measurement is invalid");
            if (i >= kWarmup) samples.push_back(ms);
        }
        record("U3", QStringLiteral("threshold slider change to repaint, %1 components").arg(components), samples,
               perf::budgetOf("U3", QStringLiteral(AOI_SOURCE_DIR)),
               "whole pipeline: classify, scene build, canvas, tree (dataChanged), card, status; then the dirty window region is painted");

        // 診斷：同一個工作階段重建場景要多久（不含 View 與繪製）。
        const InspectionSession& session = r.presenter().session();
        SceneLabels labels;
        labels.sample = QStringLiteral("樣本");
        labels.inferred = QStringLiteral("推算");
        labels.manual = QStringLiteral("手動");
        labels.board = QStringLiteral("板子");
        for (int i = 0; i < kWarmup + kRuns; ++i) {
            QElapsedTimer t;
            t.start();
            const Scene scene = buildScene(session, labels);
            const double ms = msSince(t);
            QVERIFY(scene.snapshot->components.size() > 4500);
            if (i >= kWarmup) sceneOnly.push_back(ms);
        }
        record("U3-scene", "diagnostic: buildScene alone (classify + edits + boxes + rows)", sceneOnly, -1, "diagnostic");

        // 診斷：板子上已經有 5 000 個元件、再預覽第二份提案（背景的文件元件每次也要重建）。
        r.key(Qt::Key_Return);
        QCOMPARE_STATE(r.state(), InteractionState::Idle);
        QVERIFY(r.teachSample(QRectF(300, 300, 24, 16)));
        std::vector<double> withBackground;
        stepIndex = 0;
        for (int i = 0; i < kWarmup + kRuns; ++i) {
            const int v = values[stepIndex++ % 6];
            QElapsedTimer t;
            t.start();
            slider->setValue(v);
            paintPending(r);
            const double ms = msSince(t);
            if (i >= kWarmup) withBackground.push_back(ms);
        }
        record("U3-bg", QStringLiteral("diagnostic: same, with %1 accepted components in the background").arg(r.document().components().size()),
               withBackground, -1, "diagnostic: the document part of the scene is rebuilt on every change");
    }

    // U3 的拆解：同一份 5 000 個候選的提案，把管線拆開，各步驟單獨量（診斷，沒有預算）。
    void u3_parts_data() {
        QTest::addColumn<int>("w");
        QTest::addColumn<int>("h");
        QTest::newRow("5832 objects") << 24 << 16;
        QTest::newRow("504 objects") << 82 << 54;
    }
    void u3_parts() {
        QFETCH(int, w);
        QFETCH(int, h);
        Rig r(bigWorld(), false);
        r.window.resize(1280, 800);
        r.open(kBoardW, kBoardH);
        QVERIFY(r.teachSample(QRectF(1200, 900, w, h)));
        paintPending(r);
        const QString suffix = QStringLiteral("@%1").arg(r.componentCount());
        InspectionSession& session = const_cast<InspectionSession&>(r.presenter().session());
        SceneLabels labels;
        labels.sample = QStringLiteral("樣本");
        labels.inferred = QStringLiteral("推算");
        labels.manual = QStringLiteral("手動");
        labels.board = QStringLiteral("板子");

        const double values[] = {0.50, 0.92, 0.70, 0.97};
        std::vector<double> canvasMs, canvasPaintMs, imagePaintMs, layerPaintMs, treeMs, treePaintMs, cardMs, restMs;
        for (int i = 0; i < kWarmup + kRuns; ++i) {
            session.setThreshold(values[i % 4]);
            const Scene scene = buildScene(session, labels);

            QElapsedTimer t;
            t.start();
            r.canvas()->showComponents(scene.snapshot);
            const double c = msSince(t);
            t.start();
            paintPending(r);
            const double cp = msSince(t);
            // S11：這一幀裡影像與框層各花多久（兩個 Item 自己量的；差額是 View 與背景）。
            const double ip = r.canvas()->imageItem()->lastPaintMs();
            const double lp = r.canvas()->layerItem()->lastPaintStats().paintMs;

            t.start();
            r.window.objectsPanel()->setRows(scene.rows);
            const double tr = msSince(t);
            t.start();
            paintPending(r);
            const double trp = msSince(t);

            t.start();
            const CardSpec card = previewCard(scene.counts);
            r.window.showCard(card.card);
            r.window.showSummary(QStringLiteral("x %1").arg(i));
            const double cd = msSince(t);
            t.start();
            paintPending(r);
            const double rest = msSince(t);
            if (i >= kWarmup) {
                canvasMs.push_back(c);
                canvasPaintMs.push_back(cp);
                imagePaintMs.push_back(ip);
                layerPaintMs.push_back(lp);
                treeMs.push_back(tr);
                treePaintMs.push_back(trp);
                cardMs.push_back(cd);
                restMs.push_back(rest);
            }
        }
        record("U3-canvas" + suffix, "diagnostic: canvas showComponents (rebuild the layer buckets)", canvasMs, -1, "diagnostic");
        record("U3-canvas-paint" + suffix, "diagnostic: paint after the canvas update", canvasPaintMs, -1, "diagnostic");
        record("U3-canvas-paint-image" + suffix, "diagnostic: of which ImageItem::paint (the board image)", imagePaintMs, -1, "diagnostic");
        record("U3-canvas-paint-layer" + suffix, "diagnostic: of which RoiLayerItem::paint (the boxes)", layerPaintMs, -1, "diagnostic");
        record("U3-tree" + suffix, "diagnostic: object tree setRows (dataChanged path)", treeMs, -1, "diagnostic");
        record("U3-tree-paint" + suffix, "diagnostic: paint after the tree update", treePaintMs, -1, "diagnostic");
        record("U3-card" + suffix, "diagnostic: message card + status text", cardMs, -1, "diagnostic");
        record("U3-card-paint" + suffix, "diagnostic: paint after the card update", restMs, -1, "diagnostic");
    }

    // U3 重繪成本隨「被拒絕（點線）的框的比例」變化：實線框走快速的整數像素填色矩形，點線框走 cosmetic 描邊。
    void u3_repaintByRejectedFraction() {
        Rig r(bigWorld(), false);
        r.window.resize(1280, 800);
        r.open(kBoardW, kBoardH);
        QVERIFY(r.teachSample(kSample5000));
        QSlider* slider = r.window.paramsPanel()->scoreSlider();
        QJsonArray table;
        qInfo().noquote() << "threshold  rejected%  inferred%  frame ms (median of 10, update + paint)";
        for (int v : {30, 50, 60, 65, 70, 75, 80, 85, 90, 95, 99}) {
            slider->setValue(v);
            paintPending(r);
            std::vector<double> frames;
            for (int i = 0; i < 13; ++i) {
                // 把畫面弄髒（不改內容）再畫：量的是「這個狀態組合下，一個完整畫格要多久」。
                r.canvas()->viewport()->update();
                QElapsedTimer t;
                t.start();
                paintPending(r);
                if (i >= 3) frames.push_back(msSince(t));
            }
            const SceneCounts& c = r.scene().counts;
            const double total = static_cast<double>(c.components);
            QJsonObject o;
            o["threshold"] = v / 100.0;
            o["rejectedPercent"] = std::round(100.0 * static_cast<double>(c.rejected) / total * 10) / 10;
            o["inferredPercent"] = std::round(100.0 * static_cast<double>(c.inferred) / total * 10) / 10;
            o["frameMsMedian"] = perf::round3(percentile(frames, 0.5));
            table.append(o);
            qInfo().noquote() << QStringLiteral("%1      %2     %3     %4")
                                     .arg(v / 100.0, 4, 'f', 2).arg(o["rejectedPercent"].toDouble(), 6, 'f', 1)
                                     .arg(o["inferredPercent"].toDouble(), 6, 'f', 1).arg(percentile(frames, 0.5), 8, 'f', 2);
        }
        rejectedTable_ = table;
    }

    // U3 的各階段（Presenter 內的 PerfProbe 計時）：不含最後的繪製，那一段在 u3_parts。
    void u3_phases() {
        Rig r(bigWorld(), false);
        r.window.resize(1280, 800);
        r.open(kBoardW, kBoardH);
        QVERIFY(r.teachSample(kSample5000));
        paintPending(r);
        infra::PerfProbe* probe = r.core.value().probe.get();
        probe->setEnabled(true);
        probe->clear();
        QSlider* slider = r.window.paramsPanel()->scoreSlider();
        const int values[] = {50, 70, 92, 97, 60, 85};
        for (int i = 0; i < 40; ++i) {
            slider->setValue(values[i % 6]);
            paintPending(r);
        }
        probe->setEnabled(false);
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(probe->toJson(false)));
        qInfo().noquote() << "phase                          count   median ms   p95 ms";
        for (const QJsonValue& v : doc.object().value("summary").toArray()) {
            const QJsonObject o = v.toObject();
            const QString name = o.value("name").toString();
            if (!name.startsWith(QLatin1String("ui."))) continue;
            qInfo().noquote() << QStringLiteral("%1 %2 %3 %4").arg(name, -30).arg(o.value("count").toInt(), 6)
                                     .arg(o.value("medianMs").toDouble(), 10, 'f', 3).arg(o.value("p95Ms").toDouble(), 8, 'f', 3);
        }
        phaseTable_ = doc.object().value("summary").toArray();
    }

    // U3 與 I3 隨物件個數的變化：預算在哪個規模以內成立。樣本越大，5 MP 板子上排得下的格子越少。
    void scalingWithObjectCount() {
        struct Case {
            int w, h;
        };
        // 格距 1.5 倍：格數 ≈ (2592 × 1944) ÷ (2.25 × w × h)
        const Case cases[] = {{82, 54}, {58, 38}, {41, 27}, {24, 16}};
        QJsonArray table;
        qInfo().noquote() << "objects   U3 median / p95 (ms)    I3 max block (ms)";
        for (const Case& c : cases) {
            Rig r(bigWorld(), false);
            r.window.resize(1280, 800);
            r.open(kBoardW, kBoardH);
            const QRectF sample(1200, 900, c.w, c.h);
            // I3：這個規模的一次搜尋
            QTimer heartbeat;
            heartbeat.setTimerType(Qt::PreciseTimer);
            heartbeat.setInterval(1);
            std::chrono::steady_clock::time_point last;
            double maxGap = 0;
            QObject::connect(&heartbeat, &QTimer::timeout, &heartbeat, [&] {
                const auto now = std::chrono::steady_clock::now();
                maxGap = std::max(maxGap, std::chrono::duration<double, std::milli>(now - last).count());
                last = now;
            });
            QVERIFY(r.drawSample(sample));  // ADR-025：放開只建立樣本；搜尋按 F5
            QEventLoop loop;
            QObject::connect(&r.presenter(), &AutoRoiPresenter::searchApplied, &loop, [&] { QTimer::singleShot(120, &loop, &QEventLoop::quit); });
            QTimer::singleShot(20000, &loop, &QEventLoop::quit);
            heartbeat.start();
            last = std::chrono::steady_clock::now();
            r.window.searchAction()->trigger();  // F5：同步執行 search → startSearch → 發布 Searching 的畫面
            loop.exec();
            heartbeat.stop();
            QVERIFY(r.state() == InteractionState::Preview);
            const int components = r.componentCount();

            // U3：滑桿來回拖
            QSlider* slider = r.window.paramsPanel()->scoreSlider();
            const int values[] = {50, 70, 92, 97, 60, 85};
            std::vector<double> samples;
            for (int i = 0; i < 4 + 20; ++i) {
                QElapsedTimer t;
                t.start();
                slider->setValue(values[i % 6]);
                paintPending(r);
                if (i >= 4) samples.push_back(msSince(t));
            }
            QJsonObject o;
            o["objects"] = components;
            o["u3Median"] = perf::round3(percentile(samples, 0.5));
            o["u3P95"] = perf::round3(percentile(samples, 0.95));
            o["i3MaxBlock"] = perf::round3(maxGap);
            table.append(o);
            qInfo().noquote() << QStringLiteral("%1   %2 / %3          %4").arg(components, 6).arg(percentile(samples, 0.5), 8, 'f', 2)
                                     .arg(percentile(samples, 0.95), 6, 'f', 2).arg(maxGap, 8, 'f', 2);
        }
        scalingTable_ = table;
    }

    // ---- I3：搜尋期間事件迴圈最長的一次阻塞 ----
    void i3_longestEventLoopBlockDuringASearch() {
        Rig r(bigWorld(), false);
        r.window.resize(1280, 800);
        r.open(kBoardW, kBoardH);
        QVERIFY(r.drawSample(kSample5000));  // ADR-025：樣本建一次，每回合按 F5 搜尋
        std::vector<double> perSearchMax, handlerMs;
        double overallMax = 0;
        const int searches = 20;
        for (int i = 0; i < searches + 3; ++i) {
            // 每次從 Idle 開始：捨棄上一份提案。
            if (r.state() == InteractionState::Preview) r.key(Qt::Key_Escape);
            QCOMPARE_STATE(r.state(), InteractionState::Idle);
            QCoreApplication::processEvents();
            paintPending(r);


            // 心跳：1 ms 計時器。兩次心跳之間的最大間隔 ＝ 事件迴圈被擋住的最長時間。
            QTimer heartbeat;
            heartbeat.setTimerType(Qt::PreciseTimer);
            heartbeat.setInterval(1);
            std::chrono::steady_clock::time_point last;
            double maxGap = 0;
            QObject::connect(&heartbeat, &QTimer::timeout, &heartbeat, [&] {
                const auto now = std::chrono::steady_clock::now();
                maxGap = std::max(maxGap, std::chrono::duration<double, std::milli>(now - last).count());
                last = now;
            });
            QEventLoop loop;
            QTimer overall;
            overall.setSingleShot(true);
            QObject::connect(&overall, &QTimer::timeout, &loop, &QEventLoop::quit);
            bool applied = false;
            QObject::connect(&r.presenter(), &AutoRoiPresenter::searchApplied, &loop, [&] {
                applied = true;
                // 結果套用之後，再多跑一小段事件迴圈，把「第一幀畫出來」也算進去。
                QTimer::singleShot(120, &loop, &QEventLoop::quit);
            });
            heartbeat.start();
            last = std::chrono::steady_clock::now();
            overall.start(20000);

            QElapsedTimer release_;
            release_.start();
            r.window.searchAction()->trigger();  // F5：同步執行 search → startSearch → 發布 Searching 的畫面
            const double handler = msSince(release_);
            loop.exec();
            heartbeat.stop();
            QObject::disconnect(&r.presenter(), &AutoRoiPresenter::searchApplied, &loop, nullptr);
            QVERIFY2(applied, "the search did not finish");
            QCOMPARE_STATE(r.state(), InteractionState::Preview);

            if (i >= 3) {
                const double blocked = std::max(maxGap, handler);
                perSearchMax.push_back(blocked);
                handlerMs.push_back(handler);
                overallMax = std::max(overallMax, blocked);
            }
        }
        const int components = r.componentCount();
        record("I3", QStringLiteral("longest event loop block during a search, %1 candidates (max per search)").arg(components),
               perSearchMax, perf::budgetOf("I3", QStringLiteral(AOI_SOURCE_DIR)),
               "1 ms precise heartbeat on the UI thread from before the release until 120 ms after the result is applied; also counts the release handler itself");
        record("I3-release", "diagnostic: the mouse release handler alone (start the search, publish)", handlerMs, -1, "diagnostic");
        qInfo().noquote() << QStringLiteral("I3 overall maximum over %1 searches: %2 ms").arg(searches).arg(overallMax, 0, 'f', 2);
    }

    // ---- I4：取消 → UI 回到 Idle ----
    void i4_cancelToIdle() {
        auto world = bigWorld();
        world.latencyMs = 5000;  // 搜尋一直在跑，取消的時候 worker 還在忙
        for (const char* path : {"click", "esc"}) {
            Rig r(world, true);
            r.window.resize(1280, 800);
            r.open(kBoardW, kBoardH);
            std::vector<double> samples;
            for (int i = 0; i < kWarmup + kRuns; ++i) {
                if (i == 0) QVERIFY(r.drawSample(kSample5000));  // ADR-025：樣本建一次，每回合按 F5
                r.window.searchAction()->trigger();
                QVERIFY(QTest::qWaitFor([&] { return r.presenter().busyLevel() == BusyLevel::Progress; }, 5000));
                QPushButton* cancel = r.window.messageBar()->findChild<QPushButton*>(QStringLiteral("messageCancel"));
                QVERIFY(cancel != nullptr);
                QElapsedTimer t;
                t.start();
                if (QLatin1String(path) == QLatin1String("click")) cancel->click();
                else r.key(Qt::Key_Escape);
                const bool idle = r.state() == InteractionState::Idle;
                paintPending(r);
                const double ms = msSince(t);
                QVERIFY2(idle, "the UI is not idle after the cancel");
                QVERIFY(!r.window.messageBar()->progressVisible());
                if (i >= kWarmup) samples.push_back(ms);
                // worker 還沒停：等忙碌標示收掉，下一輪再開始（舊搜尋的結果由世代丟掉）。
                QVERIFY(QTest::qWaitFor([&] { return r.presenter().busyLevel() == BusyLevel::None; }, 3000));
            }
            record(QLatin1String(path) == QLatin1String("click") ? "I4" : "I4-esc",
                   QStringLiteral("cancel (%1) to UI idle and repainted, search still running on the worker").arg(QLatin1String(path)),
                   samples, QLatin1String(path) == QLatin1String("click") ? perf::budgetOf("I4", QStringLiteral(AOI_SOURCE_DIR)) : -1,
                   "does not wait for the worker to stop (budget definition); the late result is dropped by generation");
        }
    }

    // ---- I6：復原 1 000 框的提交 ----
    void i6_undoOfA1000BoxCommit() {
        // 5 MP 板子上約 1 000 格：樣本 56 × 38，格距 1.5 倍。
        fake::FakeSpec world = bigWorld();
        world.weakFraction = 0.0;
        world.missingFraction = 0.0;
        Rig r(world, false);
        r.window.resize(1280, 800);
        r.open(kBoardW, kBoardH);
        QVERIFY(r.teachSample(QRectF(1200, 900, 56, 38)));
        r.key(Qt::Key_Return);  // 接受：一個 Command
        QCOMPARE_STATE(r.state(), InteractionState::Idle);
        const int boxes = r.boxCount();
        QVERIFY2(boxes > 900 && boxes < 1300, qPrintable(QString::number(boxes)));
        QCOMPARE(static_cast<int>(r.document().components().size()), boxes);
        paintPending(r);

        std::vector<double> undoMs, redoMs;
        for (int i = 0; i < kWarmup + kRuns; ++i) {
            QElapsedTimer t;
            t.start();
            r.window.undoAction()->trigger();
            paintPending(r);
            const double u = msSince(t);
            QCOMPARE_STATE(r.state(), InteractionState::Preview);
            QCOMPARE(r.document().components().size(), std::size_t{1});  // 復原接受：板上只剩樣本（ADR-025）

            t.start();
            r.window.redoAction()->trigger();
            paintPending(r);
            const double d = msSince(t);
            QCOMPARE_STATE(r.state(), InteractionState::Idle);
            QCOMPARE(static_cast<int>(r.document().components().size()), boxes);
            if (i >= kWarmup) {
                undoMs.push_back(u);
                redoMs.push_back(d);
            }
        }
        record("I6", QStringLiteral("undo of a %1 box commit (accept) to repainted").arg(boxes), undoMs,
               perf::budgetOf("I6", QStringLiteral(AOI_SOURCE_DIR)),
               "whole pipeline: document undo, scene rebuild (back to the proposal), canvas, tree, card, then paint");
        record("I6-redo", QStringLiteral("diagnostic: redo of the same commit"), redoMs, -1, "diagnostic");
    }

    // I6 的核心部分：只有文件的 Command，沒有任何畫面。
    void i6_documentCommandsAlone() {
        for (std::size_t n : {std::size_t{1000}, std::size_t{5000}}) {
            const ProposalSession session(makeBigProposal(n), Rect{0, 0, 10, 10}, RegionMode::Sample, 0.0);
            InspectionDocument doc;
            doc.reset("board.png", kBoardW, kBoardH, 0.0);
            QVERIFY(doc.addSample(Rect{0, 0, 24, 16}) > 0);  // ADR-025：提案的候選指向板子上的類型 t1，接受前要先有樣本
            std::vector<double> acceptMs, undoMs, redoMs;
            for (int i = 0; i < kWarmup + kRuns; ++i) {
                QElapsedTimer t;
                t.start();
                if (i == 0) doc.accept(session);
                else doc.redo();
                const double a = msSince(t);
                t.start();
                const UndoOutcome out = doc.undo();
                const double u = msSince(t);
                QVERIFY(out.performed);
                if (i >= kWarmup) {
                    acceptMs.push_back(a);
                    undoMs.push_back(u);
                }
            }
            record(QStringLiteral("I6-doc%1").arg(n), QStringLiteral("diagnostic: document undo of a %1 component accept (no UI)").arg(n), undoMs, -1, "diagnostic");
            record(QStringLiteral("I6-doc%1-do").arg(n), QStringLiteral("diagnostic: document accept/redo of %1 components (no UI)").arg(n), acceptMs, -1, "diagnostic");
        }
    }

    void cleanupTestCase() {
        QJsonArray rows;
        for (const Metric& m : metrics()) rows.append(perf::toJson(m));
        QJsonObject root;
        root["schema"] = 1;
        root["step"] = "S04";
        root["method"] = "warm-up 5, measured 30 (I3: 3 warm-up, 20 searches); median and p95; time = the user action until the dirty region is painted";
        root["machine"] = machine_;
        root["metrics"] = rows;
        root["repaintByRejectedFraction"] = rejectedTable_;
        root["scalingWithObjectCount"] = scalingTable_;
        root["u3PhasesAt5832"] = phaseTable_;
        const QString outDir = qEnvironmentVariable("AOI_PERF_OUT", QDir::current().filePath(QStringLiteral("perf-results")));
        QDir().mkpath(outDir);
        QFile out(QDir(outDir).filePath(QStringLiteral("ui-s04-%1.json").arg(QGuiApplication::platformName())));
        if (out.open(QIODevice::WriteOnly)) out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        qInfo().noquote() << "wrote" << out.fileName();

#ifdef AOI_PERF_ENFORCE
        for (const Metric& m : metrics()) {
            if (m.budget > 0 && !m.pass()) {
                if (knownOver(m.id)) {
                    qWarning().noquote() << "KNOWN OVER BUDGET:" << m.id << "p95" << m.stats.p95 << "ms, budget" << m.budget << "ms (see perf.md)";
                    continue;
                }
                qWarning().noquote() << "OVER BUDGET:" << m.id << "p95" << m.stats.p95 << "ms, budget" << m.budget << "ms";
                failedBudget_ = true;
            }
        }
        QVERIFY2(!failedBudget_, "one or more budgets were exceeded (see the log)");
#endif
    }

private:
    bool failedBudget_ = false;
};

QTEST_MAIN(PerfUiS04)
#include "perf_ui_s04.moc"
