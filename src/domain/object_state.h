#pragma once

#include <cstddef>
#include <optional>

// domain 層：只用 C++ 標準庫。
namespace aoi {

// 元件（與它的全部框）的比對狀態；視覺樣式見 docs/ux/ux-spec.md 第 7 節。
// presentation 有自己的 OverlayState（view 型別），Presenter（S04）負責對應。
enum class ObjectState {
    Sample,    // 流程 A：使用者框的那一個元件
    Found,     // 比對分數 ≥ 門檻；手動補的元件也是 Found（沒有分數）
    Inferred,  // 晶格預測這裡有元件，影像上沒有找到（可能缺件，要人看）
    Rejected,  // 候選分數 < 門檻
};

// 元件是自動辨識還是人手補的。手動補的元件沒有分數，不可以偽造一個（ux-spec §12.1 第 5 項）。
enum class ObjectOrigin { Auto, Manual };

// 搜尋沒有結果的原因（docs/ux/ux-spec.md 第 10 節的表，前 11 項再加 ScopeBoardOnly）。
// 沒有結果是正常結果，不是錯誤：use case 以 NoMatchInfo 回報，不用 Error。
enum class NoMatchReason {
    SampleTooSmall,    // 樣本邊長 < 8 像素
    SampleTooLarge,    // 樣本比元件大很多
    SampleCoversMany,  // 樣本框到不只一個元件（建議改用區域掃描）
    SampleOffPart,     // 樣本沒框到元件
    OnlyOne,           // 板上只有這一個相似元件
    LowContrast,       // 樣本的對比太低
    ThresholdTooHigh,  // 有候選但都低於門檻
    LibraryEmpty,      // 樣本庫是空的（流程 B）
    RegionEmpty,       // 區域內沒有元件（流程 B）
    NoLibraryMatch,    // 區域內有元件，但庫裡沒有符合的（流程 B）
    AlreadyOnBoard,    // 區域內的元件都已在板上（流程 B）
    ScopeBoardOnly,    // 比對範圍只有板子已有的類型，但元件可能符合庫裡其他樣本（流程 B）
};

struct NoMatchInfo {
    NoMatchReason reason = NoMatchReason::OnlyOne;
    // 有候選時的最高分（0..1）；沒有候選時是 0。
    double highestScore = 0.0;
    // 「降低門檻到 {suggested}」用的建議值；只有 ThresholdTooHigh 有意義，其餘是 0。
    double suggestedThreshold = 0.0;
    // 「區域內有元件但…」的元件數（流程 B 的文案要用）；流程 A 是 0。
    std::size_t componentCount = 0;
};

}  // namespace aoi
