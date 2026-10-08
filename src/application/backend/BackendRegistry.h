#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "application/Result.h"
#include "application/ports/IPatternSearcher.h"
#include "application/ports/ITemplateBuilder.h"

// application 層。
//
// 開放封閉：新增一個 back end ＝ 寫一組 adapter ＋ 在組裝根（app/）註冊一次；use case、domain、這個類別都不必改。
// 沒有編進程式（缺函式庫）或沒有授權的 back end 也要註冊成「不可用」並附原因，
// 這樣 --backend=halcon 與 UI 的 back end 選單都能給出明確的訊息，而不是找不到 id 就當沒這回事。
namespace aoi {

// back end 可以用、但使用者應該知道的事（S08）。例：HALCON 用的是 runtime 授權，開發（SDK）授權的 dongle 沒插。
// 文案由 presentation 依 key 從 copy.json 取（args 是 {名稱} 佔位字的值）：textId 是完整說明（訊息列的提醒卡片、
// 狀態列的提示），markId 是狀態列 back end 名稱後面的短標示（「runtime 授權」，可以是空的）。
// message 是英文說明（stderr、log、測試）。
struct BackendNotice {
    std::string textId;
    std::string markId;
    std::vector<std::pair<std::string, std::string>> args;
    std::string message;
};

// 一個可用的 back end：一組互相配套的 adapter（範本與搜尋器要同一家，它們可以共用快取等內部狀態）。
struct Backend {
    std::string id;
    std::string displayName;
    std::unique_ptr<ITemplateBuilder> templateBuilder;
    std::unique_ptr<IPatternSearcher> searcher;
    std::vector<BackendNotice> notices;  // 建立時發現的提醒（工廠填）；沒有 ＝ 空
};

struct BackendInfo {
    std::string id;
    std::string displayName;
    bool available = false;
    Error unavailable;  // available ＝ false 時的原因（code 是 BackendUnavailable）；否則內容沒有意義
};

// 契約：registerBackend 與 registerUnavailable 在 id 重複時回 ErrorCode::InvalidArgument（不靜默覆蓋）。
// create 在 id 不存在時回 ErrorCode::NotFound；id 是「不可用」的 back end 時回它註冊時的 Error；
// 工廠失敗時回工廠的 Error。每次 create 都建一份新的 Backend（執行緒之間不共享 adapter）。
// 執行緒：註冊只在啟動時（主執行緒），註冊完之後不再改。之後 list 與 create 可以從任何執行緒呼叫
// （執行中切換 back end 時，create 在 worker 上跑，ADR-021），所以工廠本身要能在任何執行緒執行。
class BackendRegistry {
public:
    using Factory = std::function<Result<Backend>()>;

    Result<void> registerBackend(std::string id, std::string displayName, Factory factory);
    Result<void> registerUnavailable(std::string id, std::string displayName, std::string reason);

    // 依註冊順序。
    std::vector<BackendInfo> list() const;
    Result<Backend> create(std::string_view id) const;

private:
    struct Entry {
        BackendInfo info;
        Factory factory;
    };
    std::vector<Entry> entries_;
};

}  // namespace aoi
