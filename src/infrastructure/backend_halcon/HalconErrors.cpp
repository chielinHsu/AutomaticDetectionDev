#include "infrastructure/backend_halcon/HalconErrors.h"

#include <halconcpp/HalconCpp.h>

#include <string>

namespace aoi::infra {

Error fromHalcon(const HalconCpp::HException& e, std::string_view what) {
    const long code = static_cast<long>(e.ErrorCode());
    const std::string detail = "HALCON error " + std::to_string(code) + " in " + e.ProcName().Text() + ": " + e.ErrorMessage().Text();
    if (code == kHalconCancelled) return makeError(ErrorCode::Cancelled, "The HALCON operation was cancelled while " + std::string(what) + ".");
    if (isHalconLicenseError(code)) {
        return makeError(ErrorCode::BackendUnavailable, "HALCON has no valid license while " + std::string(what) + ". " + detail);
    }
    return makeError(ErrorCode::BackendFailed, "HALCON failed while " + std::string(what) + ". " + detail);
}

}  // namespace aoi::infra
