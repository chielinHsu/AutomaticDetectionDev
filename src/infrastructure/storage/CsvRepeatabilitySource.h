#pragma once

#include <QByteArray>
#include "application/ports/IRepeatabilitySource.h"

namespace aoi::infra {
Result<repeatability::MeasurementRun> parseInspectionCsv(const QByteArray& bytes, const std::string& source,
                                                       const CancelToken& cancel = {});
class CsvRepeatabilitySource final : public IRepeatabilitySource {
public:
    Result<repeatability::Dataset> loadFolder(const std::string& path, const CancelToken& cancel) noexcept override;
    Result<repeatability::AnalysisSession> readSession(const std::string& path) noexcept override;
    Result<void> writeSession(const std::string& path, const repeatability::AnalysisSession& session) noexcept override;
    Result<void> exportTables(const std::string& path, const repeatability::AnalysisSession& session,
                             const std::vector<std::string>& metrics, repeatability::Statistic statistic,
                             std::size_t baseline) noexcept override;
};
}  // namespace aoi::infra
