#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "application/interaction/InspectionSession.h"

// application 層：只用標準庫（不含 Qt）。

namespace aoi {

// 操作步驟（2026-10-15，ADR-027）：教一塊板子的五步。「現在做到哪一步」是規則，從 InspectionSession 的狀態與資料推導，
// 不在 Presenter 裡猜（ADR-011）；面板與訊息列只是把它畫出來。
enum class GuideStep {
    OpenImage = 0,  // 開啟一張板子影像
    DrawSample,     // S：在一個元件上框一個樣本（可以框好幾個）
    Search,         // F5：用板子上全部樣本搜尋
    Accept,         // Enter：接受提案
    Refine,         // 微調：B 畫子框、拖把手或用數字改大小、再框樣本、再搜尋
};

inline constexpr int kGuideStepCount = 5;

constexpr std::string_view guideStepName(GuideStep s) noexcept {
    switch (s) {
        case GuideStep::OpenImage: return "OpenImage";
        case GuideStep::DrawSample: return "DrawSample";
        case GuideStep::Search: return "Search";
        case GuideStep::Accept: return "Accept";
        case GuideStep::Refine: return "Refine";
    }
    return "Unknown";
}

struct GuideStatus {
    GuideStep current = GuideStep::OpenImage;
    std::array<bool, kGuideStepCount> done{};  // 依 GuideStep 的順序；Refine 沒有「做完」
    bool busy = false;       // current 正在進行（拖曳中、搜尋中）
    bool attention = false;  // current 要重做或先處理：沒有結果、搜尋失敗、影像打不開

    bool isDone(GuideStep s) const { return done[static_cast<std::size_t>(s)]; }
    friend bool operator==(const GuideStatus& a, const GuideStatus& b) {
        return a.current == b.current && a.done == b.done && a.busy == b.busy && a.attention == b.attention;
    }
    friend bool operator!=(const GuideStatus& a, const GuideStatus& b) { return !(a == b); }
};

// 規則（每一條在 tst_step_guide 驗證）：
//  - 做完的判斷看資料：有影像 → OpenImage；板上有樣本或有自動產生的元件 → DrawSample；在預覽中或有自動產生的元件 → Search；
//    有自動產生的元件（接受過）→ Accept。
//  - 目前這一步看狀態：Empty → OpenImage；DrawingRegion → DrawSample（busy）；Searching → Search（busy）；Preview → Accept；
//    NoMatch → Search（attention）；Error → 影像打不開是 OpenImage、搜尋失敗是 Search（attention）；EditingType → Refine。
//  - Idle：沒有樣本 → DrawSample（接受過 → Refine）；有樣本的類型在板上還沒有自動產生的元件（新樣本、捨棄或取消之後）→ Search；
//    否則 → Refine。
GuideStatus guideStatus(const InspectionSession& session);

}  // namespace aoi
