#include "application/backend/BackendSwitch.h"

#include <algorithm>
#include <utility>

namespace aoi {

BackendSwitch::BackendSwitch(std::vector<BackendInfo> backends, std::string current)
    : backends_(std::move(backends)), current_(std::move(current)) {}

const BackendInfo* BackendSwitch::find(std::string_view id) const {
    const auto it = std::find_if(backends_.begin(), backends_.end(), [&](const BackendInfo& b) { return b.id == id; });
    return it == backends_.end() ? nullptr : &*it;
}

BackendSwitch::RequestResult BackendSwitch::request(std::string_view id) {
    RequestResult result;
    if (switching()) {
        result.kind = Request::Busy;
        return result;
    }
    if (id == current_) {
        result.kind = Request::AlreadyCurrent;
        return result;
    }
    const BackendInfo* info = find(id);
    if (info == nullptr) {
        result.kind = Request::Unknown;
        result.error = makeError(ErrorCode::NotFound, "There is no back end with the id '" + std::string(id) + "'.");
        return result;
    }
    if (!info->available) {
        result.kind = Request::Unavailable;
        result.error = info->unavailable;
        return result;
    }
    pending_ = std::string(id);
    result.kind = Request::Start;
    return result;
}

BackendSwitch::Outcome BackendSwitch::finish(std::string_view id, bool succeeded) {
    if (!switching() || id != pending_) return Outcome::Stale;
    pending_.clear();
    if (!succeeded) return Outcome::Failed;
    current_ = std::string(id);
    return Outcome::Switched;
}

bool BackendSwitch::researchAfter(Outcome outcome, InteractionState state) {
    switch (outcome) {
        case Outcome::Switched:
            return state == InteractionState::Searching || state == InteractionState::Preview ||
                   state == InteractionState::NoMatch;
        case Outcome::Failed: return state == InteractionState::Searching;
        case Outcome::Stale: return false;
    }
    return false;
}

std::string BackendSwitch::fallback() const {
    for (const BackendInfo& info : backends_) {
        if (info.available && info.id != current_) return info.id;
    }
    return {};
}

StartupPlan planStartupBackend(const std::optional<std::string>& commandLine, const std::optional<std::string>& remembered,
                               std::string_view defaultId) {
    StartupPlan plan;
    if (commandLine) {
        plan.initial = *commandLine;
        plan.strict = true;
        return plan;
    }
    plan.initial = std::string(defaultId);
    if (remembered && !remembered->empty() && *remembered != defaultId) plan.switchTo = *remembered;
    return plan;
}

}  // namespace aoi
