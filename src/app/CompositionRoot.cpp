#include "app/CompositionRoot.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "application/image/ImageSourceChain.h"
#include "infrastructure/backend_fake/FakeBackend.h"
#include "infrastructure/image_qt/QtImageSource.h"
#ifdef AOI_WITH_OPENCV
#include "infrastructure/backend_opencv/OpenCvBackend.h"
#include "infrastructure/backend_opencv/OpenCvImageSource.h"
#endif
#ifdef AOI_WITH_HALCON
#include "infrastructure/backend_halcon/HalconBackend.h"
#include "infrastructure/backend_halcon/HalconLicense.h"
#include "infrastructure/backend_halcon/HalconRuntime.h"
#endif

namespace aoi::app {

namespace {

#ifdef AOI_WITH_HALCON
// HALCON 能用、但授權種類要提醒的事（S08 的規則，judgeHalconLicense）→ 給畫面與 stderr 的提醒。
std::vector<BackendNotice> halconNotices() {
    std::vector<BackendNotice> notices;
    for (const infra::HalconLicenseNotice& n : infra::judgeHalconLicense(infra::halconLicense()).notices) {
        if (n.kind == infra::HalconLicenseNoticeKind::SdkDongleMissing) {
            notices.push_back({"ui.backend.halcon.sdkDongleMissing", "ui.backend.halcon.runtimeLicense",
                               {{"inUse", n.inUseId}, {"missing", n.missingId}}, n.message});
        }
    }
    return notices;
}
#endif

std::string availableList(const BackendRegistry& registry) {
    std::string list;
    for (const BackendInfo& info : registry.list()) {
        if (!info.available) continue;
        if (!list.empty()) list += ", ";
        list += info.id;
    }
    return list.empty() ? std::string("(none)") : list;
}

}  // namespace

std::size_t workerThreads() {
    const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
    return std::max(1u, hardware - 1);
}

BackendRegistry makeBackendRegistry() {
    BackendRegistry registry;
    // 註冊只在 id 重複時才會失敗，而這裡的 id 是寫死的，所以結果可以忽略。
    (void)registry.registerBackend(std::string(fake::kBackendId), "Fake (synthetic grid)",
                                   [] { return Result<Backend>(fake::makeFakeBackend()); });
#ifdef AOI_WITH_OPENCV
    // S07：OpenCV 的執行緒數和 JobRunner 的 worker 數相同（核心數 − 1，保留一個給 UI；ADR-016）。建立時才設定，
    // 所以只用 fake 的時候不會動到 OpenCV 的全域設定。執行中切換 back end（ADR-021）會再建立：只設第一次——
    // 數字每次都一樣，而且舊的 OpenCV 工作可能還在平行迴圈裡（latest-wins 不等它停下），不要在那時重設全域的執行緒池。
    (void)registry.registerBackend("opencv", "OpenCV", [] {
        static std::once_flag threadsSet;
        std::call_once(threadsSet, [] { infra::setOpenCvThreads(static_cast<int>(workerThreads())); });
        return Result<Backend>(infra::makeOpenCvBackend());
    });
#else
    (void)registry.registerUnavailable("opencv", "OpenCV",
                                       "The OpenCV back end is not built into this program. Rebuild with AOI_WITH_OPENCV=ON "
                                       "(qmake AiInspTool.pro; it is on by default).");
#endif
#ifdef AOI_WITH_HALCON
    // S08：halconcpp.dll 是延遲載入的。列 back end 時只看 PATH 上找不找得到它（微秒級）；授權檢查（約 1 秒）等真的選了 halcon 才做，
    // 失敗時工廠回 BackendUnavailable（原因寫清楚），程式不當掉。執行緒數和 JobRunner 的 worker 數相同（OpenCV 也是）。
    // 授權要提醒的事（runtime 授權、SDK 的 dongle 沒插）跟著 back end 交出去（Backend::notices）。
    // 執行中切換時工廠在 worker 上跑（ADR-021）：授權的探測是執行緒安全的（halconLicense）；執行緒數只在第一次成功時設
    // （同 OpenCV 的理由；set_system 是獨占設定，會等其他執行緒的運算子）。
    if (infra::halconRuntimeOnPath()) {
        (void)registry.registerBackend("halcon", "HALCON", [] {
            Result<Backend> backend = infra::makeHalconBackend();
            if (backend) {
                static std::once_flag threadsSet;
                std::call_once(threadsSet, [] { infra::setHalconThreads(static_cast<int>(workerThreads())); });
                backend.value().notices = halconNotices();
            }
            return backend;
        });
    } else {
        (void)registry.registerUnavailable("halcon", "HALCON",
                                           "The HALCON runtime (halconcpp.dll) is not installed or not on the PATH. Install HALCON " +
                                               infra::halconSdkVersion() + " and its license, or use another back end.");
    }
#else
    (void)registry.registerUnavailable("halcon", "HALCON",
                                       "The HALCON back end is not built into this program. Rebuild with AOI_WITH_HALCON=ON "
                                       "(qmake AiInspTool.pro; it is on by default).");
#endif
    return registry;
}

std::unique_ptr<IImageSource> makeImageSource() {
    auto chain = std::make_unique<ImageSourceChain>();
    chain->add(std::make_unique<infra::QtImageSource>());
#ifdef AOI_WITH_OPENCV
    chain->add(std::make_unique<infra::OpenCvImageSource>());
#endif
    return chain;
}

Result<CoreServices> composeCore(const BackendRegistry& registry, std::string_view backendId, bool perfEnabled,
                                 ExecutorKind executorKind) {
    auto probe = std::make_unique<infra::PerfProbe>(65536, perfEnabled);
    Result<SearchEnginePtr> engine = makeSearchEngine(registry, backendId, probe.get());
    if (!engine) {
        Error error = engine.error();
        error.message += " Available back ends: " + availableList(registry) + ".";
        return error;
    }

    CoreServices core;
    core.probe = std::move(probe);
    // 保留一個核心給 UI 執行緒；至少 1 個 worker。OpenCV 的平行迴圈用同一個數字（S07，ADR-016）。
    const std::size_t workers = workerThreads();
    if (executorKind == ExecutorKind::QtThreadPool) core.executor = std::make_unique<infra::QtThreadPoolExecutor>(workers);
    else core.executor = std::make_unique<infra::ThreadPoolExecutor>(workers);
    core.engine = std::move(engine.value());
    core.runner = std::make_unique<JobRunner>(*core.executor, core.probe.get());
    return Result<CoreServices>(std::move(core));
}

}  // namespace aoi::app
