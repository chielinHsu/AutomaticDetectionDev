#include "infrastructure/backend_halcon/HalconBackend.h"

#include <halconcpp/HalconCpp.h>

#include <string>

#include "infrastructure/backend_halcon/HalconLibraryMatcher.h"
#include "infrastructure/backend_halcon/HalconLicense.h"
#include "infrastructure/backend_halcon/HalconPatternSearcher.h"
#include "infrastructure/backend_halcon/HalconRuntime.h"
#include "infrastructure/backend_halcon/HalconTemplateBuilder.h"

namespace aoi::infra {

static_assert(kHalconId == kHalconBackendId);

Result<Backend> makeHalconBackend(const HalconSearchOptions& options) {
    const HalconLicenseVerdict verdict = judgeHalconLicense(halconLicense());
    if (!verdict.usable) return makeError(ErrorCode::BackendUnavailable, verdict.reason);
    Backend backend;
    backend.id = std::string(kHalconBackendId);
    backend.displayName = "HALCON";
    backend.templateBuilder = std::make_unique<HalconTemplateBuilder>(options);
    backend.searcher = std::make_unique<HalconPatternSearcher>(options);
    return backend;
}

std::unique_ptr<ILibraryMatcher> makeHalconLibraryMatcher(const HalconSearchOptions& options) {
    return std::make_unique<HalconLibraryMatcher>(options);
}

int setHalconThreads(int threads) {
    if (halconRuntimeProblem()) return 0;
    try {
        if (threads > 0) HalconCpp::SetSystem("thread_num", threads);
        HalconCpp::HTuple value;
        HalconCpp::GetSystem("thread_num", &value);
        return value.Length() > 0 ? static_cast<int>(value[0].L()) : 0;
    } catch (const HalconCpp::HException&) {
        return 0;
    }
}

}  // namespace aoi::infra
