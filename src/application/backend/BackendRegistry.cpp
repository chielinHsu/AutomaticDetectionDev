#include "application/backend/BackendRegistry.h"

#include <algorithm>
#include <utility>

namespace aoi {

namespace {

Error alreadyRegistered(const std::string& id) {
    return makeError(ErrorCode::InvalidArgument, "The back end '" + id + "' is already registered.");
}

}  // namespace

Result<void> BackendRegistry::registerBackend(std::string id, std::string displayName, Factory factory) {
    if (id.empty() || !factory) return makeError(ErrorCode::InvalidArgument, "A back end needs an id and a factory.");
    const bool duplicate = std::any_of(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.info.id == id; });
    if (duplicate) return alreadyRegistered(id);
    Entry entry;
    entry.info.id = std::move(id);
    entry.info.displayName = std::move(displayName);
    entry.info.available = true;
    entry.factory = std::move(factory);
    entries_.push_back(std::move(entry));
    return Result<void>::success();
}

Result<void> BackendRegistry::registerUnavailable(std::string id, std::string displayName, std::string reason) {
    if (id.empty()) return makeError(ErrorCode::InvalidArgument, "A back end needs an id.");
    const bool duplicate = std::any_of(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.info.id == id; });
    if (duplicate) return alreadyRegistered(id);
    Entry entry;
    entry.info.id = std::move(id);
    entry.info.displayName = std::move(displayName);
    entry.info.available = false;
    entry.info.unavailable = makeError(ErrorCode::BackendUnavailable, std::move(reason));
    entries_.push_back(std::move(entry));
    return Result<void>::success();
}

std::vector<BackendInfo> BackendRegistry::list() const {
    std::vector<BackendInfo> infos;
    infos.reserve(entries_.size());
    for (const Entry& e : entries_) infos.push_back(e.info);
    return infos;
}

Result<Backend> BackendRegistry::create(std::string_view id) const {
    const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.info.id == id; });
    if (it == entries_.end()) {
        return makeError(ErrorCode::NotFound, "There is no back end with the id '" + std::string(id) + "'.");
    }
    if (!it->info.available) return it->info.unavailable;

    Result<Backend> created = it->factory();
    if (!created) return created.error();
    if (!created.value().templateBuilder || !created.value().searcher) {
        return makeError(ErrorCode::Internal, "The factory of back end '" + std::string(id) + "' returned an incomplete back end.");
    }
    return created;
}

}  // namespace aoi
