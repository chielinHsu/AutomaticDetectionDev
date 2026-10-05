#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "application/Error.h"
#include "application/document/InspectionDocument.h"
#include "application/document/ProposalSession.h"
#include "application/interaction/InteractionStateMachine.h"
#include "domain/geometry.h"
#include "domain/object_state.h"

// application 層：只用標準庫（不含 Qt）。

namespace aoi {

// 使用者最近一次要搜尋的區域與做法。重新搜尋（參數變了）與再試一次都用它。
struct SearchTarget {
    Rect region;
    RegionMode mode = RegionMode::Sample;
};

enum class ErrorSource { Image, Search };

// 互動狀態的唯一擁有者：狀態機、已接受的文件、目前的提案、選取。
//
// 為什麼放在 application 層：「按 Enter 之後什麼會發生」「Ctrl+Z 作用在哪個堆疊」「刪除作用在提案還是文件」是規則，
// 不是畫面。規則寫在沒有 Qt 的類別裡，就能用決定性的單元測試逐項驗證；Presenter（Qt）只負責把使用者的動作交給它、
// 把結果畫出來，以及處理執行緒與計時。
//
// 不執行緒安全：只在 UI 執行緒使用。搜尋在 worker 上跑，結果以不可變的 SampleProposal 交回來（searchSucceeded）。
class InspectionSession {
public:
    struct Config {
        double minSamplePx = 8.0;        // 樣本邊長小於這個值不搜尋（ux-spec 第 8 節「最小樣本」）
        double defaultThreshold = 0.70;  // 分數門檻的起點（S01 原型：0.70）
        double thresholdMin = 0.30;      // 與搜尋下限相同：低於它沒有候選
        double thresholdMax = 0.99;
        double candidateFloor = 0.30;    // 要與 GenerateOptions::candidateFloor 一致
    };

    InspectionSession();
    explicit InspectionSession(Config config);

    // ---- 讀 ----
    InteractionState state() const { return machine_.state(); }
    const InteractionStateMachine& machine() const { return machine_; }
    const InspectionDocument& document() const { return document_; }
    InspectionDocument& document() { return document_; }
    bool hasImage() const { return hasImage_; }
    double threshold() const { return threshold_; }
    // 預覽（與有候選的無匹配）時才有。
    const ProposalSession* proposal() const { return proposal_ ? &*proposal_ : nullptr; }
    const std::optional<NoMatchInfo>& noMatch() const { return noMatch_; }
    const std::optional<Error>& error() const { return error_; }
    ErrorSource errorSource() const { return errorSource_; }
    // 最近一次搜尋的區域；DrawingRegion、Searching、Preview、NoMatch、Error 時有。
    const std::optional<SearchTarget>& lastSearch() const { return lastSearch_; }
    const std::vector<int>& selection() const { return selection_; }
    const Config& config() const { return config_; }

    // ---- 影像 ----
    // 開啟新影像：清掉板子、提案、選取與歷史，回到 Idle（任何狀態都收；畫到一半的區域與類型編輯會先放棄）。
    void imageOpened(std::string name, int width, int height, double umPerPixel);
    // 影像打不開：進 Error。目前狀態不收這個事件（例如搜尋中）時回 false，呼叫端改用吐司。
    bool imageFailed(Error error);

    // ---- 區域 ----
    bool beginRegion(RegionMode mode);  // 在畫布上按下
    enum class Release { Ignored, TooSmall, Search };
    // 放開。TooSmall：回到進入前的狀態。Search：進 Searching，Presenter 要開始工作（lastSearch() 是要搜尋的區域）。
    Release endRegion(const Rect& region, RegionMode mode);
    bool abortRegion();  // 拖曳中按 Esc

    // ---- 搜尋 ----
    // 工作完成。非 Searching 時回 false（已取消或被取代的結果，丟掉）。
    // 有可用結果 → Preview；沒有（樣本問題、只有一個、門檻太高）→ NoMatch。
    bool searchSucceeded(std::shared_ptr<const SampleProposal> proposal);
    // 流程 B：沒有候選的結果（例如樣本庫是空的）。
    bool searchEmpty(NoMatchInfo info);
    bool searchFailed(Error error);
    bool cancelSearch();   // Searching → Idle
    // 需要重新搜尋的參數變了：Searching、Preview、NoMatch → Searching。沒有上次的區域時不動。舊的提案留著，直到新結果取代它。
    bool restartSearch();
    bool retrySearch();    // Error → Searching（上次的區域）
    bool dismiss();        // NoMatch、Error → Idle（影像開啟失敗且沒有舊影像時 → Empty）

    // ---- 門檻（不進堆疊）----
    // 夾到 [thresholdMin, thresholdMax]。Preview、NoMatch 時重新過濾快取的候選，不搜尋；其餘狀態只記下數值。
    void setThreshold(double threshold);

    // ---- 提案 ----
    // 接受：回傳接受的元件數；不在預覽中回 nullopt；沒有任何可接受的物件回 0（留在預覽）。
    std::optional<std::size_t> accept();
    bool discard();

    // ---- 選取與編輯（Idle 作用在文件，Preview 作用在提案）----
    void select(const std::vector<int>& ids);
    std::size_t deleteSelection();
    enum class AddResult { Added, NeedType, NotAllowed };
    // 補一個：Idle 用所選元件的類型，沒有就用最近一次的類型；Preview 補一個提案物件。
    AddResult addManual(const Pose2D& pose);
    std::size_t moveSelection(Point2D delta, bool allowMerge = false);

    // ---- 歷史（作用的堆疊依狀態，ADR-002 規則 5）----
    // Preview：提案堆疊（不會越過去動文件）；Idle：文件堆疊（復原「接受／捨棄」會回到 Preview）；其餘狀態忽略。
    bool undo();
    bool redo();
    std::optional<CommandInfo> undoInfo() const;
    std::optional<CommandInfo> redoInfo() const;

private:
    void enterIdleCleanup();
    void pruneSelection();
    void clearProposal();
    bool previewAcceptsEdits() const { return state() == InteractionState::Preview && proposal_.has_value(); }

    Config config_;
    InteractionStateMachine machine_;
    InspectionDocument document_;
    std::optional<ProposalSession> proposal_;
    std::optional<NoMatchInfo> noMatch_;
    std::optional<Error> error_;
    ErrorSource errorSource_ = ErrorSource::Search;
    std::optional<SearchTarget> lastSearch_;
    std::vector<int> selection_;
    double threshold_;
    bool hasImage_ = false;
};

}  // namespace aoi
