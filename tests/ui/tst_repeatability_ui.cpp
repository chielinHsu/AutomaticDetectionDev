#include <QtTest>
#include <QComboBox>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <cmath>
#include "presentation/repeatability/RepeatabilityWindow.h"
#include "presentation/repeatability/RepeatabilityPlot.h"
#include "presentation/repeatability/RepeatabilityComparisonDialog.h"
#include "presentation/theme/ThemeManager.h"
#include "presentation/i18n/Translations.h"

using namespace aoi;
using namespace aoi::ui;
using namespace aoi::repeatability;
class Source : public IRepeatabilitySource {
public:
    Result<Dataset> loadFolder(const std::string& path, const CancelToken&) noexcept override {
        Dataset d{path,path,{}};
        for (double v : {1.,2.,3.}) { MeasurementRun r; r.unit="µm"; r.boxes[{"0","Pin"}]={{"CO",v},{"XO",v*2},{"YO",v*3},{"RO",v*4}}; d.runs.push_back(r); }
        return d;
    }
    Result<AnalysisSession> readSession(const std::string&) noexcept override { return makeError(ErrorCode::IoFailed,"test"); }
    Result<void> writeSession(const std::string&, const AnalysisSession&) noexcept override { return {}; }
    Result<void> exportTables(const std::string&, const AnalysisSession&, const std::vector<std::string>&, Statistic, std::size_t) noexcept override { return {}; }
};
class RepeatabilityUiTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QVERIFY(Translations::install(QStringLiteral("zh_TW")));
        QVERIFY(qtTrId("repeat.status").contains(QStringLiteral("%1")));
    }
    void adaptiveFoldersAndMetricChoices() {
        ThemeManager theme(*qobject_cast<QApplication*>(QCoreApplication::instance()), ThemeTokens::builtin());
        RepeatabilityWindow window(std::make_shared<Source>(), theme); window.show();
        window.loadFolder(QStringLiteral("A")); QTRY_COMPARE(window.presenter().state().analyses.size(), std::size_t(1));
        QCOMPARE(window.findChildren<RepeatabilityPlot*>().size(), qsizetype(4));
        auto settings = window.presenter().state().session.metrics;
        settings.at("CO").tolerance = 700;
        window.presenter().configure(settings);
        QTRY_VERIFY(!window.presenter().busy());
        const auto& stats = window.presenter().state().analyses[0].metrics.at("CO").begin()->second;
        QVERIFY(std::abs(*stats.pt - 6.0 / 700) < 1e-12);
        window.loadFolder(QStringLiteral("B")); QTRY_COMPARE(window.presenter().state().analyses.size(), std::size_t(2));
        QCOMPARE(window.findChildren<RepeatabilityPlot*>().size(), qsizetype(2));
        auto* combo = window.findChild<QComboBox*>(QStringLiteral("repeatMetric1")); QVERIFY(combo); combo->setCurrentText(QStringLiteral("YO"));
        QCoreApplication::processEvents();
        QCOMPARE(window.findChild<QComboBox*>(QStringLiteral("repeatMetric1"))->currentText(), QStringLiteral("YO"));
        window.loadFolder(QStringLiteral("C")); QTRY_COMPARE(window.presenter().state().analyses.size(), std::size_t(3));
        window.loadFolder(QStringLiteral("D")); QTRY_COMPARE(window.presenter().state().analyses.size(), std::size_t(4));
        QCOMPARE(window.findChildren<RepeatabilityPlot*>().size(), qsizetype(4));
        QVERIFY(!window.findChild<QPushButton*>(QStringLiteral("addFolder"))->isEnabled());
        window.presenter().removeFolder(3); window.presenter().removeFolder(2); window.presenter().removeFolder(1);
        QCOMPARE(window.findChildren<RepeatabilityPlot*>().size(), qsizetype(4));
        auto* list = window.findChild<QListWidget*>(QStringLiteral("metricsList")); QVERIFY(list);
        list->item(0)->setCheckState(Qt::Unchecked);
        QCOMPARE(window.findChildren<RepeatabilityPlot*>().size(), qsizetype(3));
    }
    void failedOpenRetainsExistingData() {
        RepeatabilityPresenter p(std::make_shared<Source>()); QSignalSpy failure(&p, &RepeatabilityPresenter::failed);
        p.loadFolder(QStringLiteral("A")); QTRY_COMPARE(p.state().analyses.size(), std::size_t(1));
        p.loadSession(QStringLiteral("bad")); QTRY_COMPARE(failure.count(), 1); QCOMPARE(p.state().analyses.size(), std::size_t(1));
    }
    void folderSummaryAndSelectedBoxShowBothStatistics() {
        auto state = std::make_shared<RepeatabilityState>();
        Source source;
        state->session.datasets = {source.loadFolder("A", {}).value(), source.loadFolder("B", {}).value()};
        for (auto& dataset : state->session.datasets) for (auto& run : dataset.runs) {
            const double v = *run.boxes.at({"0", "Pin"}).at("XO");
            run.boxes[{"0", "Other"}] = {{"XO", v * 10}};
            run.boxes[{"1", "Pin"}] = {{"XO", v * 3}};
        }
        for (auto& run : state->session.datasets[0].runs) run.boxes[{"0", "OnlyA"}] = {{"XO", 100}};
        for (auto& run : state->session.datasets[1].runs) {
            for (auto& [key, fields] : run.boxes) { (void)key; *fields.at("XO") *= 2; }
            run.boxes[{"0", "OnlyB"}] = {{"XO", 200}};
        }
        for (const auto& d : state->session.datasets) state->analyses.push_back(analyze(d, state->session.metrics).value());
        RepeatabilityComparisonDialog dialog(state, {"CO", "XO"}); dialog.show();
        auto* metric = dialog.findChild<QComboBox*>(QStringLiteral("comparisonMetric")); QVERIFY(metric);
        auto* filter = dialog.findChild<QLineEdit*>(QStringLiteral("boxName")); QVERIFY(filter);
        metric->setCurrentText(QStringLiteral("XO"));
        auto* pt = dialog.findChild<RepeatabilityPlot*>(QStringLiteral("ptComparison")); QVERIFY(pt);
        auto* range = dialog.findChild<RepeatabilityPlot*>(QStringLiteral("rangeComparison")); QVERIFY(range);
        QCOMPARE(pt->points().size(), std::size_t(2)); QCOMPARE(pt->points()[0].key.box, std::string("A"));
        QCOMPARE(*range->points()[0].value, 40.0); QCOMPARE(*range->points()[1].value, 80.0);
        QVERIFY(std::abs(*pt->points()[0].value - 48.0) < 1e-12);
        filter->setText(QStringLiteral("Pin"));
        QVERIFY(!pt->points()[0].value); // Duplicate Box name needs its Component.
        filter->setText(QStringLiteral("0/Pin"));
        QCOMPARE(*range->points()[0].value, 4.0); QCOMPARE(*range->points()[1].value, 8.0);
        QVERIFY(std::abs(*pt->points()[0].value - 4.8) < 1e-12);
        QVERIFY(std::abs(*pt->points()[1].value - 9.6) < 1e-12);
        filter->setText(QStringLiteral("OnlyA"));
        QVERIFY(pt->points()[0].value); QVERIFY(!pt->points()[1].value);
        filter->setText(QStringLiteral("Missing"));
        QVERIFY(!range->points()[0].value); QVERIFY(!range->points()[1].value);
        filter->clear(); QCOMPARE(*range->points()[0].value, 40.0);
        QVERIFY(!dialog.grab().isNull());
        auto changed = std::make_shared<RepeatabilityState>(*state);
        changed->session.metrics.at("XO").tolerance.reset();
        for (std::size_t g = 0; g < changed->analyses.size(); ++g) changed->analyses[g] = analyze(changed->session.datasets[g], changed->session.metrics).value();
        RepeatabilityComparisonDialog noSpec(changed, {"XO"});
        QVERIFY(!noSpec.findChild<RepeatabilityPlot*>(QStringLiteral("ptComparison"))->points()[0].value);
        QCOMPARE(*noSpec.findChild<RepeatabilityPlot*>(QStringLiteral("rangeComparison"))->points()[0].value, 40.0);
    }
};
QTEST_MAIN(RepeatabilityUiTest)
#include "tst_repeatability_ui.moc"
