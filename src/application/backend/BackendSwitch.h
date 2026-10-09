#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "application/Error.h"
#include "application/backend/BackendRegistry.h"
#include "application/interaction/InteractionStateMachine.h"

// application 層：執行中切換 back end 的規則（ADR-021）。只用標準庫，沒有 Qt（ADR-011：規則放在 application 層，
// 用決定性的單元測試逐項驗證；Presenter 只負責 Qt 這一側：背景執行緒、計時、畫面）。
namespace aoi {

// 現在用哪一個、正在換成哪一個，以及「這個要求要不要做」「做完之後搜尋怎麼辦」。
//
// 建立 back end 可能很慢（HALCON 第一次約 1.5 秒，授權檢查），所以由呼叫端放到背景執行：
//   request(id) 回 Start → 呼叫端在背景建立 pending() → 完成時在 UI 執行緒呼叫 finish(id, 成功與否)。
// 同一時間只換一個（換的期間選單是停用的）；失敗時 current() 不變，也就是回到原本的 back end。
// 不執行緒安全：只在 UI 執行緒使用。
class BackendSwitch {
public:
    BackendSwitch(std::vector<BackendInfo> backends, std::string current);

    const std::vector<BackendInfo>& backends() const { return backends_; }
    const std::string& current() const { return current_; }
    const std::string& pending() const { return pending_; }  // 正在換成哪一個；空 ＝ 沒有在換
    bool switching() const { return !pending_.empty(); }
    const BackendInfo* find(std::string_view id) const;

    enum class Request {
        Start,           // 開始換：pending() ＝ id，呼叫端在背景建立它，完成時呼叫 finish
        AlreadyCurrent,  // 就是目前用的：什麼都不做
        Busy,            // 正在換別的：不收
        Unknown,         // 沒有這個 id：error 說明
        Unavailable,     // 登記成不可用（沒編進程式、沒有 runtime）：不必嘗試，error 是登記時的原因
    };
    struct RequestResult {
        Request kind = Request::AlreadyCurrent;
        Error error;  // Unknown、Unavailable 時的原因
    };
    RequestResult request(std::string_view id);

    enum class Outcome {
        Switched,  // 建立成功：current() 換成新的
        Failed,    // 建立失敗：current() 不變（回到原本的 back end）
        Stale,     // 不是正在換的那一個：忽略（不該發生）
    };
    Outcome finish(std::string_view id, bool succeeded);

    // 切換開始時，進行中的搜尋要先作廢（latest-wins：不等舊的工作停下；它的結果回來時被丟掉）。
    // 狀態留在 Searching：切換結束時再用新的（或失敗時用原來的）back end 接著搜尋（researchAfter）。
    static bool pausesSearch(InteractionState state) { return state == InteractionState::Searching; }

    // 切換結束之後，要不要對同一個區域重新搜尋（使用者 2026-10-08 決定：畫面上有提案時用新的 back end 重新搜尋，
    // 和改了角度範圍一樣；舊的提案留在畫面上，直到新的結果取代它）。
    //   Switched：Searching（切換開始時暫停的）、Preview、NoMatch → 是；Error → 是（搜尋失敗的錯誤：用新的 back end 再試一次，
    //     例如錯誤卡片的〔改用 OpenCV〕；影像開不了的錯誤由呼叫端忽略）；其餘 → 否。
    //   Failed：Searching → 是（暫停的搜尋用原來的 back end 接著做）；其餘 → 否（維持原狀）。
    //   Stale → 否。
    static bool researchAfter(Outcome outcome, InteractionState state);

    // 搜尋時 back end 無法使用（錯誤卡片的〔改用 …〕）時建議改用的那一個：可用、不是 current() 的**最後**一個（註冊順序由簡單到完整：
    // fake、opencv、halcon。HALCON 的授權不見了 → 建議 OpenCV，不建議不看影像的 fake）；沒有就是空字串。
    std::string fallback() const;

private:
    std::vector<BackendInfo> backends_;
    std::string current_;
    std::string pending_;
};

// 啟動時用哪一個 back end（使用者 2026-10-08 決定：記住上次）。
struct StartupPlan {
    std::string initial;   // 視窗出現之前同步建立的那一個
    bool strict = false;   // 命令列指定的：initial 失敗就結束程式（結束碼 2），不改用別的
    std::string switchTo;  // 記住的那一個：視窗出現之後在背景換過去（失敗就留在 initial 並顯示原因）；空 ＝ 不換
};

// 規則：
//  - 命令列有指定（--backend）→ 用它，strict（使用者要的是哪一個就是哪一個；S03 起的行為）。
//  - 否則先用 defaultId（快，視窗馬上出現）；有記住的、而且不是 defaultId → 視窗出現之後在背景換過去。
//    記住的 back end 不能用時（例如 dongle 沒插）不結束程式：留在 defaultId，訊息列說原因。
StartupPlan planStartupBackend(const std::optional<std::string>& commandLine, const std::optional<std::string>& remembered,
                               std::string_view defaultId);

}  // namespace aoi
