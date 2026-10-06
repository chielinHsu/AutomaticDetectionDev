#include "infrastructure/storage/JsonFile.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QString>
#include <cmath>
#include <string>

namespace aoi::infra::json {
namespace {

QString qpath(std::string_view path) { return QString::fromUtf8(path.data(), static_cast<qsizetype>(path.size())); }

std::string quoted(std::string_view path) { return "'" + std::string(path) + "'"; }

std::string text(const QString& s) { return s.toUtf8().toStdString(); }

}  // namespace

Result<QJsonObject> readDocument(std::string_view path, std::string_view format) {
    const QString file = qpath(path);
    const QFileInfo info(file);
    if (file.isEmpty() || !info.exists()) return makeError(ErrorCode::NotFound, "The file does not exist: " + quoted(path) + ".");
    if (!info.isFile()) return makeError(ErrorCode::IoFailed, "The path is not a file: " + quoted(path) + ".");
    if (info.size() > kMaxFileBytes) return makeError(ErrorCode::FormatInvalid, "The file is too large: " + quoted(path) + ".");

    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        return makeError(ErrorCode::IoFailed, "Cannot open " + quoted(path) + ": " + text(f.errorString()) + ".");
    }
    QByteArray bytes = f.readAll();
    if (f.error() != QFileDevice::NoError) {
        return makeError(ErrorCode::IoFailed, "Cannot read " + quoted(path) + ": " + text(f.errorString()) + ".");
    }
    if (bytes.startsWith("\xEF\xBB\xBF")) bytes.remove(0, 3);  // Qt 的 fromJson 本身也接受（S06 的變異 M24 存活）：多一層保險

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return makeError(ErrorCode::FormatInvalid, quoted(path) + " is not valid JSON at byte " + std::to_string(parseError.offset) +
                                                       ": " + text(parseError.errorString()) + ".");
    }
    if (!doc.isObject()) return makeError(ErrorCode::FormatInvalid, quoted(path) + " does not hold a JSON object.");
    QJsonObject root = doc.object();
    const QJsonValue tag = root.value(QStringLiteral("format"));
    if (!tag.isString() || text(tag.toString()) != format) {
        return makeError(ErrorCode::FormatInvalid, quoted(path) + " is not an '" + std::string(format) + "' file.");
    }
    return root;
}

Result<void> writeDocument(std::string_view path, const QJsonObject& root) {
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    QSaveFile f(qpath(path));
    if (!f.open(QIODevice::WriteOnly)) {
        return makeError(ErrorCode::IoFailed, "Cannot write " + quoted(path) + ": " + text(f.errorString()) + ".");
    }
    if (f.write(bytes) != bytes.size()) {
        const std::string why = text(f.errorString());
        f.cancelWriting();
        return makeError(ErrorCode::IoFailed, "Cannot write " + quoted(path) + ": " + why + ".");
    }
    if (!f.commit()) return makeError(ErrorCode::IoFailed, "Cannot replace " + quoted(path) + ": " + text(f.errorString()) + ".");
    return {};
}

Result<int> migrate(QJsonObject& root, int current, const std::vector<Migration>& migrations) {
    const QJsonValue v = root.value(QStringLiteral("schemaVersion"));
    const double d = v.isDouble() ? v.toDouble() : 0.0;
    if (!v.isDouble() || !std::isfinite(d) || std::floor(d) != d || d < 1.0 || d > 1e6) {
        return makeError(ErrorCode::FormatInvalid, "schemaVersion must be a positive whole number.");
    }
    const int original = static_cast<int>(d);
    if (original > current) {
        return makeError(ErrorCode::FormatInvalid, "The file uses schema version " + std::to_string(original) +
                                                       ", but this program reads up to version " + std::to_string(current) +
                                                       ". The file comes from a newer program.");
    }
    for (int k = original; k < current; ++k) {
        const std::size_t index = static_cast<std::size_t>(k - 1);
        if (index >= migrations.size() || !migrations[index]) {
            return makeError(ErrorCode::FormatInvalid, "No migration from schema version " + std::to_string(k) + ".");
        }
        const Result<void> step = migrations[index](root);
        if (!step) {
            return makeError(ErrorCode::FormatInvalid,
                             "The migration from schema version " + std::to_string(k) + " failed: " + step.error().message);
        }
        root.insert(QStringLiteral("schemaVersion"), k + 1);
    }
    return original;
}

}  // namespace aoi::infra::json
