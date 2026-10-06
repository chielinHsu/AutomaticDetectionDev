#include "infrastructure/storage/JsonLibraryStore.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <new>
#include <utility>

#include "infrastructure/storage/JsonCodec.h"
#include "infrastructure/storage/JsonFile.h"

namespace aoi::infra {
namespace {

// 比對用的鍵：去掉前後空白、Unicode 不分大小寫（case folding）。
std::string keyOf(std::string_view s) {
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size())).trimmed().toCaseFolded().toUtf8().toStdString();
}

constexpr int kMaxEntryNumber = 2000000000;

}  // namespace

JsonLibraryStore::JsonLibraryStore(std::string path, std::string idPrefix) : path_(std::move(path)), idPrefix_(std::move(idPrefix)) {}

JsonLibraryStore::Entry JsonLibraryStore::makeEntry(ComponentType type) {
    Entry e;
    e.partKey = keyOf(type.partNumber);
    for (const std::string& a : type.aliases) e.aliasKeys.push_back(keyOf(a));
    e.type = std::move(type);
    return e;
}

Result<void> JsonLibraryStore::ensureLoaded() {
    if (loaded_) return {};
    Result<QJsonObject> root = json::readDocument(path_, kFormat);
    if (!root) {
        if (root.error().code != ErrorCode::NotFound) return root.error();
        entries_.clear();  // 還沒有檔案：空的庫
        nextEntryNumber_ = 1;
        loaded_ = true;
        return {};
    }
    QJsonObject& r = root.value();
    const Result<int> version = json::migrate(r, kSchemaVersion, {});
    if (!version) return version.error();

    const QJsonValue next = r.value(QStringLiteral("nextEntryNumber"));
    const double n = next.isDouble() ? next.toDouble() : 0.0;
    if (!next.isDouble() || n != static_cast<double>(static_cast<int>(n)) || n < 1 || n > kMaxEntryNumber) {
        return makeError(ErrorCode::FormatInvalid, "nextEntryNumber must be a positive whole number.");
    }
    const QJsonValue list = r.value(QStringLiteral("entries"));
    if (!list.isArray()) return makeError(ErrorCode::FormatInvalid, "entries must be an array.");

    std::vector<Entry> entries;
    const QJsonArray array = list.toArray();
    for (qsizetype i = 0; i < array.size(); ++i) {
        Result<ComponentType> t = json::decodeComponentType(array.at(i), "entries[" + std::to_string(i) + "]");
        if (!t) return t.error();
        for (const Entry& e : entries) {
            if (e.type.id == t.value().id) return makeError(ErrorCode::FormatInvalid, "entries has the id '" + e.type.id + "' twice.");
        }
        entries.push_back(makeEntry(std::move(t.value())));
    }
    entries_ = std::move(entries);
    nextEntryNumber_ = static_cast<int>(n);
    loaded_ = true;
    return {};
}

Result<void> JsonLibraryStore::write(const std::vector<Entry>& entries, int nextEntryNumber) const {
    QJsonArray array;
    for (const Entry& e : entries) {
        Result<QJsonObject> o = json::encodeComponentType(e.type);
        if (!o) return o.error();
        array.append(o.value());
    }
    QJsonObject root;
    root.insert(QStringLiteral("format"), QString::fromLatin1(kFormat));
    root.insert(QStringLiteral("schemaVersion"), kSchemaVersion);
    root.insert(QStringLiteral("nextEntryNumber"), nextEntryNumber);
    root.insert(QStringLiteral("entries"), array);
    return json::writeDocument(path_, root);
}

Result<std::vector<ComponentType>> JsonLibraryStore::loadAll() {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (Result<void> r = ensureLoaded(); !r) return r.error();
        std::vector<ComponentType> all;
        all.reserve(entries_.size());
        for (const Entry& e : entries_) all.push_back(e.type);
        return all;
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::IoFailed, "Not enough memory to load the library.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while loading the library.");
    }
}

Result<std::optional<ComponentType>> JsonLibraryStore::find(std::string_view partNumberOrAlias) {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (Result<void> r = ensureLoaded(); !r) return r.error();
        const std::string key = keyOf(partNumberOrAlias);
        if (key.empty()) return std::optional<ComponentType>();
        for (const Entry& e : entries_) {
            if (e.partKey == key) return std::optional<ComponentType>(e.type);
        }
        for (const Entry& e : entries_) {
            for (const std::string& a : e.aliasKeys) {
                if (a == key) return std::optional<ComponentType>(e.type);
            }
        }
        return std::optional<ComponentType>();
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::IoFailed, "Not enough memory to search the library.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while searching the library.");
    }
}

Result<std::optional<ComponentType>> JsonLibraryStore::findPart(std::string_view partNumber) {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (Result<void> r = ensureLoaded(); !r) return r.error();
        const std::string key = keyOf(partNumber);
        if (key.empty()) return std::optional<ComponentType>();
        for (const Entry& e : entries_) {
            if (e.partKey == key) return std::optional<ComponentType>(e.type);
        }
        return std::optional<ComponentType>();
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::IoFailed, "Not enough memory to search the library.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while searching the library.");
    }
}

Result<bool> JsonLibraryStore::hasEntry(std::string_view libId) {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (Result<void> r = ensureLoaded(); !r) return r.error();
        for (const Entry& e : entries_) {
            if (e.type.id == libId) return true;
        }
        return false;
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while searching the library.");
    }
}

Result<void> JsonLibraryStore::save(const ComponentType& type) {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (Result<void> r = ensureLoaded(); !r) return r;
        Entry entry = makeEntry(type);
        if (entry.partKey.empty()) return makeError(ErrorCode::InvalidArgument, "A library entry needs a part number.");
        entry.type.libId.clear();

        std::vector<Entry> entries = entries_;
        int next = nextEntryNumber_;
        bool replaced = false;
        for (Entry& e : entries) {
            if (e.partKey == entry.partKey) {
                entry.type.id = e.type.id;  // 同料號：覆寫內容，保留 id（板子複本的 libId 仍指得到）
                e = std::move(entry);
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            if (next >= kMaxEntryNumber) return makeError(ErrorCode::InvalidArgument, "The library has run out of entry numbers.");
            entry.type.id = idPrefix_ + std::to_string(next++);
            entries.push_back(std::move(entry));
        }
        if (Result<void> w = write(entries, next); !w) return w;  // 寫失敗：記憶體不變
        entries_ = std::move(entries);
        nextEntryNumber_ = next;
        return {};
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::IoFailed, "Not enough memory to save the library entry.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while saving the library entry.");
    }
}

Result<void> JsonLibraryStore::remove(std::string_view libId) {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        if (Result<void> r = ensureLoaded(); !r) return r;
        std::vector<Entry> entries;
        entries.reserve(entries_.size());
        bool found = false;
        for (const Entry& e : entries_) {
            if (!found && e.type.id == libId) found = true;
            else entries.push_back(e);
        }
        if (!found) return makeError(ErrorCode::NotFound, "The library has no entry '" + std::string(libId) + "'.");
        if (Result<void> w = write(entries, nextEntryNumber_); !w) return w;
        entries_ = std::move(entries);
        return {};
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::IoFailed, "Not enough memory to remove the library entry.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while removing the library entry.");
    }
}

}  // namespace aoi::infra
