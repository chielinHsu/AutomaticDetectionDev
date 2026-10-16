#pragma once

#include "application/CancelToken.h"
#include "application/Result.h"
#include "domain/repeatability.h"

namespace aoi::repeatability {

struct AnalysisSession {
    MetricCatalog metrics = defaultMetrics();
    std::vector<Dataset> datasets;
};
struct DatasetAnalysis {
    std::map<std::string, std::map<BoxKey, Statistics>> metrics;
    std::size_t incomplete = 0;
};
struct Comparison {
    std::size_t commonBoxes = 0;
    std::vector<std::optional<double>> values;
};
// Always compare the same complete Box keys across every group. Missing values never become zero.
Result<DatasetAnalysis> analyze(const Dataset& dataset, const MetricCatalog& catalog, const CancelToken& cancel = {});
Result<void> discoverMetrics(AnalysisSession& session);
Comparison compare(const std::vector<DatasetAnalysis>& groups, const std::string& metric, Statistic statistic);

}  // namespace aoi::repeatability
