#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "application/document/ProposalSession.h"
#include "application/document/UndoStack.h"
#include "domain/component.h"
#include "domain/geometry.h"

// application 層：只用標準庫。

namespace aoi {

namespace detail {
struct DocumentAccess;  // 只有 InspectionDocument.cpp 裡的 Command 用它改文件
}

struct DocumentChange {
    enum class Cause { Edit, Undo, Redo, Reset };
    Cause cause = Cause::Edit;
    CommandKind kind = CommandKind::Accept;  // Reset 時沒有意義
    std::size_t count = 0;
};

// undo()／redo() 的結果。狀態機需要知道：復原了「接受／捨棄」要回到預覽（UndoToPreview），重做它們要再離開預覽。
struct UndoOutcome {
    bool performed = false;
    CommandKind kind = CommandKind::Accept;
    std::size_t count = 0;
    std::optional<ProposalSession> restoredProposal;  // 復原了接受／捨棄：當時的提案（連同它的修改堆疊）
    bool consumedProposal = false;                    // 重做了接受／捨棄
};

// 已接受的文件：一塊板子的元件，以及對它的全部修改（ADR-002 的「文件堆疊」）。
//
//  - 所有修改都是一個 Command（一個使用者意圖一個），執行時發出**一次**變更通知（規則 11，I6 的前提）。
//  - 板子的資料模型是 domain 的 Board（Board › Component › Box）：元件依編號由小到大排列，編號只增不減，
//    所以刪除再復原會回到同一個位置，名稱與編號都不會變。
//  - 接受提案時，板子得到一個新的「暫時類型」（流程 A：只有主體框），元件指向它。
//  - 不執行緒安全：只在 UI 執行緒使用（工作執行緒只傳不可變的快照）。
class InspectionDocument {
public:
    // 編號與名稱的計數；Command 在建立時算好「前」「後」兩份，復原與重做時整份換回去，所以編號與名稱永遠不會變。
    struct Counters {
        int nextComponentId = 1;
        int nextTypeId = 1;
        std::map<std::string, int> nameSeq;  // 名稱開頭字元 → 已用到的流水號
    };

    InspectionDocument();

    // 開啟新影像：清掉板子與歷史，通知 Reset。影像的有效位元數設定（Board::significantBits）保留：它是相機的設定，
    // 下一張影像多半來自同一台相機（使用者 2026-10-06 決定：換影像沿用，直到使用者改回「自動」）。
    void reset(std::string name, int imageWidth, int imageHeight, double umPerPixel);

    // 影像的有效位元數設定（S07）：0 ＝ 自動（看最大值推測），8..16 ＝ 指定。不是 Command：和分數門檻一樣是檢視的設定，
    // 不進復原堆疊、不改「有沒有未存的修改」；存配方時一起存（schema v2）。不合法的值（不是 0、也不在 8..16）不收，回 false。
    bool setSignificantBits(int bits);

    const Board& board() const { return board_; }
    const std::vector<ComponentInstance>& components() const { return board_.components; }
    const ComponentInstance* find(int componentId) const;
    const ComponentType* findType(const std::string& typeId) const;
    // 最近一次接受（或補一個）用的類型；A 工具補元件的預設類型。空字串 ＝ 板子上還沒有類型。
    std::string lastTypeId() const { return lastTypeId_; }

    // ---- 修改（各是一個 Command）----
    // 接受提案：Sample、Found、Inferred 變成元件，Rejected 不要；指派名稱。回傳元件數。沒有任何元件可接受時回 0（不進堆疊）。
    std::size_t accept(const ProposalSession& session);
    // 捨棄提案：板子不變，但這一步帶著提案，復原時還給預覽。
    void discard(const ProposalSession& session);
    std::size_t removeComponents(const std::vector<int>& ids);
    // 補一個：用 typeId 的類型、手動來源、沒有分數。typeId 不存在時回 0；否則回新元件的編號。
    int addManualComponent(const Pose2D& pose, const std::string& typeId);
    // 移動；allowMerge ＝ true 且上一步是對同一組元件的移動時併成一步。
    std::size_t moveComponents(const std::vector<int>& ids, Point2D delta, bool allowMerge = false);

    // ---- 歷史 ----
    UndoOutcome undo();
    UndoOutcome redo();
    bool canUndo() const { return history_.canUndo(); }
    bool canRedo() const { return history_.canRedo(); }
    std::optional<CommandInfo> undoInfo() const;
    std::optional<CommandInfo> redoInfo() const;
    // 下一步重做若是「接受／捨棄」，它帶的提案；否則空指標。預覽中按 Ctrl+Y 時，用它判斷是不是在重做剛才復原的那一步。
    const ProposalSession* nextRedoProposal() const;
    std::size_t historyDepth() const { return history_.depth(); }
    // 丟掉可重做的部分。預覽中改了提案（編輯、門檻）之後，先前復原出來的「接受」不能再重做：它帶的是舊的提案。
    void dropRedo() { history_.dropRedo(); }

    bool isDirty() const { return !history_.isClean(); }
    void markSaved() { history_.markClean(); }

    // 修訂號：每次變更通知（Command、復原、重做、reset）加 1；板子的內容（元件、類型、校正值）只會在這些時候變。
    // 讀取端拿它判斷「上次看過的還是不是現在的」（S11：Presenter 快取文件那一部分的場景）。
    // 有效位元數、markSaved、dropRedo 不改板子的內容，不算。
    std::uint64_t revision() const { return revision_; }

    // 每個 Command（含復原、重做）通知一次。只有一個觀察者；空 function ＝ 不通知。
    void setObserver(std::function<void(const DocumentChange&)> observer) { observer_ = std::move(observer); }

private:
    friend struct detail::DocumentAccess;

    void notify(DocumentChange::Cause cause, CommandKind kind, std::size_t count);
    void push(UndoStack<InspectionDocument>::CommandPtr command, bool allowMerge = false);

    Board board_;
    Counters counters_;
    std::string lastTypeId_;
    UndoStack<InspectionDocument> history_;
    std::function<void(const DocumentChange&)> observer_;
    std::uint64_t revision_ = 0;
};

}  // namespace aoi
