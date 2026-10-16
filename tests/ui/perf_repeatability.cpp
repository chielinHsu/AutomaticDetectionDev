#include <QtTest>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFont>
#include <QLineEdit>
#include <QComboBox>
#include <QTemporaryDir>
#include <QPushButton>
#include <QTimer>
#include "PerfSupport.h"
#include "infrastructure/storage/CsvRepeatabilitySource.h"
#include "presentation/repeatability/RepeatabilityWindow.h"
#include "presentation/repeatability/RepeatabilityComparisonDialog.h"
#include "presentation/theme/ThemeManager.h"
#include "presentation/i18n/Translations.h"

using namespace aoi;
using namespace aoi::repeatability;
class RepeatabilityPerformance : public QObject {
    Q_OBJECT
private slots:
    void fourGroupsThirtyRuns() {
        Dataset dataset{"synthetic","synthetic",{}};
        for (int run = 0; run < 30; ++run) {
            MeasurementRun r; r.unit = "µm";
            for (int box = 0; box < 703; ++box)
                r.boxes[{"0", "Pin_" + std::to_string(box)}] = {{"CO", double(run % 5 + box % 7)}, {"XO", double(run % 3)}, {"YO", double(run % 2)}, {"RO", double(run % 4)}};
            dataset.runs.push_back(std::move(r));
        }
        std::vector<double> times;
        for (int i = 0; i < 35; ++i) {
            QElapsedTimer timer; timer.start();
            for (int group = 0; group < 4; ++group) {
                const auto analysis = analyze(dataset, defaultMetrics()); QVERIFY(analysis);
                QCOMPARE(analysis.value().metrics.at("CO").size(), std::size_t(703));
            }
            if (i >= 5) times.push_back(static_cast<double>(timer.nsecsElapsed()) / 1e6);
        }
        aoi::test::perf::Metric metric;
        metric.id = QStringLiteral("RS-ANALYZE-4x30x703"); metric.label = QStringLiteral("Four folders, thirty runs, 703 Boxes, four metrics");
        metric.stats = aoi::test::perf::summarize(times);
        metric.note = QStringLiteral("Informational: no repeatability budget is defined.");
        const QJsonObject result{{"machine", aoi::test::perf::machineFingerprint(QStringLiteral(AOI_SOURCE_DIR), QStringLiteral(AOI_BUILD_TYPE))},
                                 {"metrics", QJsonArray{aoi::test::perf::toJson(metric)}}};
        const QString dir = QStringLiteral(AOI_SOURCE_DIR) + QStringLiteral("/perf/results"); QVERIFY(QDir().mkpath(dir));
        QFile file(dir + QStringLiteral("/repeatability_") + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-hhmmss")) + QStringLiteral(".json"));
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(result).toJson());
        qInfo() << "RS-ANALYZE median(ms)" << metric.stats.median << "p95(ms)" << metric.stats.p95;
    }
    void nativeScreenshots() {
        const QString input = qEnvironmentVariable("AOI_REPEATABILITY_DATA"), output = qEnvironmentVariable("AOI_REPEATABILITY_SCREENSHOTS");
        if (input.isEmpty() || output.isEmpty()) QSKIP("Set repeatability input and screenshot output folders.");
        QVERIFY(QDir().mkpath(output));
        QVERIFY(ui::Translations::install(QStringLiteral("zh_TW")));
        QApplication::setFont(QFont(QStringLiteral("Microsoft JhengHei"), 9));
        ui::ThemeManager theme(*qobject_cast<QApplication*>(QCoreApplication::instance()), ui::ThemeTokens::builtin());
        ui::RepeatabilityWindow window(std::make_shared<infra::CsvRepeatabilitySource>(), theme);
        window.resize(1440, 1000); window.show(); window.loadFolder(input);
        QTRY_COMPARE_WITH_TIMEOUT(window.presenter().state().analyses.size(), std::size_t(1), 15000);
        QCoreApplication::processEvents(); QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("repeatability-single.png"))));
        bool settingsCaptured = false;
        QTimer::singleShot(0, &window, [&] {
            if (QWidget* dialog = QApplication::activeModalWidget()) {
                settingsCaptured = dialog->grab().save(QDir(output).filePath(QStringLiteral("repeatability-settings.png")));
                dialog->close();
            }
        });
        window.findChild<QPushButton*>(QStringLiteral("metricSettings"))->click();
        QVERIFY(settingsCaptured);
        QTemporaryDir copies; QVERIFY(copies.isValid());
        for (int group = 1; group < 4; ++group) {
            const QString copyDir = copies.filePath(QStringLiteral("comparison-copy-%1").arg(group)); QVERIFY(QDir().mkpath(copyDir));
            for (const QFileInfo& csv : QDir(input).entryInfoList({QStringLiteral("*.csv")}, QDir::Files)) QVERIFY(QFile::copy(csv.absoluteFilePath(), QDir(copyDir).filePath(csv.fileName())));
            window.loadFolder(copyDir); QTRY_COMPARE_WITH_TIMEOUT(window.presenter().state().analyses.size(), static_cast<std::size_t>(group + 1), 15000);
        }
        QCoreApplication::processEvents(); QVERIFY(window.grab().save(QDir(output).filePath(QStringLiteral("repeatability-four-copies.png"))));
        ui::RepeatabilityComparisonDialog comparison(window.presenter().snapshot(), {"CO", "XO", "YO", "RO"}, &window);
        comparison.show(); comparison.findChild<QComboBox*>(QStringLiteral("comparisonMetric"))->setCurrentText(QStringLiteral("XO"));
        QCoreApplication::processEvents(); QVERIFY(comparison.grab().save(QDir(output).filePath(QStringLiteral("repeatability-folder-lines-summary.png"))));
        const auto& key = window.presenter().state().analyses.front().metrics.at("XO").begin()->first;
        comparison.findChild<QLineEdit*>(QStringLiteral("boxName"))->setText(QString::fromStdString(key.component + "/" + key.box));
        QCoreApplication::processEvents(); QVERIFY(comparison.grab().save(QDir(output).filePath(QStringLiteral("repeatability-folder-lines-box.png"))));
    }
};
QTEST_MAIN(RepeatabilityPerformance)
#include "perf_repeatability.moc"
