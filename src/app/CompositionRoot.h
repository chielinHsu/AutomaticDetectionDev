#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "application/Result.h"
#include "application/backend/BackendRegistry.h"
#include "application/backend/SearchEngine.h"
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

// 工作用哪一種執行緒池。兩個都滿足 IExecutor 的契約；UI 程式用 QThreadPool（S04），純標準庫的測試可以用 std::thread 版。
enum class ExecutorKind { QtThreadPool, StdThreads };

// 組裝好的核心。成員的宣告順序就是建立順序，解構從最後一個往前：
// runner 先停（等進行中的工作結束），再來是 engine（back end ＋ use case）、executor（排著的工作做完）、probe。
// 執行中切換 back end（ADR-021）只換 engine：executor、JobRunner、probe 不重建。engine 是共用的：Presenter 與每個搜尋工作
// 各持有一份，所以這裡的 engine 只是「啟動時的那一個」，換過之後它可能已經不是目前的 back end。
struct CoreServices {
    std::unique_ptr<infra::PerfProbe> probe;  // 預設關閉
    std::unique_ptr<IExecutor> executor;
    SearchEnginePtr engine;  // engine->backend（id、adapter、提醒）、engine->generate（流程 A 的 use case）
    std::unique_ptr<JobRunner> runner;
};

// 影像來源（S06）：Qt（常見格式零複製）在前；OpenCV 編進程式時接在後面，讀 Qt 解不開的格式（這台機器的 Qt 沒有 TIFF 外掛）。
// 順序的理由見 ADR-014。回傳的物件可以同時被多個執行緒使用；請在主執行緒建立（Qt 的影像外掛在這裡載入）。
std::unique_ptr<IImageSource> makeImageSource();

// ---- HALCON 的授權在另一個行程檢查（2026-10-09）----
// HALCON 在一個行程裡第一次初始化失敗之後，可能不會再讀授權（找不到授權檔的錯誤 2036 實測會一直留著）。為了「插上 dongle、
// 再切一次就好」不必重新啟動程式，這個程式碰 HALCON 之前，先用另一個行程（AiInspTool.exe --probe-halcon-license，和這個程式
// 同一個資料夾）確認授權可以用；不能用就不碰這個行程的 HALCON。代價：第一次用 HALCON 多約 1.2 秒（在背景執行緒）。
inline constexpr const char* kProbeHalconLicenseOption = "--probe-halcon-license";

struct HalconLicenseCheck {
    bool ran = false;     // 另一個行程有跑完（找不到 AiInspTool.exe、逾時、當掉 ＝ false：沒有結論，照舊在這個行程試）
    bool usable = false;
    std::string reason;   // usable ＝ false 時的原因（judgeHalconLicense，英文）
};

// 用另一個行程檢查（QProcess，阻塞，最多 30 秒；可以在 worker 執行緒呼叫）。沒有 QCoreApplication 或找不到程式時 ran ＝ false。
HalconLicenseCheck checkHalconLicenseInSeparateProcess();

// --probe-halcon-license 的本體：在這個行程探測一次，結果寫到 stdout（第一行 usable／unusable，第二行原因）。回傳結束碼（0 ＝ 可以用）。
int runHalconLicenseProbe();

// 建立 HALCON back end 的規則（純粹的流程，三個動作都可以換成假的測）：
//  - 這個行程還沒碰過 HALCON：先問另一個行程。確定不能用 → 回它的原因，不碰這個行程的 HALCON；能用或沒有結論 → 在這個行程建立。
//  - 已經碰過：在這個行程建立（失敗過的話，授權會重新檢查，HalconLicenseCache）。仍然沒有授權、但另一個行程拿得到 → 這個行程的
//    HALCON 卡在第一次的失敗：原因說要重新啟動程式。
Result<Backend> createHalconBackendSafely(const std::function<HalconLicenseCheck()>& checkInSeparateProcess,
                                          const std::function<bool()>& probedInProcess, const std::function<Result<Backend>()>& create);

// 依 id 建立 back end 並接上 use case、JobRunner。id 不存在或不可用時回 Error（訊息說明原因與可用的 back end）。
// back end 的提醒（例如 HALCON 用的是 runtime 授權）在 engine->backend.notices（工廠填，application/backend/BackendRegistry.h）。
// 執行中要換成別的 back end 時用 makeSearchEngine（application/backend/SearchEngine.h），在背景執行緒建立。
Result<CoreServices> composeCore(const BackendRegistry& registry, std::string_view backendId, bool perfEnabled = false,
                                 ExecutorKind executorKind = ExecutorKind::QtThreadPool);

}  // namespace aoi::app
