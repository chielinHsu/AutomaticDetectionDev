// AOI_DOMAIN_IMPL=cpp：轉呼叫 C++ 的實作（aoi::legacy，legacy/repeatability_legacy.cpp）。
// AOI_DOMAIN_IMPL=rust 時編的是 repeatability_rs.cpp（ADR-031）。valueOf、defaultMetrics 在 repeatability_types.cpp（兩種建置共用）。
#include "domain/repeatability.h"

#include "domain/legacy/repeatability_legacy.h"

namespace aoi::repeatability {

Statistics summarize(std::span<const double> values, std::size_t expected, std::optional<double> tolerance) {
    return legacy::summarize(values, expected, tolerance);
}

}  // namespace aoi::repeatability
