#pragma once

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
#include <cmath>
#include <numeric>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "TestSupport.h"

// 效能量測共用的輔助（S04 起；S02 的 perf_ui_baseline 有自己的一份，沒有改動它）：統計、預算查詢、機器指紋。
// 方法見 docs/perf-test-plan.md：丟掉前 5 次熱機，正式量 30 次，回報中位數與 p95；每份結果記錄機器指紋。
namespace aoi::test::perf {

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

struct Metric {
    QString id;
    QString label;
    QString unit = QStringLiteral("ms");
    Stats stats;
    double budget = -1;  // < 0 ＝ 沒有預算（資訊性的量測）
    QString note;
    bool pass() const { return budget < 0 || stats.p95 <= budget; }
};

inline double round3(double v) { return std::round(v * 1000.0) / 1000.0; }

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
    if (!m.note.isEmpty()) o["note"] = m.note;
    return o;
}

// 預算：perf/budgets.json 是來源。
inline double budgetOf(const QString& id, const QString& sourceDir) {
    QFile f(sourceDir + QStringLiteral("/perf/budgets.json"));
    if (!f.open(QIODevice::ReadOnly)) return -1;
    const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).object().value("budgets").toArray();
    for (const QJsonValue& v : arr)
        if (v.toObject().value("id").toString() == id) return v.toObject().value("limit").toDouble(-1);
    return -1;
}

inline QString runCommand(const QString& program, const QStringList& args, const QString& workingDir) {
    QProcess p;
    p.setWorkingDirectory(workingDir);
    p.start(program, args);
    if (!p.waitForFinished(5000)) return {};
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

inline QJsonObject machineFingerprint(const QString& sourceDir, const QString& buildType) {
    QJsonObject m;
    QSettings cpu(QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), QSettings::NativeFormat);
    m["cpu"] = cpu.value(QStringLiteral("ProcessorNameString"), QStringLiteral("unknown")).toString().trimmed();
    m["logicalCores"] = QThread::idealThreadCount();
#ifdef Q_OS_WIN
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) m["ramGB"] = std::round(static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024 * 1024) * 10) / 10;
#endif
    m["os"] = QSysInfo::prettyProductName() + QStringLiteral(" (kernel ") + QSysInfo::kernelVersion() + QLatin1Char(')');
    m["qt"] = QString::fromLatin1(qVersion());
#ifdef _MSC_FULL_VER
    m["compiler"] = QStringLiteral("MSVC %1").arg(_MSC_FULL_VER);
#endif
    m["buildType"] = buildType;
    m["qpaPlatform"] = QGuiApplication::platformName();
    const QString head = runCommand(QStringLiteral("git"), {QStringLiteral("rev-parse"), QStringLiteral("--short"), QStringLiteral("HEAD")}, sourceDir);
    const QString dirty = runCommand(QStringLiteral("git"), {QStringLiteral("status"), QStringLiteral("--porcelain")}, sourceDir);
    m["commit"] = head.isEmpty() ? QStringLiteral("unknown") : head + (dirty.isEmpty() ? QString() : QStringLiteral("+uncommitted"));
    m["machineName"] = QSysInfo::machineHostName();
    return m;
}

}  // namespace aoi::test::perf
