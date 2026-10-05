#pragma once

#include <optional>
#include <string_view>

// application 層：只用標準庫（不含 Qt）。轉移表照 docs/ux/ux-spec.md 第 6 節，S04 的單元測試逐格驗證。

namespace aoi {

enum class InteractionState {
    Empty,          // 還沒有影像
    Idle,           // 有影像，沒有進行中的操作
    DrawingRegion,  // 正在拖出樣本框（S）或掃描區域（R）
    Searching,      // 搜尋進行中
    Preview,        // 有一份提案，等使用者接受或捨棄
    NoMatch,        // 搜尋完成但沒有結果（正常結果，不是故障）
    Error,          // 影像開啟失敗、back end 無法使用、搜尋失敗
    EditingType,    // 元件類型編輯器（流程 C）
};

enum class InteractionEvent {
    ImageOpened,
    ImageFailed,
    PressRegion,      // 在畫布上按下，開始拖出一個區域
    Release,          // 放開，區域夠大：開始搜尋
    ReleaseTooSmall,  // 放開，區域太小
    Esc,              // 拖曳中按 Esc
    ResultFound,
    ResultEmpty,
    Failure,
    Cancel,           // 取消搜尋，或取消類型編輯
    ParamsChanged,    // 需要重新搜尋的參數變了
    ThresholdChanged, // 分數門檻變了（只重新過濾快取的候選，不搜尋）
    Accept,
    Discard,
    Dismiss,
    Retry,
    EditType,
    UndoToPreview,    // 復原了「接受」或「捨棄」，提案回來了
    Save,             // 儲存類型
};

// 轉移的條件。轉移表裡只有三格要看它。
struct TransitionContext {
    bool hasImage = true;   // Error 的 Dismiss：影像開啟失敗且沒有舊影像時回 Empty，否則回 Idle
    bool foundAny = false;  // NoMatch 的 ThresholdChanged：新的門檻下有 ≥ 1 個找到就進 Preview
};

constexpr std::string_view toString(InteractionState s) noexcept {
    switch (s) {
        case InteractionState::Empty: return "Empty";
        case InteractionState::Idle: return "Idle";
        case InteractionState::DrawingRegion: return "DrawingRegion";
        case InteractionState::Searching: return "Searching";
        case InteractionState::Preview: return "Preview";
        case InteractionState::NoMatch: return "NoMatch";
        case InteractionState::Error: return "Error";
        case InteractionState::EditingType: return "EditingType";
    }
    return "Unknown";
}

constexpr std::string_view toString(InteractionEvent e) noexcept {
    switch (e) {
        case InteractionEvent::ImageOpened: return "ImageOpened";
        case InteractionEvent::ImageFailed: return "ImageFailed";
        case InteractionEvent::PressRegion: return "PressRegion";
        case InteractionEvent::Release: return "Release";
        case InteractionEvent::ReleaseTooSmall: return "ReleaseTooSmall";
        case InteractionEvent::Esc: return "Esc";
        case InteractionEvent::ResultFound: return "ResultFound";
        case InteractionEvent::ResultEmpty: return "ResultEmpty";
        case InteractionEvent::Failure: return "Failure";
        case InteractionEvent::Cancel: return "Cancel";
        case InteractionEvent::ParamsChanged: return "ParamsChanged";
        case InteractionEvent::ThresholdChanged: return "ThresholdChanged";
        case InteractionEvent::Accept: return "Accept";
        case InteractionEvent::Discard: return "Discard";
        case InteractionEvent::Dismiss: return "Dismiss";
        case InteractionEvent::Retry: return "Retry";
        case InteractionEvent::EditType: return "EditType";
        case InteractionEvent::UndoToPreview: return "UndoToPreview";
        case InteractionEvent::Save: return "Save";
    }
    return "Unknown";
}

inline constexpr int kInteractionStateCount = 8;
inline constexpr int kInteractionEventCount = 19;

// 手寫的列舉狀態機：沒有 Qt、沒有計時器、沒有副作用，所以每個轉移都能直接測。
// 副作用（畫面、搜尋工作）在 Presenter 與 InspectionSession，它們看轉移的結果。
//
// 沒有列在轉移表裡的 (狀態, 事件) 一律不合法：fire 回 false，狀態不變，呼叫端要忽略這個動作。
class InteractionStateMachine {
public:
    InteractionState state() const noexcept { return state_; }
    // DrawingRegion 進入前的狀態（Idle、Preview 或 NoMatch）；按 Esc 或區域太小時回到它，提案不丟。
    InteractionState stateBeforeDrawing() const noexcept { return before_; }

    // 只問不動：這個事件現在合法嗎？合法就回傳會去的狀態。
    std::optional<InteractionState> peek(InteractionEvent event, TransitionContext ctx = {}) const noexcept;
    bool fire(InteractionEvent event, TransitionContext ctx = {}) noexcept;
    // 開機與測試用：直接設定狀態（不經過轉移表）。
    void reset(InteractionState state = InteractionState::Empty) noexcept {
        state_ = state;
        before_ = InteractionState::Idle;
    }

private:
    InteractionState state_ = InteractionState::Empty;
    InteractionState before_ = InteractionState::Idle;
};

}  // namespace aoi
