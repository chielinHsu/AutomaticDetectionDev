// 重複性統計的 C++ 實作（R05.4 從 repeatability.cpp 原樣搬過來，ADR-031；說明在 legacy/repeatability_legacy.h）。
// valueOf、defaultMetrics 是型別的存取與預設的指標表，兩種建置共用 repeatability_types.cpp，不在這裡。
#include "domain/legacy/repeatability_legacy.h"

#include <cmath>
#include <algorithm>

namespace aoi::legacy {

using repeatability::Statistics;

Statistics summarize(std::span<const double> values, std::size_t expected, std::optional<double> tolerance) {
    Statistics out;
    out.expected = expected;
    double mean = 0.0, m2 = 0.0;
    for (double v : values) {
        if (!std::isfinite(v)) continue;
        ++out.count;
        const double delta = v - mean;
        mean += delta / static_cast<double>(out.count);
        m2 += delta * (v - mean);
        out.minimum = out.minimum ? std::min(*out.minimum, v) : v;
        out.maximum = out.maximum ? std::max(*out.maximum, v) : v;
    }
    if (out.count == 0) return out;
    out.average = mean;
    out.range = *out.maximum - *out.minimum;
    if (out.count >= 2) {
        out.deviation = std::sqrt(std::max(0.0, m2 / static_cast<double>(out.count - 1)));
        if (tolerance && std::isfinite(*tolerance) && *tolerance > 0.0) out.pt = 6.0 * *out.deviation / *tolerance;
    }
    return out;
}

}  // namespace aoi::legacy
