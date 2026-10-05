// PerfProbe（環狀緩衝與 JSON）與 ScopedTimer。開／關的單次成本在 perf_core 量，這裡只驗證行為。
#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include <atomic>
#include <chrono>
#include <set>
#include <thread>
#include <vector>

#include "application/ports/IPerfProbe.h"
#include "infrastructure/perf/PerfProbe.h"

using namespace aoi;
using namespace std::chrono_literals;

namespace {

// 計數用的假 probe：確認「關閉時 record 不會被呼叫」。
struct SpyProbe final : IPerfProbe {
    bool enabled() const noexcept override { return on.load(); }
    void record(const char*, std::int64_t) noexcept override { ++records; }
    std::atomic<bool> on{false};
    std::atomic<int> records{0};
};

QJsonObject parse(const std::string& json) {
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json), &error);
    if (error.error != QJsonParseError::NoError) qWarning("JSON error: %s at %d", qPrintable(error.errorString()), error.offset);
    return doc.object();
}

}  // namespace

class TstPerfProbe : public QObject {
    Q_OBJECT

private slots:
    void aDisabledProbeIsNeverWrittenTo() {
        SpyProbe spy;
        { ScopedTimer t(&spy, "x"); }
        QCOMPARE(spy.records.load(), 0);
        spy.on = true;
        { ScopedTimer t(&spy, "x"); }
        QCOMPARE(spy.records.load(), 1);
    }

    void aNullProbeIsSafe() {
        { ScopedTimer t(nullptr, "x"); }
        QVERIFY(true);  // 沒有當機就是通過
    }

    // 「開或關」在建構時決定一次：關閉時開始的計時，中途打開也不記（否則會記到沒有起點的半段時間）；
    // 開啟時開始的計時，中途關掉照記（起點已經有了）。
    void theSwitchIsReadOnlyOnceAtConstruction() {
        SpyProbe spy;
        {
            ScopedTimer t(&spy, "x");  // 開始時是關的
            spy.on = true;
        }
        QCOMPARE(spy.records.load(), 0);
        {
            ScopedTimer t(&spy, "x");  // 開始時是開的
            spy.on = false;
        }
        QCOMPARE(spy.records.load(), 1);
    }

    void anEnabledProbeRecordsNameAndDuration() {
        infra::PerfProbe probe(16, true);
        {
            ScopedTimer t(&probe, "sleepy");
            std::this_thread::sleep_for(20ms);
        }
        const auto records = probe.snapshot();
        QCOMPARE(records.size(), std::size_t{1});
        QCOMPARE(std::string(records[0].name), std::string("sleepy"));
        QVERIFY2(records[0].durationNs >= 15'000'000, "a 20 ms sleep must record at least 15 ms");
        QVERIFY(records[0].durationNs < 2'000'000'000);
        QVERIFY(records[0].endNs >= records[0].durationNs);
        QVERIFY(records[0].thread >= 1);
    }

    void theProbeIsOffUntilSwitchedOn() {
        infra::PerfProbe probe;
        QVERIFY(!probe.enabled());
        probe.setEnabled(true);
        QVERIFY(probe.enabled());
    }

    // 環狀緩衝：滿了覆蓋最舊的，並累計被覆蓋的筆數；快照由舊到新。
    void theRingKeepsTheNewestRecordsAndCountsWhatItDropped() {
        infra::PerfProbe probe(4, true);
        for (int i = 1; i <= 10; ++i) probe.record("r", i * 1000);
        QCOMPARE(probe.recorded(), std::size_t{4});
        QCOMPARE(probe.dropped(), std::size_t{6});
        const auto records = probe.snapshot();
        QCOMPARE(records.size(), std::size_t{4});
        for (std::size_t k = 0; k < 4; ++k) QCOMPARE(records[k].durationNs, static_cast<std::int64_t>((7 + k) * 1000));
    }

    void theRingBeforeItWrapsIsInOrder() {
        infra::PerfProbe probe(8, true);
        for (int i = 1; i <= 3; ++i) probe.record("r", i);
        const auto records = probe.snapshot();
        QCOMPARE(records.size(), std::size_t{3});
        QCOMPARE(records[0].durationNs, std::int64_t{1});
        QCOMPARE(records[2].durationNs, std::int64_t{3});
        QCOMPARE(probe.dropped(), std::size_t{0});
    }

    void clearEmptiesTheRing() {
        infra::PerfProbe probe(4, true);
        for (int i = 0; i < 9; ++i) probe.record("r", 1);
        probe.clear();
        QCOMPARE(probe.recorded(), std::size_t{0});
        QCOMPARE(probe.dropped(), std::size_t{0});
        QVERIFY(probe.snapshot().empty());
    }

    void aCapacityOfZeroIsRaisedToOne() {
        infra::PerfProbe probe(0, true);
        probe.record("r", 5);
        probe.record("r", 6);
        QCOMPARE(probe.capacity(), std::size_t{1});
        QCOMPARE(probe.snapshot().front().durationNs, std::int64_t{6});
    }

    void jsonSummaryHasExactPercentiles() {
        infra::PerfProbe probe(1024, true);
        for (int ms = 1; ms <= 100; ++ms) probe.record("a", static_cast<std::int64_t>(ms) * 1'000'000);
        probe.record("b", 2'000'000);
        const QJsonObject root = parse(probe.toJson());
        QCOMPARE(root["schema"].toInt(), 1);
        QCOMPARE(root["recorded"].toInt(), 101);
        QCOMPARE(root["dropped"].toInt(), 0);
        const QJsonArray summary = root["summary"].toArray();
        QCOMPARE(summary.size(), 2);
        QJsonObject a;
        QJsonObject b;
        for (const QJsonValue& v : summary) (v["name"].toString() == "a" ? a : b) = v.toObject();
        QCOMPARE(a["count"].toInt(), 100);
        QCOMPARE(a["minMs"].toDouble(), 1.0);
        QCOMPARE(a["medianMs"].toDouble(), 50.0);  // 最近排名法：第 ceil(0.5 × 100) ＝ 50 個
        QCOMPARE(a["p95Ms"].toDouble(), 95.0);
        QCOMPARE(a["maxMs"].toDouble(), 100.0);
        QCOMPARE(a["totalMs"].toDouble(), 5050.0);
        QCOMPARE(b["count"].toInt(), 1);
        QCOMPARE(b["p95Ms"].toDouble(), 2.0);
    }

    void jsonCanLeaveOutTheRawRecords() {
        infra::PerfProbe probe(16, true);
        probe.record("a", 1000);
        const QJsonObject full = parse(probe.toJson(true));
        QVERIFY(full.contains("records"));
        QCOMPARE(full["records"].toArray().size(), 1);
        QCOMPARE(full["records"].toArray()[0].toObject()["durationNs"].toInteger(), qint64{1000});
        const QJsonObject slim = parse(probe.toJson(false));
        QVERIFY(!slim.contains("records"));
        QVERIFY(slim.contains("summary"));
    }

    void anEmptyProbeStillWritesValidJson() {
        infra::PerfProbe probe(16, true);
        const QJsonObject root = parse(probe.toJson());
        QCOMPARE(root["recorded"].toInt(), 0);
        QVERIFY(root["summary"].toArray().isEmpty());
        QVERIFY(root["records"].toArray().isEmpty());
    }

    void namesAreEscapedInJson() {
        infra::PerfProbe probe(16, true);
        const char* nasty = "a\"b\\c\nd\te\x01";
        probe.record(nasty, 10);
        const QJsonObject root = parse(probe.toJson());
        const QJsonArray summary = root["summary"].toArray();
        QCOMPARE(summary.size(), 1);
        QCOMPARE(summary[0]["name"].toString().toStdString(), std::string(nasty));
    }

    void manyThreadsCanRecordAtOnce() {
        infra::PerfProbe probe(8192, true);
        constexpr int kThreads = 4;
        constexpr int kEach = 1000;
        std::vector<std::thread> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&] {
                for (int i = 0; i < kEach; ++i) probe.record("w", 100);
            });
        }
        for (auto& th : threads) th.join();
        QCOMPARE(probe.recorded(), static_cast<std::size_t>(kThreads * kEach));
        std::set<std::uint32_t> threadNumbers;
        for (const auto& r : probe.snapshot()) threadNumbers.insert(r.thread);
        QCOMPARE(threadNumbers.size(), static_cast<std::size_t>(kThreads));
    }

    void writeJsonWritesAFileThatParses() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        infra::PerfProbe probe(16, true);
        probe.record("a", 1'000'000);
        const QString path = dir.filePath(QStringLiteral("perf-測試.json"));  // 路徑含非 ASCII 字元
        const Result<void> written = probe.writeJson(path.toUtf8().toStdString());
        QVERIFY2(written.ok(), written.ok() ? "" : written.error().message.c_str());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QVERIFY(doc.isObject());
        QCOMPARE(doc.object()["recorded"].toInt(), 1);
    }

    void writeJsonToAMissingFolderIsAnIoError() {
        infra::PerfProbe probe(16, true);
        const Result<void> written = probe.writeJson("Z:/this/folder/does/not/exist/perf.json");
        QVERIFY(!written.ok());
        QVERIFY(written.error().code == ErrorCode::IoFailed);
    }
};

QTEST_APPLESS_MAIN(TstPerfProbe)
#include "tst_perf_probe.moc"
