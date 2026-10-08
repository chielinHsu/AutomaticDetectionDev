#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "application/Result.h"
#include "application/backend/BackendRegistry.h"
#include "application/jobs/JobRunner.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "infrastructure/perf/PerfProbe.h"
#include "application/ports/IExecutor.h"
#include "application/ports/IImageSource.h"
#include "infrastructure/threading/QtThreadPoolExecutor.h"
#include "infrastructure/threading/ThreadPoolExecutor.h"

// app 層：組裝根。這裡是整個程式唯一同時認識 application 與所有 adapter 的地方（依賴反轉：use case 只認識 port）。
namespace aoi::app {

// 背景工作的執行緒數：核心數 − 1（保留一個給 UI 執行緒），至少 1。JobRunner 的 worker、OpenCV 的平行迴圈（ADR-016）
// 與 HALCON 的自動平行化（S08，set_system('thread_num')）都用它。
std::size_t workerThreads();

// 註冊所有 back end：可用的與不可用的（附原因）。新增一個 back end ＝ 在這裡加一行，其他地方都不必改。
// 不可用的 back end 也要登記，這樣 --backend=halcon 與 UI 的選單才能給出明確的訊息。
BackendRegistry makeBackendRegistry();

// 組裝好的核心。成員的宣告順序就是建立順序，解構從最後一個往前：
// runner 先停（等進行中的工作結束），再來是 use case、back end、executor、probe。
// 工作用哪一種執行緒池。兩個都滿足 IExecutor 的契約；UI 程式用 QThreadPool（S04），純標準庫的測試可以用 std::thread 版。
enum class ExecutorKind { QtThreadPool, StdThreads };

struct CoreServices {
    std::unique_ptr<infra::PerfProbe> probe;  // 預設關閉
    std::unique_ptr<IExecutor> executor;
    Backend backend;
    std::unique_ptr<GenerateObjectsFromSample> generateObjects;
    std::unique_ptr<JobRunner> runner;
};

// 影像來源（S06）：Qt（常見格式零複製）在前；OpenCV 編進程式時接在後面，讀 Qt 解不開的格式（這台機器的 Qt 沒有 TIFF 外掛）。
// 順序的理由見 ADR-014。回傳的物件可以同時被多個執行緒使用；請在主執行緒建立（Qt 的影像外掛在這裡載入）。
std::unique_ptr<IImageSource> makeImageSource();

// 依 id 建立 back end 並接上 use case、JobRunner。id 不存在或不可用時回 Error（訊息說明原因與可用的 back end）。
// back end 的提醒（例如 HALCON 用的是 runtime 授權）在 backend.notices（工廠填，application/backend/BackendRegistry.h）。
Result<CoreServices> composeCore(const BackendRegistry& registry, std::string_view backendId, bool perfEnabled = false,
                                 ExecutorKind executorKind = ExecutorKind::QtThreadPool);

}  // namespace aoi::app
