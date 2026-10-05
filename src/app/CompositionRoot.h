#pragma once

#include <memory>
#include <string_view>

#include "application/Result.h"
#include "application/backend/BackendRegistry.h"
#include "application/jobs/JobRunner.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "infrastructure/perf/PerfProbe.h"
#include "infrastructure/threading/ThreadPoolExecutor.h"

// app 層：組裝根。這裡是整個程式唯一同時認識 application 與所有 adapter 的地方（依賴反轉：use case 只認識 port）。
namespace aoi::app {

// 註冊所有 back end：可用的與不可用的（附原因）。新增一個 back end ＝ 在這裡加一行，其他地方都不必改。
// 不可用的 back end 也要登記，這樣 --backend=halcon 與 UI 的選單才能給出明確的訊息。
BackendRegistry makeBackendRegistry();

// 組裝好的核心。成員的宣告順序就是建立順序，解構從最後一個往前：
// runner 先停（等進行中的工作結束），再來是 use case、back end、executor、probe。
struct CoreServices {
    std::unique_ptr<infra::PerfProbe> probe;  // 預設關閉
    std::unique_ptr<infra::ThreadPoolExecutor> executor;
    Backend backend;
    std::unique_ptr<GenerateObjectsFromSample> generateObjects;
    std::unique_ptr<JobRunner> runner;
};

// 依 id 建立 back end 並接上 use case、JobRunner。id 不存在或不可用時回 Error（訊息說明原因與可用的 back end）。
Result<CoreServices> composeCore(const BackendRegistry& registry, std::string_view backendId, bool perfEnabled = false);

}  // namespace aoi::app
