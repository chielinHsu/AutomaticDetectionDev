#include "application/document/ProposalSession.h"

#include <algorithm>

#include "application/usecases/ProposalView.h"

namespace aoi {

namespace {

// 把 memento 的「前」「後」兩份修改包成 Command。redo 套「後」，undo 套「前」。
class EditsCommand final : public Command<ProposalState> {
public:
    EditsCommand(std::shared_ptr<const ProposalEdits> before, std::shared_ptr<const ProposalEdits> after,
                 CommandKind kind, std::size_t count, std::vector<int> movedIds = {})
        : before_(std::move(before)), after_(std::move(after)), kind_(kind), count_(count), movedIds_(std::move(movedIds)) {}

    void redo(ProposalState& target) const override { target.edits = after_; }
    void undo(ProposalState& target) const override { target.edits = before_; }
    CommandKind kind() const override { return kind_; }
    std::size_t count() const override { return count_; }

    // 兩步都是對「同一組」物件的移動，才併成一步。
    std::shared_ptr<const Command<ProposalState>> mergeWith(const Command<ProposalState>& next) const override {
        if (kind_ != CommandKind::MoveComponents || next.kind() != CommandKind::MoveComponents) return nullptr;
        const auto* other = dynamic_cast<const EditsCommand*>(&next);
        if (other == nullptr || other->movedIds_ != movedIds_) return nullptr;
        return std::make_shared<EditsCommand>(before_, other->after_, kind_, count_, movedIds_);
    }

private:
    std::shared_ptr<const ProposalEdits> before_;
    std::shared_ptr<const ProposalEdits> after_;
    CommandKind kind_;
    std::size_t count_;
    std::vector<int> movedIds_;  // 排序過的
};

bool isDeleted(const ProposalEdits& edits, int id) {
    return std::binary_search(edits.deleted.begin(), edits.deleted.end(), id);
}

const ProposalEdits::Manual* findManual(const ProposalEdits& edits, int id) {
    for (const ProposalEdits::Manual& m : edits.manual) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

}  // namespace

ProposalSession::ProposalSession(std::shared_ptr<const BoardProposal> proposal, Rect region, RegionMode mode,
                                 double threshold, double candidateFloor)
    : proposal_(std::move(proposal)), region_(region), mode_(mode), threshold_(threshold), candidateFloor_(candidateFloor) {
    auto edits = std::make_shared<ProposalEdits>();
    // classify 把候選依順序編成 1..N；手動補的接在後面，所以編號不隨門檻或修改改變。
    edits->nextManualId = static_cast<int>(proposal_->candidates.size()) + 1;
    state_.edits = std::move(edits);
}

ProposalObjects ProposalSession::objects() const {
    ProposalView base = classify(*proposal_, threshold_, candidateFloor_);
    const ProposalEdits& edits = *state_.edits;

    ProposalObjects out;
    out.noMatch = base.noMatch;
    out.objects.reserve(base.objects.size() + edits.manual.size());

    auto moved = edits.moved.begin();
    for (InspectionObject& object : base.objects) {
        if (isDeleted(edits, object.id)) continue;
        while (moved != edits.moved.end() && moved->first < object.id) ++moved;
        if (moved != edits.moved.end() && moved->first == object.id) {
            object.rect.pose.x += moved->second.x;
            object.rect.pose.y += moved->second.y;
        }
        switch (object.state) {
            case ObjectState::Sample: ++out.sample; break;
            case ObjectState::Found: ++out.found; break;
            case ObjectState::Inferred: ++out.inferred; break;
            case ObjectState::Rejected: ++out.rejected; break;
        }
        out.objects.push_back(std::move(object));
    }
    for (const ProposalEdits::Manual& m : edits.manual) {
        InspectionObject object;
        object.id = m.id;
        object.rect = RectObj{m.pose, m.partSize.w, m.partSize.h};
        object.state = ObjectState::Found;
        object.origin = ObjectOrigin::Manual;
        object.typeId = m.typeId;
        out.objects.push_back(object);
        ++out.found;
        ++out.manual;
    }
    return out;
}

bool ProposalSession::contains(int objectId) const {
    const ProposalEdits& edits = *state_.edits;
    if (objectId >= 1 && objectId <= static_cast<int>(proposal_->candidates.size())) return !isDeleted(edits, objectId);
    return findManual(edits, objectId) != nullptr;
}

std::optional<ProposalSession::TypeAndSize> ProposalSession::typeOf(int objectId) const {
    if (!contains(objectId)) return std::nullopt;
    if (objectId >= 1 && objectId <= static_cast<int>(proposal_->candidates.size())) {
        const BoardCandidate& c = proposal_->candidates[static_cast<std::size_t>(objectId - 1)];
        return TypeAndSize{c.typeId, c.partSize};
    }
    const ProposalEdits::Manual* m = findManual(*state_.edits, objectId);
    return TypeAndSize{m->typeId, m->partSize};
}

std::optional<Pose2D> ProposalSession::poseOf(int objectId) const {
    if (!contains(objectId)) return std::nullopt;
    const ProposalEdits& edits = *state_.edits;
    if (objectId >= 1 && objectId <= static_cast<int>(proposal_->candidates.size())) {
        Pose2D pose = proposal_->candidates[static_cast<std::size_t>(objectId - 1)].pose;
        const auto it = std::lower_bound(edits.moved.begin(), edits.moved.end(), objectId,
                                         [](const auto& entry, int key) { return entry.first < key; });
        if (it != edits.moved.end() && it->first == objectId) {
            pose.x += it->second.x;
            pose.y += it->second.y;
        }
        return pose;
    }
    const ProposalEdits::Manual* m = findManual(edits, objectId);
    return m->pose;
}

std::size_t ProposalSession::deleteObjects(const std::vector<int>& ids) {
    const ProposalEdits& current = *state_.edits;
    std::vector<int> fresh;  // 真的要刪的（存在、還沒刪、不重複）
    for (int id : ids) {
        if (contains(id) && std::find(fresh.begin(), fresh.end(), id) == fresh.end()) fresh.push_back(id);
    }
    if (fresh.empty()) return 0;

    auto next = std::make_shared<ProposalEdits>(current);
    for (int id : fresh) {
        if (id <= static_cast<int>(proposal_->candidates.size())) {
            next->deleted.insert(std::upper_bound(next->deleted.begin(), next->deleted.end(), id), id);
            next->moved.erase(std::remove_if(next->moved.begin(), next->moved.end(),
                                             [id](const auto& m) { return m.first == id; }),
                              next->moved.end());
        } else {
            next->manual.erase(std::remove_if(next->manual.begin(), next->manual.end(),
                                              [id](const ProposalEdits::Manual& m) { return m.id == id; }),
                               next->manual.end());
        }
    }
    history_.push(state_, std::make_shared<EditsCommand>(state_.edits, next, CommandKind::DeleteComponents, fresh.size()));
    return fresh.size();
}

int ProposalSession::addManual(const Pose2D& pose, std::string typeId, Size2D partSize) {
    auto next = std::make_shared<ProposalEdits>(*state_.edits);
    const int id = next->nextManualId++;
    next->manual.push_back({id, pose, std::move(typeId), partSize});
    history_.push(state_, std::make_shared<EditsCommand>(state_.edits, next, CommandKind::AddComponent, 1));
    return id;
}

std::size_t ProposalSession::moveObjects(const std::vector<int>& ids, Point2D delta, bool allowMerge) {
    std::vector<int> valid;
    for (int id : ids) {
        if (contains(id) && std::find(valid.begin(), valid.end(), id) == valid.end()) valid.push_back(id);
    }
    if (valid.empty()) return 0;
    std::sort(valid.begin(), valid.end());

    auto next = std::make_shared<ProposalEdits>(*state_.edits);
    for (int id : valid) {
        if (id > static_cast<int>(proposal_->candidates.size())) {
            for (ProposalEdits::Manual& m : next->manual) {
                if (m.id == id) {
                    m.pose.x += delta.x;
                    m.pose.y += delta.y;
                }
            }
            continue;
        }
        auto it = std::lower_bound(next->moved.begin(), next->moved.end(), id,
                                   [](const auto& entry, int key) { return entry.first < key; });
        if (it != next->moved.end() && it->first == id) {
            it->second.x += delta.x;
            it->second.y += delta.y;
        } else {
            next->moved.insert(it, {id, delta});
        }
    }
    history_.push(state_,
                  std::make_shared<EditsCommand>(state_.edits, next, CommandKind::MoveComponents, valid.size(), valid),
                  allowMerge);
    return valid.size();
}

bool ProposalSession::undo() { return history_.undo(state_); }
bool ProposalSession::redo() { return history_.redo(state_); }

std::optional<CommandInfo> ProposalSession::undoInfo() const {
    const auto* c = history_.nextUndo();
    if (c == nullptr) return std::nullopt;
    return CommandInfo{c->kind(), c->count()};
}

std::optional<CommandInfo> ProposalSession::redoInfo() const {
    const auto* c = history_.nextRedo();
    if (c == nullptr) return std::nullopt;
    return CommandInfo{c->kind(), c->count()};
}

}  // namespace aoi
