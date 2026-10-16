#include "presentation/repeatability/RepeatabilityPresenter.h"

#include <QtConcurrentRun>
#include <exception>

namespace aoi::ui {
using namespace repeatability;

RepeatabilityPresenter::RepeatabilityPresenter(std::shared_ptr<IRepeatabilitySource> source, QObject* parent)
    : QObject(parent), source_(std::move(source)) {}

RepeatabilityPresenter::~RepeatabilityPresenter() {
    cancellation_.cancel();
    watcher_.disconnect(this);
    watcher_.waitForFinished();
}

void RepeatabilityPresenter::start(Prepare prepare, const QString& exportFolder) {
    if (busy_) return;
    busy_ = true;
    cancellation_ = CancelSource();
    const CancelToken cancel = cancellation_.token();
    emit busyChanged(true);
    watcher_.disconnect(this);
    connect(&watcher_, &QFutureWatcherBase::finished, this, [this, exportFolder] {
        const auto result = watcher_.result();
        busy_ = false;
        if (!cancellation_.cancelled()) {
            if (result) { state_ = result.value(); emit changed(); if (!exportFolder.isEmpty()) emit exported(exportFolder); }
            else if (result.error().code != ErrorCode::Cancelled) emit failed(QString::fromStdString(result.error().message));
        }
        emit busyChanged(false);
    });
    watcher_.setFuture(QtConcurrent::run([prepare = std::move(prepare), cancel]() -> Result<std::shared_ptr<RepeatabilityState>> {
        try {
            auto session = prepare(cancel);
            if (!session) return session.error();
            auto discovered = discoverMetrics(session.value());
            if (!discovered) return discovered.error();
            auto result = std::make_shared<RepeatabilityState>();
            result->session = std::move(session.value());
            for (const Dataset& d : result->session.datasets) {
                auto analysis = analyze(d, result->session.metrics, cancel);
                if (!analysis) return analysis.error();
                result->analyses.push_back(std::move(analysis.value()));
            }
            return result;
        } catch (const std::exception& e) { return makeError(ErrorCode::Internal, e.what()); }
        catch (...) { return makeError(ErrorCode::Internal, "Unexpected analysis failure."); }
    }));
}

void RepeatabilityPresenter::loadFolder(const QString& path) {
    if (!source_ || busy_) return;
    if (state_->session.datasets.size() >= 4) { emit failed(QStringLiteral("At most four folders are allowed.")); return; }
    start([source = source_, current = state_, path](const CancelToken& cancel) -> Result<AnalysisSession> {
        auto d = source->loadFolder(path.toStdString(), cancel);
        if (!d) return d.error();
        for (const Dataset& old : current->session.datasets)
            if (old.folder == d.value().folder) return makeError(ErrorCode::InvalidArgument, "This folder is already loaded.");
        AnalysisSession session = current->session;
        session.datasets.push_back(std::move(d.value()));
        return session;
    });
}

void RepeatabilityPresenter::removeFolder(std::size_t index) {
    if (busy_ || index >= state_->session.datasets.size()) return;
    auto next = std::make_shared<RepeatabilityState>(*state_);
    next->session.datasets.erase(next->session.datasets.begin() + static_cast<std::ptrdiff_t>(index));
    next->analyses.erase(next->analyses.begin() + static_cast<std::ptrdiff_t>(index));
    state_ = std::move(next);
    emit changed();
}

void RepeatabilityPresenter::configure(const MetricCatalog& metrics) {
    start([current = state_, metrics](const CancelToken&) -> Result<AnalysisSession> {
        AnalysisSession session = current->session; session.metrics = metrics; return session;
    });
}
void RepeatabilityPresenter::loadSession(const QString& path) {
    if (!source_) return;
    start([source = source_, path](const CancelToken&) { return source->readSession(path.toStdString()); });
}
void RepeatabilityPresenter::saveSession(const QString& path) {
    if (!source_) return;
    start([source = source_, current = state_, path](const CancelToken&) -> Result<AnalysisSession> {
        const auto saved = source->writeSession(path.toStdString(), current->session);
        if (!saved) return saved.error();
        return current->session;
    });
}
void RepeatabilityPresenter::exportTables(const QString& folder, const std::vector<std::string>& metrics,
                                        Statistic statistic, std::size_t baseline) {
    if (!source_) return;
    start([source = source_, current = state_, folder, metrics, statistic, baseline](const CancelToken&) -> Result<AnalysisSession> {
        const auto saved = source->exportTables(folder.toStdString(), current->session, metrics, statistic, baseline);
        if (!saved) return saved.error();
        return current->session;
    }, folder);
}
void RepeatabilityPresenter::cancel() { cancellation_.cancel(); }
}  // namespace aoi::ui
