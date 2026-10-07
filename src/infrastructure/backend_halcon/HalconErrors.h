#pragma once

#include <string_view>

#include "application/Error.h"

namespace HalconCpp {
class HException;
}

// infrastructure/backend_halcon：HALCON 的錯誤碼 → application 的 Error（S08）。例外不穿過 port：adapter 在邊界攔下 HException 轉成 Error。
// 錯誤碼見 third_party/halcon/include/HErrorDef.h。
namespace aoi::infra {

// 授權的錯誤碼（HErrorDef.h 的兩段：H_ERR_LIC_RANGE1 2003..2091、H_ERR_LIC_RANGE2 2300..2384）。
constexpr bool isHalconLicenseError(long code) noexcept { return (code >= 2003 && code <= 2091) || (code >= 2300 && code <= 2384); }

inline constexpr long kHalconCancelled = 22;          // H_ERR_CANCEL：interrupt_operator(…, 'cancel') 打斷了運算子
inline constexpr long kHalconTimeout = 9400;          // H_ERR_TIMEOUT
inline constexpr long kHalconTooFewModelPoints = 8510; // H_ERR_NTPTS：樣本裡的邊緣點太少，建不出 shape model

// 對應：取消 → Cancelled；授權 → BackendUnavailable（例如執行中 dongle 被拔掉）；其他 → BackendFailed。
// what：正在做的事（"building the template"、"searching"），寫進訊息。
Error fromHalcon(const HalconCpp::HException& e, std::string_view what);

}  // namespace aoi::infra
