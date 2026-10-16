#pragma once

#include <compare>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace aoi::repeatability {

struct BoxKey {
    std::string component;
    std::string box;
    auto operator<=>(const BoxKey&) const = default;
};
using Values = std::map<std::string, std::optional<double>>;
struct MeasurementRun {
    std::string source;
    std::string timestamp;
    std::string unit;
    std::map<BoxKey, Values> boxes;
};
struct Dataset {
    std::string name;
    std::string folder;
    std::vector<MeasurementRun> runs;
};
struct MetricDefinition {
    std::string id;
    std::string unit;
    double multiplier = 1.0;
    std::optional<double> tolerance;
    double ptLimit = 0.20;
};
using MetricCatalog = std::map<std::string, MetricDefinition>;
enum class Statistic { Average, Range, StandardDeviation, Pt };
struct Statistics {
    std::size_t count = 0;
    std::size_t expected = 0;
    std::optional<double> minimum, maximum, average, range, deviation, pt;
    bool complete() const { return count == expected && count >= 2; }
};
Statistics summarize(std::span<const double> values, std::size_t expected, std::optional<double> tolerance);
std::optional<double> valueOf(const Statistics& stats, Statistic statistic);
MetricCatalog defaultMetrics();

}  // namespace aoi::repeatability
