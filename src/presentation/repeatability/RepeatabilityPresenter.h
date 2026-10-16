#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <functional>
#include <memory>
#include "application/ports/IRepeatabilitySource.h"

namespace aoi::ui {
struct RepeatabilityState {
    repeatability::AnalysisSession session;
    std::vector<repeatability::DatasetAnalysis> analyses;
};
class RepeatabilityPresenter : public QObject {
    Q_OBJECT
public:
    explicit RepeatabilityPresenter(std::shared_ptr<IRepeatabilitySource> source, QObject* parent = nullptr);
    ~RepeatabilityPresenter() override;
    const RepeatabilityState& state() const { return *state_; }
    std::shared_ptr<const RepeatabilityState> snapshot() const { return state_; }
    bool busy() const { return busy_; }
    void loadFolder(const QString& path);
    void removeFolder(std::size_t index);
    void configure(const repeatability::MetricCatalog& metrics);
    void loadSession(const QString& path);
    void saveSession(const QString& path);
    void exportTables(const QString& folder, const std::vector<std::string>& metrics,
                      repeatability::Statistic statistic, std::size_t baseline);
    void cancel();
signals:
    void changed();
    void busyChanged(bool busy);
    void failed(const QString& message);
    void exported(const QString& folder);
private:
    using Prepare = std::function<Result<repeatability::AnalysisSession>(const CancelToken&)>;
    void start(Prepare prepare, const QString& exportFolder = {});
    std::shared_ptr<IRepeatabilitySource> source_;
    std::shared_ptr<RepeatabilityState> state_ = std::make_shared<RepeatabilityState>();
    CancelSource cancellation_;
    QFutureWatcher<Result<std::shared_ptr<RepeatabilityState>>> watcher_;
    bool busy_ = false;
};
}  // namespace aoi::ui
