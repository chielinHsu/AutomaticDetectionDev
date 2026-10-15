#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "application/document/UndoStack.h"
#include "application/usecases/BoardProposal.h"
#include "domain/geometry.h"
#include "domain/match.h"
#include "domain/object_state.h"

// application 層：只用標準庫。
namespace aoi {

// 使用者畫出的區域做什麼：Sample ＝ 流程 A「教樣本」（2026-10-14 起只建立樣本，搜尋另外按），Region ＝ 流程 B「區域掃描」。
enum class RegionMode { Sample, Region };

// 這個 Command 是什麼、影響幾個物件。顯示用（「復原：刪除 12 個元件」）；文字在 presentation。
struct CommandInfo {
    CommandKind kind = CommandKind::Accept;
    std::size_t count = 0;
};

// 使用者在預覽中對提案做的修改。不可變：每次修改產生一份新的，Command 只記「前」與「後」兩份（memento）。
struct ProposalEdits {
    struct Manual {
        int id = 0;
        Pose2D pose;
        std::string typeId;  // 補的元件的類型（所選物件的，或提案裡第一個候選的）
        Size2D partSize;     // 主體框的大小（畫面與分類用）
    };
    std::vector<int> deleted;                      // 被刪掉的候選編號，由小到大
    std::vector<std::pair<int, Point2D>> moved;    // 候選編號 → 累計位移，編號由小到大
    std::vector<Manual> manual;                    // 手動補的（編號接在候選之後）
    int nextManualId = 1;
};

// 提案堆疊的「目標」：Command 改的就是這個。
struct ProposalState {
    std::shared_ptr<const ProposalEdits> edits;
};

// 套過門檻與修改之後，使用者看到的物件。
struct ProposalObjects {
    std::vector<InspectionObject> objects;  // 依候選順序，手動補的接在最後；編號不隨門檻或修改改變
    std::size_t sample = 0;
    std::size_t found = 0;      // 含手動補的
    std::size_t inferred = 0;
    std::size_t rejected = 0;
    std::size_t manual = 0;
    std::size_t offLattice = 0;  // Found 與 Inferred 之中沒有晶格位姿的（不含手動補的；ADR-026，卡片上提醒）
    // 沒有可用結果的原因（取自 classify）；有結果時是 nullopt。修改（刪除、補一個）不會讓它出現或消失。
    std::optional<NoMatchInfo> noMatch;
};

// 一份提案與使用者在預覽中對它做的修改（ADR-002 的「提案堆疊」）。
//
// 值語意：複製一份是便宜的（提案本身與每一份修改都是共用的不可變資料，堆疊裡的 Command 也是），
// 所以「接受」與「捨棄」可以帶著整個提案一起放進文件堆疊，復原時原樣還給預覽（UndoToPreview）。
//
// 分數門檻不是修改：它是對快取候選重新過濾（不進堆疊，拖回去就回來），由 setThreshold 改。
// 2026-10-14 起提案是「板子的提案」（幾個樣本合併之後，每個候選帶著類型；ADR-025）。
class ProposalSession {
public:
    // candidateFloor 要與產生這份提案時的 GenerateOptions::candidateFloor 一致（classify 的建議門檻下限）。
    // positionMode（ADR-026）：候選用格點還是匹配位置；和門檻一樣不是修改（不進堆疊），由 setPositionMode 改。
    ProposalSession(std::shared_ptr<const BoardProposal> proposal, Rect region, RegionMode mode, double threshold,
                    double candidateFloor = 0.30, PositionMode positionMode = PositionMode::Lattice);

    const BoardProposal& proposal() const { return *proposal_; }
    const std::shared_ptr<const BoardProposal>& proposalPtr() const { return proposal_; }
    const Rect& region() const { return region_; }
    RegionMode mode() const { return mode_; }
    double threshold() const { return threshold_; }
    void setThreshold(double threshold) { threshold_ = threshold; }
    PositionMode positionMode() const { return positionMode_; }
    void setPositionMode(PositionMode mode) { positionMode_ = mode; }

    ProposalObjects objects() const;
    // 這個編號現在看得到嗎（沒被刪、存在）。
    bool contains(int objectId) const;
    // 這個物件的類型與主體框大小（候選或手動補的）；不存在時 nullopt。補一個、接受時用。
    struct TypeAndSize {
        std::string typeId;
        Size2D partSize;
    };
    std::optional<TypeAndSize> typeOf(int objectId) const;
    // 這個物件現在的位姿（候選的位姿加上累計的移動，或手動補的位姿）；不存在時 nullopt。只算這一個，不像 objects() 掃全部
    // （選取資訊的表單每次選取、每次門檻變動都要問）。
    std::optional<Pose2D> poseOf(int objectId) const;
    // 這個物件和晶格的關係（ADR-026；選取資訊的表單）：候選是不是晶格的成員（有晶格位姿，或 Predicted）、使用者有沒有移過它、
    // 現在的位置是不是格點（成員、沒移過、而且位置模式是 Lattice 或 Predicted），以及匹配位置離最近格點多遠（候選的
    // latticeDeviationPx）。手動補的、不存在的：全部 false、deviation 是 nullopt。
    struct LatticeRelation {
        bool inLattice = false;
        bool moved = false;
        bool onLattice = false;
        std::optional<double> deviationPx;
    };
    LatticeRelation latticeOf(int objectId) const;

    // ---- 修改（各是一個 Command）----
    // 刪除：不存在或已刪除的編號略過；回傳實際刪除的個數，0 ＝ 沒有任何改變（也不進堆疊）。
    std::size_t deleteObjects(const std::vector<int>& ids);
    // 補一個：位姿是影像像素；類型與大小由呼叫端決定（所選物件的，或第一個候選的）；狀態 Found、來源 Manual、沒有分數。回傳新物件的編號。
    int addManual(const Pose2D& pose, std::string typeId, Size2D partSize);
    // 移動：看得到的編號才動。allowMerge ＝ true 且上一步是對同一組物件的移動時，併成一步（方向鍵連續微調）。
    std::size_t moveObjects(const std::vector<int>& ids, Point2D delta, bool allowMerge = false);

    bool canUndo() const { return history_.canUndo(); }
    bool canRedo() const { return history_.canRedo(); }
    bool undo();
    bool redo();
    void dropRedo() { history_.dropRedo(); }
    std::optional<CommandInfo> undoInfo() const;
    std::optional<CommandInfo> redoInfo() const;
    std::size_t editCount() const { return history_.depth(); }

private:
    std::shared_ptr<const BoardProposal> proposal_;
    Rect region_;
    RegionMode mode_ = RegionMode::Sample;
    double threshold_ = 0.0;
    double candidateFloor_ = 0.30;
    PositionMode positionMode_ = PositionMode::Lattice;
    ProposalState state_;
    UndoStack<ProposalState> history_;
};

}  // namespace aoi
