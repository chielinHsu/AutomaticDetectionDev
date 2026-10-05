#include "presentation/presenters/ParamsPresenter.h"

namespace aoi::ui {

ParamsPresenter::ParamsPresenter(IParamsSink& sink, int debounceMs, QObject* parent)
    : QObject(parent), sink_(sink), pending_(sink.searchParams()) {
    timer_.setSingleShot(true);
    timer_.setInterval(debounceMs);
    connect(&timer_, &QTimer::timeout, this, &ParamsPresenter::apply);
}

void ParamsPresenter::setScoreThreshold(double value) { sink_.applyThreshold(value); }

void ParamsPresenter::setAngleRange(double minDeg, double maxDeg) {
    // 每次變動都重新計時：使用者停下來 debounceMs 之後才搜尋一次。
    pending_ = sink_.searchParams();
    pending_.angleMinDeg = minDeg;
    pending_.angleMaxDeg = maxDeg;
    timer_.start();
}

void ParamsPresenter::setScanScope(int scopeIndex) { sink_.applyScanScope(scopeIndex); }

void ParamsPresenter::flush() {
    if (!timer_.isActive()) return;
    timer_.stop();
    apply();
}

void ParamsPresenter::apply() { sink_.applySearchParams(pending_); }

}  // namespace aoi::ui
