#pragma once

#include <memory>
#include <string_view>

#include "application/Result.h"
#include "application/backend/BackendRegistry.h"
#include "application/ports/IPerfProbe.h"
#include "application/usecases/GenerateObjectsFromSample.h"

// application 層。
namespace aoi {

// 一個建好的 back end 加上用它的 use case（流程 A）。建好之後不再改變。
//
// 以 shared_ptr<const SearchEngine> 交出去（ADR-021）：Presenter 持有目前的那一份，每一個搜尋工作也持有一份。
// 執行中換 back end 時，Presenter 只換掉自己的那一份；舊的 adapter 等最後一個用它的工作結束才釋放（在那個 worker 上），
// 所以 UI 不必等舊的工作停下（latest-wins，I4），也不會有工作用到已經釋放的 adapter。
struct SearchEngine {
    Backend backend;
    // 參照 backend 的兩個 adapter。adapter 在堆積上（unique_ptr），SearchEngine 本身搬動也不影響參照。
    std::unique_ptr<GenerateObjectsFromSample> generate;
};
using SearchEnginePtr = std::shared_ptr<const SearchEngine>;

// 依 id 建立 back end 並接上 use case。id 不存在、不可用或工廠失敗時回 Error（同 BackendRegistry::create）。
// 可以在 worker 執行緒呼叫（見 BackendRegistry::create 的契約）；HALCON 第一次約 1.5 秒（授權檢查，ADR-017）。
// probe 可以是空指標，要活得比回傳的 SearchEngine 久。
Result<SearchEnginePtr> makeSearchEngine(const BackendRegistry& registry, std::string_view id, IPerfProbe* probe = nullptr);

// use case 的指標，和 engine 共用擁有權：拿著它的人（例如進行中的工作）讓整個 engine（含 adapter）活著。
inline std::shared_ptr<const GenerateObjectsFromSample> useCaseOf(const SearchEnginePtr& engine) {
    if (!engine) return {};
    return std::shared_ptr<const GenerateObjectsFromSample>(engine, engine->generate.get());
}

}  // namespace aoi
