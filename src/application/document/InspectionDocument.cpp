#include "application/document/InspectionDocument.h"

#include <algorithm>
#include <cstddef>
#include <iterator>

#include "domain/box_expander.h"
#include "domain/image_depth.h"

namespace aoi {

namespace detail {

// InspectionDocument 的私有成員只開放給這個檔案裡的 Command。
struct DocumentAccess {
    static Board& board(InspectionDocument& d) { return d.board_; }
    static InspectionDocument::Counters& counters(InspectionDocument& d) { return d.counters_; }
    static std::string& lastTypeId(InspectionDocument& d) { return d.lastTypeId_; }
};

}  // namespace detail

namespace {

using Access = detail::DocumentAccess;
using Counters = InspectionDocument::Counters;

// 文件堆疊上的 Command。「接受」與「捨棄」帶著提案，復原時還給預覽。
class DocumentCommand : public Command<InspectionDocument> {
public:
    virtual const ProposalSession* proposal() const { return nullptr; }
};

// 名稱開頭字元依類別：Chip → R、IC → U、其他 → X（ux-spec §12.1 第 8 項：開頭字元＋流水號，接受時指派）。
// **暫定**：樣本的類型沒有類別（Other），所以元件叫 X1、X2…；類型編輯器（流程 C）設定類別後才有 R、U。
const char* namePrefix(ComponentCategory category) {
    switch (category) {
        case ComponentCategory::Chip: return "R";
        case ComponentCategory::IC: return "U";
        case ComponentCategory::Other: return "X";
    }
    return "X";
}

std::string takeName(Counters& counters, ComponentCategory category) {
    const std::string prefix = namePrefix(category);
    return prefix + std::to_string(++counters.nameSeq[prefix]);
}

// 元件依編號排序，用編號找。
std::vector<ComponentInstance>::iterator findById(std::vector<ComponentInstance>& v, int id) {
    auto it = std::lower_bound(v.begin(), v.end(), id,
                               [](const ComponentInstance& c, int key) { return c.id < key; });
    return it != v.end() && it->id == id ? it : v.end();
}

ComponentType* findTypeMutable(Board& board, const std::string& typeId) {
    for (ComponentType& t : board.types) {
        if (t.id == typeId) return &t;
    }
    return nullptr;
}

BoxGroup* bodyOf(ComponentType& type) {
    for (BoxGroup& g : type.groups) {
        if (g.kind == BoxGroupKind::Body) return &g;
    }
    return nullptr;
}

bool byId(const ComponentInstance& a, const ComponentInstance& b) { return a.id < b.id; }

// removed 依編號排序：從 components 拿掉它們。
void eraseComponents(std::vector<ComponentInstance>& components, const std::vector<ComponentInstance>& removed) {
    if (removed.empty()) return;
    std::vector<ComponentInstance> kept;
    kept.reserve(components.size() - std::min(components.size(), removed.size()));
    auto gone = removed.begin();
    for (ComponentInstance& c : components) {
        while (gone != removed.end() && gone->id < c.id) ++gone;
        if (gone != removed.end() && gone->id == c.id) {
            ++gone;
            continue;
        }
        kept.push_back(std::move(c));
    }
    components = std::move(kept);
}

// 把 restored（依編號排序）放回去，保持排序。
void insertComponents(std::vector<ComponentInstance>& components, const std::vector<ComponentInstance>& restored) {
    if (restored.empty()) return;
    const auto middle = static_cast<std::ptrdiff_t>(components.size());
    components.insert(components.end(), restored.begin(), restored.end());
    std::inplace_merge(components.begin(), components.begin() + middle, components.end(), byId);
}

// 建立樣本：一個新類型與它的樣本元件。
class AddSampleCommand final : public DocumentCommand {
public:
    AddSampleCommand(ComponentType type, ComponentInstance sample, Counters before, Counters after, std::string lastTypeBefore)
        : type_(std::move(type)), sample_(std::move(sample)), before_(std::move(before)), after_(std::move(after)),
          lastTypeBefore_(std::move(lastTypeBefore)) {}

    void redo(InspectionDocument& d) const override {
        Board& board = Access::board(d);
        board.types.push_back(type_);
        board.components.push_back(sample_);  // 編號最大，接在最後面就保持排序
        Access::counters(d) = after_;
        Access::lastTypeId(d) = type_.id;
    }
    void undo(InspectionDocument& d) const override {
        Board& board = Access::board(d);
        board.components.pop_back();
        board.types.erase(std::remove_if(board.types.begin(), board.types.end(),
                                         [this](const ComponentType& t) { return t.id == type_.id; }),
                          board.types.end());
        Access::counters(d) = before_;
        Access::lastTypeId(d) = lastTypeBefore_;
    }
    CommandKind kind() const override { return CommandKind::AddSample; }
    std::size_t count() const override { return 1; }

private:
    ComponentType type_;
    ComponentInstance sample_;
    Counters before_;
    Counters after_;
    std::string lastTypeBefore_;
};

// 接受：拿掉上一次自動產生的元件，放進這一次的（ADR-025）。
class AcceptCommand final : public DocumentCommand {
public:
    AcceptCommand(std::vector<ComponentInstance> removed, std::vector<ComponentInstance> added, Counters before, Counters after,
                  ProposalSession session, std::string lastTypeBefore, std::string lastTypeAfter)
        : removed_(std::move(removed)), added_(std::move(added)), before_(std::move(before)), after_(std::move(after)),
          session_(std::move(session)), lastTypeBefore_(std::move(lastTypeBefore)), lastTypeAfter_(std::move(lastTypeAfter)) {}

    void redo(InspectionDocument& d) const override {
        Board& board = Access::board(d);
        eraseComponents(board.components, removed_);
        // 新元件的編號都比現有的大（編號只增不減，而且復原會把計數還原），接在最後面就保持排序。
        board.components.insert(board.components.end(), added_.begin(), added_.end());
        Access::counters(d) = after_;
        Access::lastTypeId(d) = lastTypeAfter_;
    }
    void undo(InspectionDocument& d) const override {
        Board& board = Access::board(d);
        board.components.resize(board.components.size() - added_.size());
        insertComponents(board.components, removed_);
        Access::counters(d) = before_;
        Access::lastTypeId(d) = lastTypeBefore_;
    }
    CommandKind kind() const override { return CommandKind::Accept; }
    std::size_t count() const override { return added_.size(); }
    const ProposalSession* proposal() const override { return &session_; }

private:
    std::vector<ComponentInstance> removed_;  // 依編號排序
    std::vector<ComponentInstance> added_;    // 依編號排序（都比 removed 的大）
    Counters before_;
    Counters after_;
    ProposalSession session_;
    std::string lastTypeBefore_;
    std::string lastTypeAfter_;
};

class DiscardCommand final : public DocumentCommand {
public:
    explicit DiscardCommand(ProposalSession session) : session_(std::move(session)) {}
    void redo(InspectionDocument&) const override {}
    void undo(InspectionDocument&) const override {}
    CommandKind kind() const override { return CommandKind::Discard; }
    std::size_t count() const override { return 0; }
    const ProposalSession* proposal() const override { return &session_; }

private:
    ProposalSession session_;
};

class DeleteCommand final : public DocumentCommand {
public:
    explicit DeleteCommand(std::vector<ComponentInstance> removed) : removed_(std::move(removed)) {}  // 依編號排序

    void redo(InspectionDocument& d) const override { eraseComponents(Access::board(d).components, removed_); }
    void undo(InspectionDocument& d) const override { insertComponents(Access::board(d).components, removed_); }
    CommandKind kind() const override { return CommandKind::DeleteComponents; }
    std::size_t count() const override { return removed_.size(); }

private:
    std::vector<ComponentInstance> removed_;
};

class AddCommand final : public DocumentCommand {
public:
    AddCommand(ComponentInstance component, Counters before, Counters after)
        : component_(std::move(component)), before_(std::move(before)), after_(std::move(after)) {}

    void redo(InspectionDocument& d) const override {
        Access::board(d).components.push_back(component_);
        Access::counters(d) = after_;
        Access::lastTypeId(d) = component_.typeId;
    }
    void undo(InspectionDocument& d) const override {
        Access::board(d).components.pop_back();
        Access::counters(d) = before_;
        Access::lastTypeId(d) = lastTypeBefore_;
    }
    CommandKind kind() const override { return CommandKind::AddComponent; }
    std::size_t count() const override { return 1; }
    void setLastTypeBefore(std::string id) { lastTypeBefore_ = std::move(id); }

private:
    ComponentInstance component_;
    Counters before_;
    Counters after_;
    std::string lastTypeBefore_;
};

class MoveCommand final : public DocumentCommand {
public:
    struct Entry {
        int id = 0;
        Pose2D before;
        Pose2D after;
    };
    explicit MoveCommand(std::vector<Entry> entries) : entries_(std::move(entries)) {}  // 依編號排序

    void redo(InspectionDocument& d) const override { apply(d, true); }
    void undo(InspectionDocument& d) const override { apply(d, false); }
    CommandKind kind() const override { return CommandKind::MoveComponents; }
    std::size_t count() const override { return entries_.size(); }

    std::shared_ptr<const Command<InspectionDocument>> mergeWith(const Command<InspectionDocument>& next) const override {
        const auto* other = dynamic_cast<const MoveCommand*>(&next);
        if (other == nullptr || other->entries_.size() != entries_.size()) return nullptr;
        std::vector<Entry> merged = entries_;
        for (std::size_t i = 0; i < merged.size(); ++i) {
            if (other->entries_[i].id != merged[i].id) return nullptr;
            merged[i].after = other->entries_[i].after;
        }
        return std::make_shared<MoveCommand>(std::move(merged));
    }

private:
    void apply(InspectionDocument& d, bool forward) const {
        auto& components = Access::board(d).components;
        for (const Entry& e : entries_) {
            const auto it = findById(components, e.id);
            if (it != components.end()) it->pose = forward ? e.after : e.before;
        }
    }
    std::vector<Entry> entries_;
};

// 改類型的主體框大小；拖的那個元件的位姿一起記（拖單邊時中心會移）。
class ResizeTypeCommand final : public DocumentCommand {
public:
    ResizeTypeCommand(std::string typeId, Size2D beforeUm, Size2D afterUm, int anchorId, Pose2D anchorBefore, Pose2D anchorAfter,
                      std::size_t affected)
        : typeId_(std::move(typeId)), beforeUm_(beforeUm), afterUm_(afterUm), anchorId_(anchorId), anchorBefore_(anchorBefore),
          anchorAfter_(anchorAfter), affected_(affected) {}

    void redo(InspectionDocument& d) const override { apply(d, afterUm_, anchorAfter_); }
    void undo(InspectionDocument& d) const override { apply(d, beforeUm_, anchorBefore_); }
    CommandKind kind() const override { return CommandKind::ResizeType; }
    std::size_t count() const override { return affected_; }

private:
    void apply(InspectionDocument& d, Size2D um, const Pose2D& anchor) const {
        Board& board = Access::board(d);
        if (ComponentType* type = findTypeMutable(board, typeId_)) {
            if (BoxGroup* body = bodyOf(*type)) {
                body->widthUm = um.w;
                body->heightUm = um.h;
            }
        }
        const auto it = findById(board.components, anchorId_);
        if (it != board.components.end()) it->pose = anchor;
    }

    std::string typeId_;
    Size2D beforeUm_;
    Size2D afterUm_;
    int anchorId_;
    Pose2D anchorBefore_;
    Pose2D anchorAfter_;
    std::size_t affected_;
};

// 把幾個類型的主體框設成同一個大小；每個類型記「前」的大小，復原時各自還原。元件的位姿不動。
class ResizeTypesCommand final : public DocumentCommand {
public:
    struct Entry {
        std::string typeId;
        Size2D beforeUm;
    };
    ResizeTypesCommand(std::vector<Entry> entries, Size2D afterUm) : entries_(std::move(entries)), afterUm_(afterUm) {}

    void redo(InspectionDocument& d) const override {
        for (const Entry& e : entries_) apply(d, e.typeId, afterUm_);
    }
    void undo(InspectionDocument& d) const override {
        for (const Entry& e : entries_) apply(d, e.typeId, e.beforeUm);
    }
    CommandKind kind() const override { return CommandKind::ResizeTypes; }
    std::size_t count() const override { return entries_.size(); }

private:
    static void apply(InspectionDocument& d, const std::string& typeId, Size2D um) {
        if (ComponentType* type = findTypeMutable(Access::board(d), typeId)) {
            if (BoxGroup* body = bodyOf(*type)) {
                body->widthUm = um.w;
                body->heightUm = um.h;
            }
        }
    }

    std::vector<Entry> entries_;
    Size2D afterUm_;
};

// 在類型上加一個框群組（接在最後；復原時拿掉最後一個）。
class AddBoxGroupCommand final : public DocumentCommand {
public:
    AddBoxGroupCommand(std::string typeId, BoxGroup group, std::size_t affected)
        : typeId_(std::move(typeId)), group_(std::move(group)), affected_(affected) {}

    void redo(InspectionDocument& d) const override {
        if (ComponentType* type = findTypeMutable(Access::board(d), typeId_)) type->groups.push_back(group_);
    }
    void undo(InspectionDocument& d) const override {
        if (ComponentType* type = findTypeMutable(Access::board(d), typeId_)) {
            if (!type->groups.empty() && type->groups.back().id == group_.id) type->groups.pop_back();
        }
    }
    CommandKind kind() const override { return CommandKind::AddBoxGroup; }
    std::size_t count() const override { return affected_; }

private:
    std::string typeId_;
    BoxGroup group_;
    std::size_t affected_;
};

// 改類型的比對區與比對特徵（ADR-028）。拖外擴滑桿時，同一個類型連續的修改併成一步（保留最早的「前」）。
class SetMatchCommand final : public DocumentCommand {
public:
    SetMatchCommand(std::string typeId, MatchSettings before, MatchSettings after, std::size_t affected)
        : typeId_(std::move(typeId)), before_(std::move(before)), after_(std::move(after)), affected_(affected) {}

    void redo(InspectionDocument& d) const override { apply(d, after_); }
    void undo(InspectionDocument& d) const override { apply(d, before_); }
    CommandKind kind() const override { return CommandKind::SetMatchRegion; }
    std::size_t count() const override { return affected_; }

    std::shared_ptr<const Command<InspectionDocument>> mergeWith(const Command<InspectionDocument>& next) const override {
        const auto* other = dynamic_cast<const SetMatchCommand*>(&next);
        if (other == nullptr || other->typeId_ != typeId_) return nullptr;
        return std::make_shared<SetMatchCommand>(typeId_, before_, other->after_, other->affected_);
    }

private:
    void apply(InspectionDocument& d, const MatchSettings& settings) const {
        if (ComponentType* type = findTypeMutable(Access::board(d), typeId_)) type->match = settings;
    }

    std::string typeId_;
    MatchSettings before_;
    MatchSettings after_;
    std::size_t affected_;
};

}  // namespace

InspectionDocument::InspectionDocument() = default;

void InspectionDocument::reset(std::string name, int imageWidth, int imageHeight, double umPerPixel) {
    const int significantBits = board_.significantBits;  // 相機的設定：換影像沿用
    board_ = Board{};
    board_.significantBits = significantBits;
    board_.name = std::move(name);
    board_.imageWidth = imageWidth;
    board_.imageHeight = imageHeight;
    board_.umPerPixel = umPerPixel;
    counters_ = Counters{};
    lastTypeId_.clear();
    history_.clear();
    notify(DocumentChange::Cause::Reset, CommandKind::Accept, 0);
}

bool InspectionDocument::setSignificantBits(int bits) {
    if (!validSignificantBitsSetting(bits)) return false;
    board_.significantBits = bits;
    return true;
}

const ComponentInstance* InspectionDocument::find(int componentId) const {
    const auto it = std::lower_bound(board_.components.begin(), board_.components.end(), componentId,
                                     [](const ComponentInstance& c, int key) { return c.id < key; });
    return it != board_.components.end() && it->id == componentId ? &*it : nullptr;
}

const ComponentType* InspectionDocument::findType(const std::string& typeId) const {
    for (const ComponentType& t : board_.types) {
        if (t.id == typeId) return &t;
    }
    return nullptr;
}

std::optional<Size2D> InspectionDocument::bodySizePx(const std::string& typeId) const {
    const ComponentType* type = findType(typeId);
    if (type == nullptr) return std::nullopt;
    const double pxPerUm = 1.0 / effectiveUmPerPixel(board_.umPerPixel);
    for (const BoxGroup& g : type->groups) {
        if (g.kind == BoxGroupKind::Body) return Size2D{g.widthUm * pxPerUm, g.heightUm * pxPerUm};
    }
    return std::nullopt;
}

std::vector<SampleOnBoard> InspectionDocument::samples() const {
    std::vector<SampleOnBoard> out;
    const double um = effectiveUmPerPixel(board_.umPerPixel);
    for (const ComponentInstance& c : board_.components) {
        if (c.state != ObjectState::Sample) continue;
        const std::optional<Size2D> size = bodySizePx(c.typeId);
        if (!size) continue;
        SampleOnBoard s{c.typeId, c.id, rectFromCenter(c.pose.position(), *size), {}};
        if (const ComponentType* type = findType(c.typeId)) s.match = matchSpecFor(type->match, c.pose.position(), um);
        out.push_back(std::move(s));
    }
    return out;
}

std::vector<int> InspectionDocument::componentsOfType(const std::string& typeId) const {
    std::vector<int> out;
    for (const ComponentInstance& c : board_.components) {
        if (c.typeId == typeId) out.push_back(c.id);
    }
    return out;
}

void InspectionDocument::notify(DocumentChange::Cause cause, CommandKind kind, std::size_t count) {
    ++revision_;
    if (observer_) observer_(DocumentChange{cause, kind, count});
}

void InspectionDocument::push(UndoStack<InspectionDocument>::CommandPtr command, bool allowMerge) {
    const CommandKind kind = command->kind();
    const std::size_t count = command->count();
    history_.push(*this, std::move(command), allowMerge);
    notify(DocumentChange::Cause::Edit, kind, count);
}

int InspectionDocument::addSample(const Rect& samplePx) {
    if (samplePx.empty()) return 0;
    Counters before = counters_;
    Counters after = before;

    ComponentType type;
    type.id = "t" + std::to_string(after.nextTypeId++);
    type.category = ComponentCategory::Other;
    BoxGroup body;
    body.id = 1;
    body.kind = BoxGroupKind::Body;
    const double um = effectiveUmPerPixel(board_.umPerPixel);
    body.widthUm = samplePx.w * um;
    body.heightUm = samplePx.h * um;
    type.groups.push_back(body);

    ComponentInstance sample;
    sample.id = after.nextComponentId++;
    sample.name = takeName(after, type.category);
    const Point2D center = samplePx.center();
    sample.pose = Pose2D{center.x, center.y, 0.0};
    sample.typeId = type.id;
    sample.state = ObjectState::Sample;
    sample.origin = ObjectOrigin::Manual;
    const int id = sample.id;
    push(std::make_shared<AddSampleCommand>(std::move(type), std::move(sample), std::move(before), std::move(after), lastTypeId_));
    return id;
}

std::size_t InspectionDocument::accept(const ProposalSession& session) {
    const ProposalObjects view = session.objects();
    Counters before = counters_;
    Counters after = before;

    std::vector<ComponentInstance> added;
    added.reserve(view.objects.size());
    std::string lastType = lastTypeId_;
    for (const InspectionObject& object : view.objects) {
        if (object.state == ObjectState::Rejected || object.state == ObjectState::Sample) continue;
        const ComponentType* type = findType(object.typeId);
        if (type == nullptr) continue;  // 類型已不在板子上（例如復原了建立樣本）：這個候選沒有地方可以放
        ComponentInstance c;
        c.id = after.nextComponentId++;
        c.name = takeName(after, type->category);
        c.pose = object.rect.pose;
        c.typeId = object.typeId;
        c.state = object.state;
        c.score = object.score;
        c.origin = object.origin;
        lastType = c.typeId;
        added.push_back(std::move(c));
    }
    if (added.empty()) return 0;

    // 上一次自動產生的元件整批換掉（樣本與手動補的留著）。
    std::vector<ComponentInstance> removed;
    for (const ComponentInstance& c : board_.components) {
        if (c.origin == ObjectOrigin::Auto && c.state != ObjectState::Sample) removed.push_back(c);
    }
    const std::size_t accepted = added.size();
    push(std::make_shared<AcceptCommand>(std::move(removed), std::move(added), std::move(before), std::move(after), session,
                                         lastTypeId_, std::move(lastType)));
    return accepted;
}

void InspectionDocument::discard(const ProposalSession& session) {
    push(std::make_shared<DiscardCommand>(session));
}

std::size_t InspectionDocument::removeComponents(const std::vector<int>& ids) {
    std::vector<int> sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    std::vector<ComponentInstance> removed;
    for (int id : sorted) {
        if (const ComponentInstance* c = find(id)) removed.push_back(*c);
    }
    if (removed.empty()) return 0;
    const std::size_t count = removed.size();
    push(std::make_shared<DeleteCommand>(std::move(removed)));
    return count;
}

int InspectionDocument::addManualComponent(const Pose2D& pose, const std::string& typeId) {
    const ComponentType* type = findType(typeId);
    if (type == nullptr) return 0;
    Counters before = counters_;
    Counters after = before;
    ComponentInstance c;
    c.id = after.nextComponentId++;
    c.name = takeName(after, type->category);
    c.pose = pose;
    c.typeId = typeId;
    c.state = ObjectState::Found;
    c.origin = ObjectOrigin::Manual;
    const int id = c.id;
    auto command = std::make_shared<AddCommand>(std::move(c), std::move(before), std::move(after));
    command->setLastTypeBefore(lastTypeId_);
    push(std::move(command));
    return id;
}

std::size_t InspectionDocument::moveComponents(const std::vector<int>& ids, Point2D delta, bool allowMerge) {
    std::vector<int> sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    std::vector<MoveCommand::Entry> entries;
    for (int id : sorted) {
        if (const ComponentInstance* c = find(id)) {
            Pose2D after = c->pose;
            after.x += delta.x;
            after.y += delta.y;
            entries.push_back({id, c->pose, after});
        }
    }
    if (entries.empty()) return 0;
    const std::size_t count = entries.size();
    push(std::make_shared<MoveCommand>(std::move(entries)), allowMerge);
    return count;
}

bool InspectionDocument::setBodySize(const std::string& typeId, Size2D sizePx, int anchorId, const Pose2D& anchorPose) {
    if (!(sizePx.w > 0.0) || !(sizePx.h > 0.0)) return false;
    const ComponentType* type = findType(typeId);
    const ComponentInstance* anchor = find(anchorId);
    if (type == nullptr || anchor == nullptr || anchor->typeId != typeId) return false;
    const BoxGroup* body = nullptr;
    for (const BoxGroup& g : type->groups) {
        if (g.kind == BoxGroupKind::Body) body = &g;
    }
    if (body == nullptr) return false;
    const double um = effectiveUmPerPixel(board_.umPerPixel);
    const Size2D beforeUm{body->widthUm, body->heightUm};
    const Size2D afterUm{sizePx.w * um, sizePx.h * um};
    const std::size_t affected = componentsOfType(typeId).size();
    push(std::make_shared<ResizeTypeCommand>(typeId, beforeUm, afterUm, anchorId, anchor->pose, anchorPose, affected));
    return true;
}

bool InspectionDocument::setBodySizeForTypes(const std::vector<std::string>& typeIds, Size2D sizePx) {
    if (!(sizePx.w > 0.0) || !(sizePx.h > 0.0)) return false;
    const double um = effectiveUmPerPixel(board_.umPerPixel);
    const Size2D afterUm{sizePx.w * um, sizePx.h * um};
    std::vector<ResizeTypesCommand::Entry> entries;
    for (const std::string& typeId : typeIds) {
        // 同一個類型只處理一次（幾個樣本可以是同一個類型）。
        bool seen = false;
        for (const ResizeTypesCommand::Entry& e : entries) seen = seen || e.typeId == typeId;
        if (seen) continue;
        const ComponentType* type = findType(typeId);
        if (type == nullptr) continue;
        for (const BoxGroup& g : type->groups) {
            if (g.kind != BoxGroupKind::Body) continue;
            if (g.widthUm == afterUm.w && g.heightUm == afterUm.h) break;  // 已經是這個大小
            entries.push_back({typeId, Size2D{g.widthUm, g.heightUm}});
            break;
        }
    }
    if (entries.empty()) return false;
    push(std::make_shared<ResizeTypesCommand>(std::move(entries), afterUm));
    return true;
}

bool InspectionDocument::addBoxGroup(const std::string& typeId, BoxGroup group) {
    const ComponentType* type = findType(typeId);
    if (type == nullptr) return false;
    int maxId = 0;
    for (const BoxGroup& g : type->groups) maxId = std::max(maxId, g.id);
    group.id = maxId + 1;
    const std::size_t affected = componentsOfType(typeId).size();
    push(std::make_shared<AddBoxGroupCommand>(typeId, std::move(group), affected));
    return true;
}

bool InspectionDocument::setMatchSettings(const std::string& typeId, const MatchSettings& settings, bool allowMerge) {
    const ComponentType* type = findType(typeId);
    if (type == nullptr || type->match == settings) return false;
    const std::size_t affected = componentsOfType(typeId).size();
    push(std::make_shared<SetMatchCommand>(typeId, type->match, settings, affected), allowMerge);
    return true;
}

UndoOutcome InspectionDocument::undo() {
    UndoOutcome out;
    const Command<InspectionDocument>* next = history_.nextUndo();
    if (next == nullptr) return out;
    out.kind = next->kind();
    out.count = next->count();
    if (const auto* dc = dynamic_cast<const DocumentCommand*>(next)) {
        if (const ProposalSession* carried = dc->proposal()) out.restoredProposal = *carried;
    }
    history_.undo(*this);
    out.performed = true;
    notify(DocumentChange::Cause::Undo, out.kind, out.count);
    return out;
}

UndoOutcome InspectionDocument::redo() {
    UndoOutcome out;
    const Command<InspectionDocument>* next = history_.nextRedo();
    if (next == nullptr) return out;
    out.kind = next->kind();
    out.count = next->count();
    if (const auto* dc = dynamic_cast<const DocumentCommand*>(next)) out.consumedProposal = dc->proposal() != nullptr;
    history_.redo(*this);
    out.performed = true;
    notify(DocumentChange::Cause::Redo, out.kind, out.count);
    return out;
}

std::optional<CommandInfo> InspectionDocument::undoInfo() const {
    const auto* c = history_.nextUndo();
    if (c == nullptr) return std::nullopt;
    return CommandInfo{c->kind(), c->count()};
}

std::optional<CommandInfo> InspectionDocument::redoInfo() const {
    const auto* c = history_.nextRedo();
    if (c == nullptr) return std::nullopt;
    return CommandInfo{c->kind(), c->count()};
}

const ProposalSession* InspectionDocument::nextRedoProposal() const {
    const auto* dc = dynamic_cast<const DocumentCommand*>(history_.nextRedo());
    return dc != nullptr ? dc->proposal() : nullptr;
}

}  // namespace aoi
