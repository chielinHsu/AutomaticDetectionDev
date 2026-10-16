#include "infrastructure/storage/CsvRepeatabilitySource.h"

#include <QtCore/QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtCore/QStringConverter>
#include <algorithm>
#include <cmath>
#include <exception>
#include <set>

namespace aoi::infra {
namespace {
using Rows = std::vector<QStringList>;
Result<Rows> csvRows(const QString& text) {
    Rows rows;
    QStringList row;
    QString field;
    bool quoted = false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (c == QLatin1Char('"')) {
            if (quoted && i + 1 < text.size() && text[i + 1] == c) { field += c; ++i; }
            else if (quoted || field.isEmpty()) quoted = !quoted;
            else field += c;
        } else if (!quoted && (c == QLatin1Char(',') || c == QLatin1Char('\n') || c == QLatin1Char('\r'))) {
            row.push_back(field.trimmed()); field.clear();
            if (c != QLatin1Char(',')) {
                rows.push_back(row); row.clear();
                if (c == QLatin1Char('\r') && i + 1 < text.size() && text[i + 1] == QLatin1Char('\n')) ++i;
            }
        } else field += c;
    }
    if (quoted) return makeError(ErrorCode::FormatInvalid, "Unclosed CSV quoted field.");
    if (!field.isEmpty() || !row.isEmpty()) { row.push_back(field.trimmed()); rows.push_back(row); }
    return rows;
}
}

Result<repeatability::MeasurementRun> parseInspectionCsv(const QByteArray& bytes, const std::string& source,
                                                       const CancelToken& cancel) {
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString text = decoder.decode(bytes);
    if (decoder.hasError()) {
        QStringDecoder system(QStringDecoder::System);
        text = system.decode(bytes);
        if (system.hasError()) return makeError(ErrorCode::FormatInvalid, "CSV encoding is neither UTF-8 nor system encoding.");
    }
    text.remove(QChar(0xfeff));
    auto parsed = csvRows(text);
    if (!parsed) return parsed.error();
    repeatability::MeasurementRun run;
    run.source = source;
    std::string component;
    QStringList headers;
    bool boxSection = false;
    for (const QStringList& row : parsed.value()) {
        if (cancel.cancelled()) return makeError(ErrorCode::Cancelled, "CSV loading cancelled.");
        if (row.isEmpty()) continue;
        if (row.front().isEmpty()) {
            if (boxSection && !headers.isEmpty() && std::any_of(row.begin(), row.end(), [](const QString& field) { return !field.isEmpty(); }))
                return makeError(ErrorCode::FormatInvalid, "Missing Box identifier.");
            continue;
        }
        const QString first = row.front();
        if (first.startsWith(QStringLiteral("Unit:"), Qt::CaseInsensitive)) {
            QString unit = first.mid(5).trimmed();
            if (unit.compare(QStringLiteral("Micron"), Qt::CaseInsensitive) == 0 || unit == QStringLiteral("um")) unit = QStringLiteral("µm");
            run.unit = unit.toStdString();
        }
        if (first.startsWith(QStringLiteral("Component Name:"), Qt::CaseInsensitive)) component = first.mid(15).trimmed().toStdString();
        if (first.startsWith(QStringLiteral("Date:"), Qt::CaseInsensitive)) run.timestamp = first.mid(5).trimmed().toStdString();
        if (first.contains(QStringLiteral("==="))) {
            boxSection = first.contains(QStringLiteral("BOX LEVEL"), Qt::CaseInsensitive);
            headers.clear(); continue;
        }
        if (!boxSection) continue;
        if (headers.isEmpty() && (first.compare(QStringLiteral("Box"), Qt::CaseInsensitive) == 0 || first.compare(QStringLiteral("Box Name"), Qt::CaseInsensitive) == 0)) {
            headers = row;
            std::set<QString> seen;
            for (QString& header : headers) {
                header = header.toUpper();
                if (header.isEmpty() || !seen.insert(header).second) return makeError(ErrorCode::FormatInvalid, "Empty or duplicate CSV column.");
            }
            continue;
        }
        if (headers.isEmpty()) continue;
        if (row.size() != headers.size()) return makeError(ErrorCode::FormatInvalid, "CSV column count differs at Box " + first.toStdString());
        repeatability::BoxKey key{component, first.toStdString()};
        if (run.boxes.contains(key)) return makeError(ErrorCode::FormatInvalid, "Duplicate Component/Box: " + component + "/" + key.box);
        repeatability::Values values;
        for (qsizetype i = 1; i < headers.size(); ++i) {
            if (headers[i] == QStringLiteral("DECISION")) continue;
            const std::string id = headers[i].toStdString();
            if (row[i].isEmpty()) values[id] = {};
            else {
                bool ok = false;
                const double v = row[i].toDouble(&ok);
                if (!ok || !std::isfinite(v)) return makeError(ErrorCode::FormatInvalid, "Invalid numeric value at " + key.box + "/" + id);
                values[id] = v;
            }
        }
        if (values.empty()) return makeError(ErrorCode::FormatInvalid, "BOX LEVEL has no numeric metric columns.");
        run.boxes.emplace(std::move(key), std::move(values));
    }
    if (run.boxes.empty() || run.unit.empty()) return makeError(ErrorCode::FormatInvalid, "Missing BOX LEVEL data or Unit header: " + source);
    return run;
}

Result<repeatability::Dataset> CsvRepeatabilitySource::loadFolder(const std::string& path, const CancelToken& cancel) noexcept {
    try {
        QDir dir(QString::fromStdString(path));
        if (!dir.exists()) return makeError(ErrorCode::NotFound, "Folder does not exist: " + path);
        QFileInfoList files;
        for (const QFileInfo& f : dir.entryInfoList(QDir::Files))
            if (f.suffix().compare(QStringLiteral("csv"), Qt::CaseInsensitive) == 0) files.push_back(f);
        QCollator order;
        order.setNumericMode(true);
        std::sort(files.begin(), files.end(), [&](const QFileInfo& a, const QFileInfo& b) { return order.compare(a.fileName(), b.fileName()) < 0; });
        if (files.isEmpty()) return makeError(ErrorCode::NotFound, "No CSV files in folder: " + path);
        repeatability::Dataset out{dir.dirName().toStdString(), dir.canonicalPath().toStdString(), {}};
        for (const QFileInfo& info : files) {
            if (cancel.cancelled()) return makeError(ErrorCode::Cancelled, "Folder loading cancelled.");
            QFile file(info.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly)) return makeError(ErrorCode::IoFailed, file.errorString().toStdString());
            auto run = parseInspectionCsv(file.readAll(), info.fileName().toStdString(), cancel);
            if (!run) return makeError(run.error().code, info.fileName().toStdString() + ": " + run.error().message);
            out.runs.push_back(std::move(run.value()));
        }
        return out;
    } catch (const std::exception& e) { return makeError(ErrorCode::IoFailed, e.what()); }
    catch (...) { return makeError(ErrorCode::Internal, "Unexpected CSV read failure."); }
}
}  // namespace aoi::infra
