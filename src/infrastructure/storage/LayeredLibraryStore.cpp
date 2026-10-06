#include "infrastructure/storage/LayeredLibraryStore.h"

#include <QString>
#include <set>
#include <utility>

namespace aoi::infra {
namespace {

std::string keyOf(std::string_view s) {  // 與 JsonLibraryStore 相同的正規化：去掉前後空白、Unicode 簡單折疊
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size())).trimmed().toCaseFolded().toUtf8().toStdString();
}

}  // namespace

LayeredLibraryStore::LayeredLibraryStore(std::vector<LibraryLayer> layers, std::size_t defaultLayer) : defaultLayer_(defaultLayer) {
    for (LibraryLayer& l : layers) {
        Layer layer;
        layer.store = std::make_unique<JsonLibraryStore>(l.path, l.idPrefix);
        layer.config = std::move(l);
        layers_.push_back(std::move(layer));
    }
}

std::unique_ptr<LayeredLibraryStore> LayeredLibraryStore::standard(std::string projectPath, std::string userPath, std::string systemPath) {
    std::vector<LibraryLayer> layers;
    if (!projectPath.empty()) layers.push_back({"project", std::move(projectPath), "proj-", true});
    const std::size_t user = layers.size();
    layers.push_back({"user", std::move(userPath), "user-", true});
    if (!systemPath.empty()) layers.push_back({"system", std::move(systemPath), "sys-", false});
    return std::make_unique<LayeredLibraryStore>(std::move(layers), user);
}

Error LayeredLibraryStore::withLayer(const Layer& layer, Error e) {
    e.message = "The " + layer.config.name + " library: " + e.message;
    return e;
}

Result<std::optional<std::size_t>> LayeredLibraryStore::layerIndexOf(std::string_view libId) {
    for (std::size_t i = 0; i < layers_.size(); ++i) {
        const Result<bool> has = layers_[i].store->hasEntry(libId);
        if (!has) return withLayer(layers_[i], has.error());
        if (has.value()) return std::optional<std::size_t>(i);
    }
    return std::optional<std::size_t>();
}

Result<std::vector<ComponentType>> LayeredLibraryStore::loadAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ComponentType> all;
    std::set<std::string> seen;  // 上層已經有的料號
    for (const Layer& layer : layers_) {
        Result<std::vector<ComponentType>> entries = layer.store->loadAll();
        if (!entries) return withLayer(layer, entries.error());
        for (ComponentType& t : entries.value()) {
            if (!seen.insert(keyOf(t.partNumber)).second) continue;  // 被上層遮住
            all.push_back(std::move(t));
        }
    }
    return all;
}

Result<std::optional<ComponentType>> LayeredLibraryStore::find(std::string_view partNumberOrAlias) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const Layer& layer : layers_) {
        Result<std::optional<ComponentType>> r = layer.store->find(partNumberOrAlias);
        if (!r) return withLayer(layer, r.error());
        if (r.value()) return r;
    }
    return std::optional<ComponentType>();
}

Result<void> LayeredLibraryStore::save(const ComponentType& type) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (layers_.empty() || defaultLayer_ >= layers_.size() || !layers_[defaultLayer_].config.writable) {
        return makeError(ErrorCode::InvalidArgument, "The library has no writable default layer.");
    }
    if (keyOf(type.partNumber).empty()) return makeError(ErrorCode::InvalidArgument, "A library entry needs a part number.");

    // 1. 已經有這個料號的最高可寫層
    std::optional<std::size_t> target;
    for (std::size_t i = 0; i < layers_.size() && !target; ++i) {
        const Result<std::optional<ComponentType>> part = layers_[i].store->findPart(type.partNumber);
        if (!part) return withLayer(layers_[i], part.error());
        if (part.value() && layers_[i].config.writable) target = i;
    }
    // 2. libId 指到的那一層（可寫的話）
    if (!target && !type.libId.empty()) {
        const Result<std::optional<std::size_t>> source = layerIndexOf(type.libId);
        if (!source) return source.error();
        if (source.value() && layers_[*source.value()].config.writable) target = source.value();
    }
    // 3. 預設層
    const std::size_t index = target.value_or(defaultLayer_);
    const Result<void> r = layers_[index].store->save(type);
    if (!r) return withLayer(layers_[index], r.error());
    return {};
}

Result<void> LayeredLibraryStore::remove(std::string_view libId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const Result<std::optional<std::size_t>> where = layerIndexOf(libId);
    if (!where) return where.error();
    if (!where.value()) return makeError(ErrorCode::NotFound, "The library has no entry '" + std::string(libId) + "'.");
    const Layer& layer = layers_[*where.value()];
    if (!layer.config.writable) {
        return makeError(ErrorCode::InvalidArgument, "The entry '" + std::string(libId) + "' is in the read-only " + layer.config.name + " library.");
    }
    const Result<void> r = layer.store->remove(libId);
    if (!r) return withLayer(layer, r.error());
    return {};
}

Result<std::optional<std::string>> LayeredLibraryStore::layerOf(std::string_view libId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const Result<std::optional<std::size_t>> where = layerIndexOf(libId);
    if (!where) return where.error();
    if (!where.value()) return std::optional<std::string>();
    return std::optional<std::string>(layers_[*where.value()].config.name);
}

}  // namespace aoi::infra
