#include "infrastructure/storage/CsvRepeatabilitySource.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cmath>
#include <stdexcept>

namespace aoi::infra {
namespace {
using namespace repeatability;
QString q(const std::string& s) { return QString::fromStdString(s); }
void require(bool condition) { if (!condition) throw std::invalid_argument("Invalid repeatability session structure or value."); }
double number(const QJsonValue& v) { require(v.isDouble() && std::isfinite(v.toDouble())); return v.toDouble(); }
std::string string(const QJsonValue& v) { require(v.isString()); return v.toString().toStdString(); }
QJsonValue optional(std::optional<double> v) { return v ? QJsonValue(*v) : QJsonValue(QJsonValue::Null); }
Result<void> write(const QString& path, const QByteArray& bytes) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return makeError(ErrorCode::IoFailed, file.errorString().toStdString());
    return {};
}
QString quoted(QString s) {
    if (!s.isEmpty() && QStringLiteral("=+-@").contains(s.front())) s.prepend(QLatin1Char('\''));
    s.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + s + QLatin1Char('"');
}
QString csvNumber(std::optional<double> v) { return v ? QString::number(*v, 'g', 17) : QString(); }
}

Result<void> CsvRepeatabilitySource::writeSession(const std::string& path, const AnalysisSession& session) noexcept {
    try {
        if (session.datasets.empty() || session.datasets.size() > 4) return makeError(ErrorCode::InvalidArgument, "Invalid folder count.");
        QJsonArray definitions, datasets;
        for (const auto& [id, m] : session.metrics)
            definitions.append(QJsonObject{{"id", q(id)}, {"unit", q(m.unit)}, {"multiplier", m.multiplier},
                                           {"tolerance", optional(m.tolerance)}, {"ptLimit", m.ptLimit}});
        for (const Dataset& d : session.datasets) {
            QJsonArray runs;
            for (const MeasurementRun& r : d.runs) {
                QJsonArray boxes;
                for (const auto& [key, values] : r.boxes) {
                    QJsonObject fields;
                    for (const auto& [id, v] : values) fields.insert(q(id), optional(v));
                    boxes.append(QJsonObject{{"component", q(key.component)}, {"box", q(key.box)}, {"values", fields}});
                }
                runs.append(QJsonObject{{"source", q(r.source)}, {"timestamp", q(r.timestamp)}, {"unit", q(r.unit)}, {"boxes", boxes}});
            }
            datasets.append(QJsonObject{{"name", q(d.name)}, {"folder", q(d.folder)}, {"runs", runs}});
        }
        return write(q(path), QJsonDocument(QJsonObject{{"schema", 1}, {"metrics", definitions}, {"datasets", datasets}}).toJson());
    } catch (const std::exception& e) { return makeError(ErrorCode::IoFailed, e.what()); }
    catch (...) { return makeError(ErrorCode::Internal, "Session write failed."); }
}

Result<AnalysisSession> CsvRepeatabilitySource::readSession(const std::string& path) noexcept {
    try {
        QFile file(q(path));
        if (!file.open(QIODevice::ReadOnly)) return makeError(ErrorCode::IoFailed, file.errorString().toStdString());
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        require(error.error == QJsonParseError::NoError && doc.isObject());
        const QJsonObject root = doc.object();
        require(root.value("schema").toInt(-1) == 1 && root.value("metrics").isArray() && root.value("datasets").isArray());
        AnalysisSession out;
        out.metrics.clear();
        for (const QJsonValue& v : root.value("metrics").toArray()) {
            require(v.isObject()); const QJsonObject o = v.toObject();
            MetricDefinition m;
            m.id = string(o.value("id")); m.unit = string(o.value("unit"));
            m.multiplier = number(o.value("multiplier")); m.ptLimit = number(o.value("ptLimit"));
            if (!o.value("tolerance").isNull()) m.tolerance = number(o.value("tolerance"));
            require(!m.id.empty() && !m.unit.empty() && m.multiplier > 0 && m.ptLimit > 0 && (!m.tolerance || *m.tolerance > 0));
            require(out.metrics.emplace(m.id, m).second);
        }
        for (const QJsonValue& v : root.value("datasets").toArray()) {
            require(v.isObject()); const QJsonObject o = v.toObject();
            Dataset d{string(o.value("name")), string(o.value("folder")), {}};
            require(o.value("runs").isArray());
            for (const QJsonValue& rv : o.value("runs").toArray()) {
                require(rv.isObject()); const QJsonObject ro = rv.toObject();
                MeasurementRun r;
                r.source = string(ro.value("source")); r.timestamp = string(ro.value("timestamp")); r.unit = string(ro.value("unit"));
                require(!r.unit.empty() && ro.value("boxes").isArray());
                for (const QJsonValue& bv : ro.value("boxes").toArray()) {
                    require(bv.isObject()); const QJsonObject bo = bv.toObject();
                    BoxKey key{string(bo.value("component")), string(bo.value("box"))};
                    require(!key.box.empty() && bo.value("values").isObject());
                    Values values;
                    const QJsonObject vo = bo.value("values").toObject();
                    for (auto it = vo.begin(); it != vo.end(); ++it) {
                        require(!it.key().isEmpty() && out.metrics.contains(it.key().toStdString()));
                        values.emplace(it.key().toStdString(), it.value().isNull() ? std::optional<double>() : number(it.value()));
                    }
                    require(!values.empty() && r.boxes.emplace(key, std::move(values)).second);
                }
                require(!r.boxes.empty()); d.runs.push_back(std::move(r));
            }
            require(!d.runs.empty()); out.datasets.push_back(std::move(d));
        }
        require(!out.datasets.empty() && out.datasets.size() <= 4);
        return out;
    } catch (const std::exception& e) { return makeError(ErrorCode::FormatInvalid, e.what()); }
    catch (...) { return makeError(ErrorCode::Internal, "Session read failed."); }
}

Result<void> CsvRepeatabilitySource::exportTables(const std::string& path, const AnalysisSession& session,
                                               const std::vector<std::string>& metrics, Statistic statistic,
                                               std::size_t baseline) noexcept {
    try {
        if (session.datasets.empty() || baseline >= session.datasets.size()) return makeError(ErrorCode::InvalidArgument, "Invalid export baseline.");
        std::vector<DatasetAnalysis> analyses;
        for (const Dataset& d : session.datasets) {
            auto result = analyze(d, session.metrics);
            if (!result) return result.error();
            analyses.push_back(std::move(result.value()));
        }
        QString summary = QStringLiteral("Metric,Group,Unit,CommonCompleteBoxes,Value,DeltaFromBaseline,Baseline\r\n");
        QString details = QStringLiteral("Group,Component,Box,Metric,Unit,n,Expected,Complete,Min,Max,Avg,Range,STDEV,PT_ratio\r\n");
        for (const std::string& id : metrics) {
            if (!session.metrics.contains(id)) continue;
            const MetricDefinition& def = session.metrics.at(id);
            const Comparison c = compare(analyses, id, statistic);
            for (std::size_t g = 0; g < analyses.size(); ++g) {
                const auto delta = c.values[g] && c.values[baseline] ? std::optional<double>(*c.values[g] - *c.values[baseline]) : std::nullopt;
                summary += QStringList{quoted(q(id)), quoted(q(session.datasets[g].name)), quoted(statistic == Statistic::Pt ? QStringLiteral("ratio") : q(def.unit)),
                    QString::number(c.commonBoxes), csvNumber(c.values[g]), csvNumber(delta), quoted(q(session.datasets[baseline].name))}.join(',') + QStringLiteral("\r\n");
                const auto found = analyses[g].metrics.find(id);
                if (found == analyses[g].metrics.end()) continue;
                for (const auto& [key, s] : found->second)
                    details += QStringList{quoted(q(session.datasets[g].name)), quoted(q(key.component)), quoted(q(key.box)), quoted(q(id)), quoted(q(def.unit)),
                        QString::number(s.count), QString::number(s.expected), s.complete() ? QStringLiteral("true") : QStringLiteral("false"),
                        csvNumber(s.minimum), csvNumber(s.maximum), csvNumber(s.average), csvNumber(s.range), csvNumber(s.deviation), csvNumber(s.pt)}.join(',') + QStringLiteral("\r\n");
            }
        }
        QDir dir(q(path));
        if (!dir.exists()) return makeError(ErrorCode::NotFound, "Export folder does not exist.");
        auto result = write(dir.filePath(QStringLiteral("summary.csv")), QByteArray::fromHex("efbbbf") + summary.toUtf8());
        if (!result) return result;
        return write(dir.filePath(QStringLiteral("details.csv")), QByteArray::fromHex("efbbbf") + details.toUtf8());
    } catch (const std::exception& e) { return makeError(ErrorCode::IoFailed, e.what()); }
    catch (...) { return makeError(ErrorCode::Internal, "Export failed."); }
}
}  // namespace aoi::infra
