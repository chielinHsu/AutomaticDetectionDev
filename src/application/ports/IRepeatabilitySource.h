#pragma once

#include "application/repeatability/RepeatabilityAnalysis.h"

namespace aoi {
// Reentrant, no exceptions cross the boundary. CSV input and session storage never execute macros.
class IRepeatabilitySource {
public:
    virtual ~IRepeatabilitySource() = default;
    virtual Result<repeatability::Dataset> loadFolder(const std::string& path, const CancelToken& cancel) noexcept = 0;
    virtual Result<repeatability::AnalysisSession> readSession(const std::string& path) noexcept = 0;
    virtual Result<void> writeSession(const std::string& path, const repeatability::AnalysisSession& session) noexcept = 0;
    virtual Result<void> exportTables(const std::string& path, const repeatability::AnalysisSession& session,
                                     const std::vector<std::string>& metrics, repeatability::Statistic statistic,
                                     std::size_t baseline) noexcept = 0;
};
}  // namespace aoi
