#pragma once

#include <memory>
#include <string_view>

#include "application/backend/BackendRegistry.h"
#include "application/ports/ILibraryMatcher.h"
#include "infrastructure/backend_halcon/HalconSearchOptions.h"

// infrastructure/backend_halcon：HALCON back end 的工廠與執行緒政策（S08）。組裝根只認識本檔與 HalconLicense.h、HalconRuntime.h
// （三個都不 include HALCON 的標頭：HALCON 的型別不出 adapter）。
namespace aoi::infra {

inline constexpr std::string_view kHalconId = "halcon";

// 一組配套的 HALCON adapter（範本建立器＋搜尋器，用同一份 options）。組裝根以 id "halcon" 註冊。
// 先確認程式庫載得起來、授權可用、而且含 shape matching（HalconLicense.h；第一次約 1 秒），
// 不行就回 ErrorCode::BackendUnavailable，訊息說原因（找到哪些授權檔、這台電腦有哪些 ID）。程式不當掉。
Result<Backend> makeHalconBackend(const HalconSearchOptions& options = {});

// 流程 B 的比對器（S08；和 OpenCV 版一樣，UI 還沒接，Backend 結構也沒有這個欄位）。呼叫前要先確認 makeHalconBackend 成功過。
std::unique_ptr<ILibraryMatcher> makeHalconLibraryMatcher(const HalconSearchOptions& options = {});

// 執行緒政策（S08，和 OpenCV 版相同的理由，ADR-016）：set_system('thread_num', threads)，HALCON 自動平行化的執行緒數
// （含呼叫的執行緒）。組裝根傳入「核心數 − 1」，和 JobRunner 的 worker 數相同。這是整個程式共用的設定，而且是「獨占設定」
// （說明：會等其他執行緒的運算子做完），所以只在建立 back end 時設一次。
// threads ≤ 0 時不改。回傳設定之後 get_system('thread_num') 的值；程式庫不可用時回 0、不呼叫 HALCON。
int setHalconThreads(int threads);

}  // namespace aoi::infra
