#include "application/repeatability/RepeatabilityAnalysis.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace aoi::repeatability {
namespace {
double unitScale(const std::string& source, const std::string& target) {
    if (source == "mm" && target == "µm") return 1000.0;
    if (source == "µm" && target == "mm") return 0.001;
    return 1.0;  // A custom unit is an explicit user override of the report-wide unit.
}
}

Result<void> discoverMetrics(AnalysisSession& session) {
    if (session.datasets.size() > 4) return makeError(ErrorCode::InvalidArgument, "At most four folders are allowed.");
    for (const Dataset& d : session.datasets) {
        if (d.runs.empty()) return makeError(ErrorCode::InvalidArgument, "A folder has no measurement runs.");
        for (const MeasurementRun& run : d.runs) {
            for (const auto& [key, values] : run.boxes) {
                (void)key;
                for (const auto& [id, v] : values) {
                    (void)v;
                    if (!session.metrics.contains(id)) session.metrics.emplace(id, MetricDefinition{id, run.unit, 1.0, {}, 0.20});
                }
            }
        }
    }
    return {};
}

Result<DatasetAnalysis> analyze(const Dataset& dataset, const MetricCatalog& catalog, const CancelToken& cancel) {
    if (dataset.runs.empty()) return makeError(ErrorCode::InvalidArgument, "No measurement runs.");
    std::map<std::string, std::map<BoxKey, std::vector<double>>> collected;
    std::set<BoxKey> keys;
    std::set<std::string> ids;
    for (const MeasurementRun& run : dataset.runs) {
        if (cancel.cancelled()) return makeError(ErrorCode::Cancelled, "Analysis cancelled.");
        for (const auto& [key, values] : run.boxes) {
            keys.insert(key);
            for (const auto& [id, v] : values) {
                ids.insert(id);
                const auto def = catalog.find(id);
                if (def == catalog.end()) return makeError(ErrorCode::InvalidArgument, "Unknown metric: " + id);
                const MetricDefinition& m = def->second;
                if ((m.unit == "µm" || m.unit == "mm") && run.unit != "µm" && run.unit != "mm")
                    return makeError(ErrorCode::InvalidArgument, "Unsupported source unit for " + id + ": " + run.unit);
                if (m.unit.empty() || !std::isfinite(m.multiplier) || m.multiplier <= 0 || !std::isfinite(m.ptLimit) || m.ptLimit <= 0 ||
                    (m.tolerance && (!std::isfinite(*m.tolerance) || *m.tolerance <= 0)))
                    return makeError(ErrorCode::InvalidArgument, "Invalid metric settings: " + id);
                if (v) {
                    const double scaled = *v * m.multiplier * unitScale(run.unit, m.unit);
                    if (!std::isfinite(scaled)) return makeError(ErrorCode::InvalidArgument, "Non-finite value: " + id);
                    collected[id][key].push_back(scaled);
                }
            }
        }
    }
    DatasetAnalysis out;
    for (const std::string& id : ids) {
        for (const BoxKey& key : keys) {
            Statistics s = summarize(collected[id][key], dataset.runs.size(), catalog.at(id).tolerance);
            if ((s.average && !std::isfinite(*s.average)) || (s.range && !std::isfinite(*s.range)) ||
                (s.deviation && !std::isfinite(*s.deviation)) || (s.pt && !std::isfinite(*s.pt)))
                return makeError(ErrorCode::InvalidArgument, "Numeric overflow for metric: " + id);
            if (!s.complete()) ++out.incomplete;
            out.metrics[id].emplace(key, std::move(s));
        }
    }
    return out;
}

Comparison compare(const std::vector<DatasetAnalysis>& groups, const std::string& metric, Statistic statistic) {
    Comparison out;
    out.values.resize(groups.size());
    if (groups.empty()) return out;
    const auto first = groups.front().metrics.find(metric);
    if (first == groups.front().metrics.end()) return out;
    for (const auto& [key, stats] : first->second) {
        (void)stats;
        std::vector<std::optional<double>> values;
        for (const DatasetAnalysis& group : groups) {
            const auto mi = group.metrics.find(metric);
            if (mi == group.metrics.end()) break;
            const auto si = mi->second.find(key);
            if (si == mi->second.end() || !si->second.complete()) break;
            values.push_back(valueOf(si->second, statistic));
        }
        if (values.size() != groups.size()) continue;
        ++out.commonBoxes;
        for (std::size_t g = 0; g < values.size(); ++g) {
            if (!values[g]) continue;
            if (statistic == Statistic::Average) {
                const double old = out.values[g].value_or(0.0);
                out.values[g] = old + (*values[g] - old) / static_cast<double>(out.commonBoxes);
            }
            else out.values[g] = out.values[g] ? std::max(*out.values[g], *values[g]) : values[g];
        }
    }
    return out;
}

}  // namespace aoi::repeatability
