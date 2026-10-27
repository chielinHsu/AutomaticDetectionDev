// 重複性統計的型別存取（valueOf）與預設的指標表（defaultMetrics），repeatability.h。不是搬進 Rust 的演算法
// （ADR-031：型別留在 C++；指標表是資料），所以 AOI_DOMAIN_IMPL=cpp 與 rust 都編這個檔案（R05.4 從 repeatability.cpp 分出來，內容沒有改）。
#include "domain/repeatability.h"

namespace aoi::repeatability {

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
