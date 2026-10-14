#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

// application 層：只用標準庫（不能用 QUndoStack，見 docs/adr/002-undo-model.md）。
//
// Command 模式的復原堆疊。三個範圍各有一個（提案、類型編輯草稿、已接受的文件），所以做成以「目標」為參數的樣板：
//   UndoStack<InspectionDocument>  已接受的文件
//   UndoStack<ProposalState>       目前的提案
//
// 設計決定：
//  - Command 建立之後不可變（redo／undo 都是 const，參數是目標）。所以同一個 Command 可以被多份堆疊（例如提案堆疊的複本）共用，
//    「合併」也不修改舊的 Command，而是回傳一個新的。
//  - 一個 Command 對應一個使用者意圖（ADR-002 規則 8）；push 時執行一次 redo，所以呼叫端不必先改再 push。
//  - 深度上限 200（規則 9），超過丟掉最舊的並釋放它帶的資料。
//  - 「未儲存的修改」＝堆疊目前位置是否等於 markClean 時的位置（規則 10）。
namespace aoi {

// Command 的種類：presentation 用它與 count 組出「復原：刪除 12 個元件」之類的文字（文字不在 application 層）。
enum class CommandKind {
    Accept,            // 接受提案（2026-10-14 起：上一次自動產生的元件整批換成這一次的，ADR-025）
    Discard,           // 捨棄提案
    DeleteComponents,  // 刪除元件（板上或提案內）
    AddComponent,      // 補一個元件
    MoveComponents,    // 移動元件
    AddSample,         // 建立一個樣本（一個新類型與它的樣本元件；S 工具放開時）
    ResizeType,        // 改類型的主體框大小（同類型全部元件一起變）
    AddBoxGroup,       // 在類型上加一個框群組（子框；同類型全部元件一起多出這些框）
    ResizeTypes,       // 把幾個類型的主體框設成同一個大小（選取資訊的〔套用大小到所有樣本〕；元件的中心不動）
};

template <class Target>
class Command {
public:
    virtual ~Command() = default;
    virtual void redo(Target& target) const = 0;
    virtual void undo(Target& target) const = 0;
    virtual CommandKind kind() const = 0;
    virtual std::size_t count() const = 0;  // 影響的物件數（顯示用）

    // 把緊接在自己之後的 next 併進來：成功就回傳「相當於先做自己、再做 next」的新 Command（自己不變），否則回 nullptr。
    // 只有 push 時呼叫端允許合併（allowMerge），才會被問到。
    virtual std::shared_ptr<const Command> mergeWith(const Command& next) const {
        (void)next;
        return nullptr;
    }
};

template <class Target>
class UndoStack {
public:
    using CommandType = Command<Target>;
    using CommandPtr = std::shared_ptr<const CommandType>;

    static constexpr std::size_t kDefaultMaxDepth = 200;

    explicit UndoStack(std::size_t maxDepth = kDefaultMaxDepth) : maxDepth_(maxDepth == 0 ? 1 : maxDepth) {}

    // 執行 command.redo(target) 並放進堆疊，清掉可重做的部分。
    // allowMerge ＝ true 且頂端的 Command 願意合併時，不增加深度（連續微調一個框，只算一步）。
    void push(Target& target, CommandPtr command, bool allowMerge = false) {
        command->redo(target);
        commands_.resize(applied_);
        if (cleanPos_ && *cleanPos_ > applied_) cleanPos_.reset();  // 乾淨的位置在被丟掉的可重做部分裡

        if (allowMerge && applied_ > 0) {
            if (CommandPtr merged = commands_.back()->mergeWith(*command)) {
                commands_.back() = std::move(merged);
                // 頂端的內容變了：如果它正好是「已儲存」的位置，那個位置就回不去了。
                if (cleanPos_ && *cleanPos_ == applied_) cleanPos_.reset();
                return;
            }
        }
        commands_.push_back(std::move(command));
        ++applied_;
        if (commands_.size() > maxDepth_) {
            commands_.erase(commands_.begin());
            --applied_;
            if (cleanPos_) {
                if (*cleanPos_ == 0) cleanPos_.reset();
                else --*cleanPos_;
            }
        }
    }

    bool undo(Target& target) {
        if (!canUndo()) return false;
        commands_[applied_ - 1]->undo(target);
        --applied_;
        return true;
    }

    bool redo(Target& target) {
        if (!canRedo()) return false;
        commands_[applied_]->redo(target);
        ++applied_;
        return true;
    }

    bool canUndo() const noexcept { return applied_ > 0; }
    bool canRedo() const noexcept { return applied_ < commands_.size(); }
    // 下一次 undo／redo 會作用的 Command（沒有就是空指標）。
    const CommandType* nextUndo() const { return canUndo() ? commands_[applied_ - 1].get() : nullptr; }
    const CommandType* nextRedo() const { return canRedo() ? commands_[applied_].get() : nullptr; }

    // 已套用的步數（上限 maxDepth）。
    std::size_t depth() const noexcept { return applied_; }
    std::size_t redoDepth() const noexcept { return commands_.size() - applied_; }

    // 丟掉可重做的部分（ADR-002 規則 4：接受或捨棄時，提案堆疊殘留的「重做」不可以留下）。
    void dropRedo() {
        commands_.resize(applied_);
        if (cleanPos_ && *cleanPos_ > applied_) cleanPos_.reset();
    }

    void clear() {
        commands_.clear();
        applied_ = 0;
        cleanPos_ = 0;
    }

    void markClean() { cleanPos_ = applied_; }
    bool isClean() const noexcept { return cleanPos_ && *cleanPos_ == applied_; }

private:
    std::size_t maxDepth_;
    std::vector<CommandPtr> commands_;
    std::size_t applied_ = 0;             // commands_[0, applied_) 已套用
    std::optional<std::size_t> cleanPos_ = 0;  // markClean 時的 applied_；nullopt ＝ 那個位置已經回不去
};

}  // namespace aoi
