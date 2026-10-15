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

// 使用者最近一次要搜尋的東西。重新搜尋（參數變了）與再試一次都用它。
//  - Sample（2026-10-14 起，ADR-025）：用板子上的**全部樣本**搜尋整張板子；samples 是按下〔搜尋〕時板子上的樣本，
//    region 是第一個樣本的框（A/B 比較與畫面用）。
//  - Region（流程 B）：使用者框的區域。
struct SearchTarget {
    Rect region;
    RegionMode mode = RegionMode::Sample;
    std::vector<SampleOnBoard> samples;
};

enum class ErrorSource { Image, Search };

// 選取資訊（右側 dock 的「選取資訊」表單，ux-spec §4）：選了幾個；恰好一個時它的名稱、類型、位姿、主體框、框數，
// 以及**現在**能不能改位置、改大小。規則在這裡，不在 Presenter（ADR-011）：
//  - Idle（板子上的元件）：可以改位置（moveSelection）與大小（resizeBodyKeepingCenter：改類型、同類型一起）。
//  - Preview（提案的物件）：只能改位置；改大小是文件堆疊上的事，預覽時不做（ADR-025 §4）。
//  - 多選或沒有選取：count 不是 1，其餘欄位沒有意義。
struct SelectionInfo {
    std::size_t count = 0;     // 選了幾個（0 ＝ 沒有選取）
    int id = 0;                // 文件的元件編號，或提案物件的編號（proposal ＝ true 時）
    bool proposal = false;
    std::string name;          // 提案物件沒有名稱（空字串）
    std::string typeId;
    Pose2D pose;               // 影像像素
    Size2D bodyPx;             // 主體框（影像像素）
    std::size_t boxCount = 0;  // 類型展開的框數（含主體）
    bool canMove = false;
    bool canResize = false;
    // 晶格（ADR-026；提案的物件才有意義，見 ProposalSession::LatticeRelation）：是不是晶格的成員、使用者有沒有移過、
    // 現在的位置是不是格點；匹配位置離最近格點多遠（不在任何陣列範圍內的沒有）。
    bool inLattice = false;
    bool moved = false;
    bool onLattice = false;
    std::optional<double> latticeDeviationPx;
};

// 互動狀態的唯一擁有者：狀態機、已接受的文件、目前的提案、選取。
//
// 為什麼放在 application 層：「按 Enter 之後什麼會發生」「Ctrl+Z 作用在哪個堆疊」「刪除作用在提案還是文件」是規則，
// 不是畫面。規則寫在沒有 Qt 的類別裡，就能用決定性的單元測試逐項驗證；Presenter（Qt）只負責把使用者的動作交給它、
// 把結果畫出來，以及處理執行緒與計時。
//
// 2026-10-14 起（ADR-025）：S 工具放開＝**建立樣本**（文件堆疊上的 Command，回到 Idle），搜尋是另一個動作（startSearch：
// 用板子上全部樣本），接受＝上一次自動產生的元件整批換成這一次的。
//
// 不執行緒安全：只在 UI 執行緒使用。搜尋在 worker 上跑，結果以不可變的 BoardProposal 交回來（searchSucceeded）。
class InspectionSession {
public:
    struct Config {
        double minSamplePx = 8.0;        // 樣本邊長小於這個值不建立（ux-spec 第 8 節「最小樣本」）
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
    // 最近一次搜尋的目標；Searching、Preview、NoMatch、Error 時有。
    const std::optional<SearchTarget>& lastSearch() const { return lastSearch_; }
    const std::vector<int>& selection() const { return selection_; }
    const Config& config() const { return config_; }
    // 板子上有沒有樣本可以搜尋。
    bool hasSamples() const { return !document_.samples().empty(); }

    // ---- 影像 ----
    // 開啟新影像：清掉板子、提案、選取與歷史，回到 Idle（任何狀態都收；畫到一半的區域與類型編輯會先放棄）。
    void imageOpened(std::string name, int width, int height, double umPerPixel);
    // 影像打不開：進 Error。目前狀態不收這個事件（例如搜尋中）時回 false，呼叫端改用吐司。
    bool imageFailed(Error error);

    // ---- 區域 ----
    bool beginRegion(RegionMode mode);  // 在畫布上按下
    enum class Release { Ignored, TooSmall, Search, SampleAdded };
    // 放開。TooSmall：回到進入前的狀態。Sample：建立樣本（Command）、回到 Idle、選取它（SampleAdded）；舊的提案丟掉。
    // Region：進 Searching（Search），Presenter 要開始工作（lastSearch() 是要搜尋的區域）。
    Release endRegion(const Rect& region, RegionMode mode);
    bool abortRegion();  // 拖曳中按 Esc

    // ---- 搜尋 ----
    // 〔搜尋〕：用板子上全部樣本搜尋（Idle、Preview、NoMatch → Searching）。沒有樣本、或目前狀態不收時回 false。
    // 舊的提案留著，直到新結果取代它（和 restartSearch 一樣）；lastSearch() 帶著要搜尋的樣本。
    bool startSearch();
    // 工作完成。非 Searching 時回 false（已取消或被取代的結果，丟掉）。
    // 有可用結果 → Preview；沒有（樣本問題、只有一個、門檻太高）→ NoMatch。
    bool searchSucceeded(std::shared_ptr<const BoardProposal> proposal);
    // 流程 B：沒有候選的結果（例如樣本庫是空的）。
    bool searchEmpty(NoMatchInfo info);
    bool searchFailed(Error error);
    bool cancelSearch();   // Searching → Idle
    // 需要重新搜尋的參數變了：Searching、Preview、NoMatch → Searching。沒有上次的目標時不動（Sample 模式還要板子上仍有樣本）。
    // 舊的提案留著，直到新結果取代它。
    bool restartSearch();
    bool retrySearch();    // Error → Searching（上次的目標）
    bool dismiss();        // NoMatch、Error → Idle（影像開啟失敗且沒有舊影像時 → Empty）

    // ---- 門檻（不進堆疊）----
    // 夾到 [thresholdMin, thresholdMax]。Preview、NoMatch 時重新過濾快取的候選，不搜尋；其餘狀態只記下數值。
    void setThreshold(double threshold);
    // ---- 位置模式（ADR-026；和門檻一樣不進堆疊）----
    // Lattice：候選有晶格位姿的用格點（CAD 的理論位置）；Matched：一律用 back end 的匹配位置。Preview、NoMatch 時重新分類
    // 快取的候選（不搜尋、編號不變）；其餘狀態只記下來給下一份提案。已經接受到板子上的元件不動。
    void setPositionMode(PositionMode mode);
    PositionMode positionMode() const { return positionMode_; }

    // ---- 提案 ----
    // 接受：回傳接受的元件數；不在預覽中回 nullopt；沒有任何可接受的物件回 0（留在預覽）。
    std::optional<std::size_t> accept();
    bool discard();

    // ---- 選取與編輯（Idle 作用在文件，Preview 作用在提案）----
    void select(const std::vector<int>& ids);
    // 全選、選同類型（所選元件的類型；沒有選取時不動）。Idle 作用在文件，Preview 作用在提案。回傳選了幾個。
    std::size_t selectAll();
    std::size_t selectSameType();
    std::size_t deleteSelection();
    enum class AddResult { Added, NeedType, NotAllowed };
    // 補一個：Idle 用所選元件的類型，沒有就用最近一次的類型；Preview 用所選物件的類型，沒有就用第一個候選的。
    AddResult addManual(const Pose2D& pose);
    std::size_t moveSelection(Point2D delta, bool allowMerge = false);
    // 改主體框（ADR-025）：Idle 時把 componentId 的類型的主體框改成 rect 的大小（同類型全部元件一起變），這個元件的中心移到
    // rect 的中心（角度不變）。其他狀態、元件不存在、框太小（minSamplePx）回 false。
    bool resizeBody(int componentId, const Rect& rectPx);
    // 同上，但中心不動（選取資訊表單輸入寬高）：規則和 resizeBody 相同。
    bool resizeBodyKeepingCenter(int componentId, Size2D sizePx);
    // 〔套用大小到所有樣本〕：把 sizePx 套到板子上每一個樣本的類型（InspectionDocument::setBodySizeForTypes，一個 Command）。
    // 只在 Idle、邊長 ≥ minSamplePx；回傳改了幾個類型（0 ＝ 沒有改：狀態不對、太小、或全部已經是這個大小）。
    std::size_t applyBodySizeToSamples(Size2D sizePx);
    // 選取資訊（見 SelectionInfo）。
    SelectionInfo selectionInfo() const;
    // 子框（ADR-025，B 工具）：Idle 時在 componentId 的類型上加一個 Pin 1×1 的群組，位置與大小由 rectPx（影像像素）換成
    // 類型座標（µm、相對主體中心、反旋轉）。同類型全部元件一起多出這個框。其他狀態、元件不存在、框不合法回 false。
    bool addBoxGroup(int componentId, const Rect& rectPx);

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
    // 用板子上現在的樣本填 lastSearch_（Sample 模式）；沒有樣本回 false。
    bool refreshSampleTarget();

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
    PositionMode positionMode_ = PositionMode::Lattice;
    bool hasImage_ = false;
};

}  // namespace aoi
