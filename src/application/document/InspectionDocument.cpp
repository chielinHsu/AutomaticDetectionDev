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
// **暫定**：流程 A 的暫時類型沒有類別（Other），所以接受出來的元件叫 X1、X2…；類型編輯器（流程 C）設定類別後才有 R、U。
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

class AcceptCommand final : public DocumentCommand {
public:
    AcceptCommand(ComponentType type, std::vector<ComponentInstance> components, Counters before, Counters after,
                  ProposalSession session)
        : type_(std::move(type)), components_(std::move(components)), before_(std::move(before)),
          after_(std::move(after)), session_(std::move(session)) {}

    void redo(InspectionDocument& d) const override {
        Board& board = Access::board(d);
        board.types.push_back(type_);
        // 新元件的編號都比現有的大（編號只增不減，而且復原會把計數還原），接在最後面就保持排序。
        board.components.insert(board.components.end(), components_.begin(), components_.end());
        Access::counters(d) = after_;
        Access::lastTypeId(d) = type_.id;
    }
    void undo(InspectionDocument& d) const override {
        Board& board = Access::board(d);
        board.components.resize(board.components.size() - components_.size());
        board.types.erase(std::remove_if(board.types.begin(), board.types.end(),
                                         [this](const ComponentType& t) { return t.id == type_.id; }),
                          board.types.end());
        Access::counters(d) = before_;
        Access::lastTypeId(d) = lastTypeBefore_;
    }
    CommandKind kind() const override { return CommandKind::Accept; }
    std::size_t count() const override { return components_.size(); }
    const ProposalSession* proposal() const override { return &session_; }

    // 接受之前的 lastTypeId：建構時由文件填入，復原時還原。
    void setLastTypeBefore(std::string id) { lastTypeBefore_ = std::move(id); }

private:
    ComponentType type_;
    std::vector<ComponentInstance> components_;
    Counters before_;
    Counters after_;
    ProposalSession session_;
    std::string lastTypeBefore_;
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

    void redo(InspectionDocument& d) const override {
        auto& components = Access::board(d).components;
        std::vector<ComponentInstance> kept;
        kept.reserve(components.size() - removed_.size());
        auto gone = removed_.begin();
        for (ComponentInstance& c : components) {
            if (gone != removed_.end() && gone->id == c.id) {
                ++gone;
                continue;
            }
            kept.push_back(std::move(c));
        }
        components = std::move(kept);
    }
    void undo(InspectionDocument& d) const override {
        auto& components = Access::board(d).components;
        const auto middle = static_cast<std::ptrdiff_t>(components.size());
        components.insert(components.end(), removed_.begin(), removed_.end());
        std::inplace_merge(components.begin(), components.begin() + middle, components.end(),
                           [](const ComponentInstance& a, const ComponentInstance& b) { return a.id < b.id; });
    }
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

void InspectionDocument::notify(DocumentChange::Cause cause, CommandKind kind, std::size_t count) {
    if (observer_) observer_(DocumentChange{cause, kind, count});
}

void InspectionDocument::push(UndoStack<InspectionDocument>::CommandPtr command, bool allowMerge) {
    const CommandKind kind = command->kind();
    const std::size_t count = command->count();
    history_.push(*this, std::move(command), allowMerge);
    notify(DocumentChange::Cause::Edit, kind, count);
}

std::size_t InspectionDocument::accept(const ProposalSession& session) {
    const ProposalObjects view = session.objects();
    Counters before = counters_;
    Counters after = before;

    // 這一批元件共用一個新的暫時類型：只有主體框，大小就是範本大小（µm；換算見 box_expander.h 的 effectiveUmPerPixel）。
    ComponentType type;
    type.id = "t" + std::to_string(after.nextTypeId++);
    type.category = ComponentCategory::Other;
    BoxGroup body;
    body.id = 1;
    body.kind = BoxGroupKind::Body;
    const double um = effectiveUmPerPixel(board_.umPerPixel);
    body.widthUm = view.partSize.w * um;
    body.heightUm = view.partSize.h * um;
    type.groups.push_back(body);

    std::vector<ComponentInstance> components;
    components.reserve(view.objects.size());
    for (const InspectionObject& object : view.objects) {
        if (object.state == ObjectState::Rejected) continue;
        ComponentInstance c;
        c.id = after.nextComponentId++;
        c.name = takeName(after, type.category);
        c.pose = object.rect.pose;
        c.typeId = type.id;
        c.state = object.state;
        c.score = object.score;
        c.origin = object.origin;
        components.push_back(std::move(c));
    }
    if (components.empty()) return 0;

    const std::size_t accepted = components.size();
    auto command = std::make_shared<AcceptCommand>(std::move(type), std::move(components), std::move(before),
                                                   std::move(after), session);
    command->setLastTypeBefore(lastTypeId_);
    push(std::move(command));
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
