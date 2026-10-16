#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cmath>
#include <numeric>
#include "infrastructure/storage/CsvRepeatabilitySource.h"

using namespace aoi;
using namespace aoi::repeatability;
namespace {
QByteArray report(const QByteArray& values = "-48,31,54,62", const QByteArray& extraHeader = {}, const QByteArray& extraValue = {}) {
    return "Date: 2026-10-14 11:19:54\nUnit: Micron\nComponent Name: 0\n=== COMPONENT LEVEL ===\nItem,Avg\nCO,5\n\n=== BOX LEVEL - AdvancePinInspect ===\nBox,CO,XO,YO,RO" + extraHeader + ",Decision\n\"Pin,1\"," + values + extraValue + ",Fail\n";
}
Dataset sample() {
    Dataset d{"sample", "sample-folder", {}};
    for (const double v : {-48.0, -41.0, -43.0, -44.0, -46.0}) {
        MeasurementRun r; r.source = std::to_string(d.runs.size()); r.unit = "µm";
        r.boxes[{"0", "Pin_29_29"}] = {{"CO", v}, {"NEW", v * 2}}; d.runs.push_back(r);
    }
    return d;
}
void save(const QString& file, const QByteArray& bytes) { QFile f(file); QVERIFY(f.open(QIODevice::WriteOnly)); QCOMPARE(f.write(bytes), bytes.size()); }
}
class RepeatabilityTest : public QObject {
    Q_OBJECT
private slots:
    void excelFormulaAndSampleDenominator() {
        const std::vector<double> values{-48, -41, -43, -44, -46};
        const Statistics s = summarize(values, 5, 350);
        QCOMPARE(s.count, std::size_t(5)); QCOMPARE(*s.range, 7.0); QCOMPARE(*s.average, -44.4);
        QVERIFY(std::abs(*s.deviation - 2.701851217221259) < 1e-12);
        QVERIFY(std::abs(*s.pt - 0.04631744943807873) < 1e-12);
        const std::vector<double> one{0}; QVERIFY(!summarize(one, 1, 350).deviation);
        QVERIFY(!summarize(values, 5, 0).pt); QVERIFY(!summarize(values, 5, {}).pt);
    }
    void quotedNamesNewMetricsAndInvalidInput() {
        auto parsed = infra::parseInspectionCsv(report("-48,31,54,62", ",NEW", ",2.5"), "test");
        QVERIFY(parsed); QCOMPARE(parsed.value().boxes.size(), std::size_t(1));
        QCOMPARE(*parsed.value().boxes.at({"0", "Pin,1"}).at("NEW"), 2.5);
        const auto blankBetweenRows = infra::parseInspectionCsv(report() + "\nSecond,1,2,3,4,Fail\n", "test");
        QVERIFY(blankBetweenRows); QCOMPARE(blankBetweenRows.value().boxes.size(), std::size_t(2));
        QVERIFY(!infra::parseInspectionCsv(report() + ",1,2,3,4,Fail\n", "test"));
        QVERIFY(!infra::parseInspectionCsv(report("NaN,31,54,62"), "test"));
        QVERIFY(!infra::parseInspectionCsv(report() + "\"Pin,1\",1,2,3,4,Fail\n", "test"));
        QVERIFY(!infra::parseInspectionCsv("Box,CO\nA,1", "test"));
        CancelSource cancel; cancel.cancel(); QVERIFY(!infra::parseInspectionCsv(report(), "test", cancel.token()));
    }
    void alignKeysAndExposeMissingValues() {
        AnalysisSession session; session.datasets = {sample()}; QVERIFY(discoverMetrics(session));
        QVERIFY(!session.metrics.at("NEW").tolerance);
        const auto complete = analyze(session.datasets[0], session.metrics); QVERIFY(complete);
        const auto noSpec = compare({complete.value()}, "NEW", Statistic::Pt);
        QCOMPARE(noSpec.commonBoxes, std::size_t(1)); QVERIFY(!noSpec.values[0]);
        session.datasets[0].runs.back().boxes.clear();
        const auto result = analyze(session.datasets[0], session.metrics); QVERIFY(result);
        const auto& stats = result.value().metrics.at("CO").begin()->second;
        QCOMPARE(stats.count, std::size_t(4)); QCOMPARE(stats.expected, std::size_t(5)); QVERIFY(!stats.complete());
        QVERIFY(!compare({result.value()}, "CO", Statistic::Pt).values[0]);
        session.datasets[0].runs.back().boxes = {{{"0", "another"}, {{"CO", 10}}}};
        const auto changed = analyze(session.datasets[0], session.metrics); QVERIFY(changed);
        QCOMPARE(changed.value().metrics.at("CO").size(), std::size_t(2));
    }
    void commonBoxPopulationAndUnitConversion() {
        AnalysisSession session; session.datasets = {sample(), sample()}; QVERIFY(discoverMetrics(session));
        session.datasets[1].runs[0].boxes[{"0", "only-B"}] = {{"CO", 1000}};
        auto a = analyze(session.datasets[0], session.metrics), b = analyze(session.datasets[1], session.metrics);
        QVERIFY(a && b); auto compared = compare({a.value(), b.value()}, "CO", Statistic::Pt);
        QCOMPARE(compared.commonBoxes, std::size_t(1)); QCOMPARE(compared.values[0], compared.values[1]);
        for (auto& r : session.datasets[0].runs) { r.unit = "mm"; *r.boxes.begin()->second.at("CO") /= 1000; }
        auto mm = analyze(session.datasets[0], session.metrics); QVERIFY(mm);
        QVERIFY(std::abs(*mm.value().metrics.at("CO").begin()->second.average + 44.4) < 1e-10);
    }
    void folderOrderSessionAndExports() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        save(dir.filePath("10.csv"), report()); save(dir.filePath("2.csv"), report()); save(dir.filePath("1.csv"), report());
        infra::CsvRepeatabilitySource source;
        auto d = source.loadFolder(dir.path().toStdString(), {}); QVERIFY(d);
        QCOMPARE(d.value().runs[0].source, std::string("1.csv")); QCOMPARE(d.value().runs[1].source, std::string("2.csv"));
        AnalysisSession session; session.datasets.push_back(d.value());
        const std::string path = dir.filePath("session.json").toStdString(); QVERIFY(source.writeSession(path, session));
        const auto restored = source.readSession(path); QVERIFY(restored);
        QCOMPARE(restored.value().datasets[0].runs[0].boxes, session.datasets[0].runs[0].boxes);
        QVERIFY(source.exportTables(dir.path().toStdString(), session, {"CO"}, Statistic::Pt, 0));
        QFile summary(dir.filePath("summary.csv")); QVERIFY(summary.open(QIODevice::ReadOnly)); QVERIFY(summary.readAll().contains("CommonCompleteBoxes"));
        QFile details(dir.filePath("details.csv")); QVERIFY(details.open(QIODevice::ReadOnly)); QVERIFY(details.readAll().contains("\"Pin,1\""));
        save(dir.filePath("bad.json"), "{\"schema\":2}"); QVERIFY(!source.readSession(dir.filePath("bad.json").toStdString()));
    }
    void realFiveRuns() {
        const QString folder = qEnvironmentVariable("AOI_REPEATABILITY_DATA");
        if (folder.isEmpty()) QSKIP("Set AOI_REPEATABILITY_DATA to the supplied five-run folder.");
        infra::CsvRepeatabilitySource source; auto d = source.loadFolder(folder.toStdString(), {}); QVERIFY2(d, d ? "" : d.error().message.c_str());
        QCOMPARE(d.value().runs.size(), std::size_t(5));
        const auto result = analyze(d.value(), defaultMetrics()); QVERIFY(result);
        for (const auto& [id, rows] : result.value().metrics) {
            for (const auto& [key, actual] : rows) {
                std::vector<double> raw;
                for (const auto& run : d.value().runs) raw.push_back(*run.boxes.at(key).at(id));
                const double mean = std::accumulate(raw.begin(), raw.end(), 0.0) / static_cast<double>(raw.size());
                double squared = 0; for (const double v : raw) squared += (v - mean) * (v - mean);
                const double sd = std::sqrt(squared / static_cast<double>(raw.size() - 1));
                QVERIFY(std::abs(*actual.average - mean) < 1e-10); QVERIFY(std::abs(*actual.deviation - sd) < 1e-10);
                QVERIFY(std::abs(*actual.pt - 6 * sd / *defaultMetrics().at(id).tolerance) < 1e-10);
            }
        }
        QCOMPARE(result.value().metrics.at("CO").size(), std::size_t(703));
        const auto& co = result.value().metrics.at("CO").at({"0", "Pin_29_29"});
        QVERIFY(std::abs(*co.pt - 0.04631744943807873) < 1e-12);
        std::size_t exceeded = 0; for (const auto& [key, s] : result.value().metrics.at("CO")) { (void)key; if (s.pt && *s.pt > .20) ++exceeded; }
        QCOMPARE(exceeded, std::size_t(93));
    }
};
QTEST_APPLESS_MAIN(RepeatabilityTest)
#include "tst_repeatability.moc"
