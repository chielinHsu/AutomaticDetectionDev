#include "app/CompositionRoot.h"

#include <algorithm>
#include <string>
#include <thread>
#include <utility>

#include "application/image/ImageSourceChain.h"
#include "infrastructure/backend_fake/FakeBackend.h"
#include "infrastructure/image_qt/QtImageSource.h"
#ifdef AOI_WITH_OPENCV
#include "infrastructure/backend_opencv/OpenCvImageSource.h"
#endif

namespace aoi::app {

namespace {

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

BackendRegistry makeBackendRegistry() {
    BackendRegistry registry;
    // 註冊只在 id 重複時才會失敗，而這裡的 id 是寫死的，所以結果可以忽略。
    (void)registry.registerBackend(std::string(fake::kBackendId), "Fake (synthetic grid)",
                                   [] { return Result<Backend>(fake::makeFakeBackend()); });
    // S07、S08 把下面兩行換成真的 registerBackend(...)；只改這裡，use case 與 domain 不動。
    (void)registry.registerUnavailable("opencv", "OpenCV",
                                       "The OpenCV back end is not built into this program yet. It arrives in step S07.");
    (void)registry.registerUnavailable("halcon", "HALCON",
                                       "The HALCON back end is not built into this program yet. It arrives in step S08.");
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
    Result<Backend> backend = registry.create(backendId);
    if (!backend) {
        Error error = backend.error();
        error.message += " Available back ends: " + availableList(registry) + ".";
        return error;
    }

    CoreServices core;
    core.probe = std::make_unique<infra::PerfProbe>(65536, perfEnabled);
    // 保留一個核心給 UI 執行緒；至少 1 個 worker。這是起點，S07 與 OpenCV 自己的執行緒協調後再校準。
    const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
    const std::size_t workers = std::max(1u, hardware - 1);
    if (executorKind == ExecutorKind::QtThreadPool) core.executor = std::make_unique<infra::QtThreadPoolExecutor>(workers);
    else core.executor = std::make_unique<infra::ThreadPoolExecutor>(workers);
    core.backend = std::move(backend.value());
    core.generateObjects = std::make_unique<GenerateObjectsFromSample>(*core.backend.templateBuilder,
                                                                       *core.backend.searcher, core.probe.get());
    core.runner = std::make_unique<JobRunner>(*core.executor, core.probe.get());
    return Result<CoreServices>(std::move(core));
}

}  // namespace aoi::app
