#include "domain/repeatability.h"

#include <cmath>
#include <algorithm>

namespace aoi::repeatability {

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

std::optional<double> valueOf(const Statistics& s, Statistic statistic) {
    switch (statistic) {
        case Statistic::Average: return s.average;
        case Statistic::Range: return s.range;
        case Statistic::StandardDeviation: return s.deviation;
        case Statistic::Pt: return s.pt;
    }
    return {};
}

MetricCatalog defaultMetrics() {
    MetricCatalog out;
    for (const auto& [id, spec] : std::map<std::string, double>{{"CO", 350}, {"XO", 250}, {"YO", 250}, {"RO", 200},
             {"LC", 200}, {"CX", 200}, {"CY", 200}, {"WI", 200}, {"BH", 200}, {"LH", 200}, {"PP", 200}})
        out.emplace(id, MetricDefinition{id, "µm", 1.0, spec, 0.20});
    return out;
}

}  // namespace aoi::repeatability
