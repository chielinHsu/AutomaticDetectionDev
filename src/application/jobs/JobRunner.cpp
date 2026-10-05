#include "application/jobs/JobRunner.h"

#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <utility>

namespace aoi {

namespace {

using Clock = std::chrono::steady_clock;

double millisecondsBetween(Clock::time_point from, Clock::time_point to) {
    return std::chrono::duration<double, std::milli>(to - from).count();
}

}  // namespace

// 所有執行緒共享的狀態。由 shared_ptr 持有，排進 executor 的工作也持有一份，
// 所以即使 JobRunner 先被解構，還沒開始的工作也不會碰到已釋放的記憶體。
struct JobRunner::State {
    std::mutex mutex;
    std::condition_variable idle;
    JobId nextId = 0;
    JobId latest = 0;          // 0 ＝ 沒有最新的工作（cancel() 之後）
    CancelSource current;      // 最新一項的取消來源
    std::size_t pending = 0;   // 已 submit 但還沒結束（含排隊中）的工作數
};

JobRunner::JobRunner(IExecutor& executor, IPerfProbe* probe)
    : executor_(executor), probe_(probe), state_(std::make_shared<State>()) {}

JobRunner::~JobRunner() {
    cancel();
    std::unique_lock lock(state_->mutex);
    state_->idle.wait(lock, [this] { return state_->pending == 0; });
}

JobId JobRunner::submit(Work work, Done done) {
    CancelSource source;
    CancelSource previous;
    JobId id = 0;
    {
        std::lock_guard lock(state_->mutex);
        id = ++state_->nextId;
        previous = state_->current;
        state_->current = source;
        state_->latest = id;
        ++state_->pending;
    }
    // latest-wins：舊工作的旗標在這一刻升起。它是否真的停下，由它自己在安全的地方查旗標決定。
    previous.cancel();

    const Clock::time_point submitted = Clock::now();
    executor_.post([state = state_, probe = probe_, id, token = source.token(), work = std::move(work),
                    done = std::move(done), submitted]() mutable {
        JobReport report;
        report.id = id;
        const Clock::time_point started = Clock::now();
        report.queuedMs = millisecondsBetween(submitted, started);

        if (token.cancelled()) {
            report.status = JobStatus::Cancelled;  // 還沒開始就被取代：不必執行
        } else {
            try {
                if (work) work(token);
                report.status = token.cancelled() ? JobStatus::Cancelled : JobStatus::Completed;
            } catch (const std::exception& e) {
                report.status = JobStatus::Failed;
                report.failure = e.what();
            } catch (...) {
                report.status = JobStatus::Failed;
                report.failure = "unknown exception";
            }
        }
        report.runMs = millisecondsBetween(started, Clock::now());
        {
            std::lock_guard lock(state->mutex);
            report.latest = (state->latest == id);
        }
        if (probe != nullptr && probe->enabled()) {
            probe->record("job.queue", static_cast<std::int64_t>(report.queuedMs * 1e6));
            probe->record("job.run", static_cast<std::int64_t>(report.runMs * 1e6));
        }
        if (done) {
            try {
                done(report);
            } catch (...) {
                // done 不可以丟例外；丟出來的一律丟棄，不讓 worker 終止。
            }
        }
        {
            std::lock_guard lock(state->mutex);
            if (--state->pending == 0) state->idle.notify_all();
        }
    });
    return id;
}

void JobRunner::cancel() {
    CancelSource current;
    {
        std::lock_guard lock(state_->mutex);
        current = state_->current;
        state_->latest = 0;
    }
    current.cancel();
}

JobId JobRunner::latestId() const {
    std::lock_guard lock(state_->mutex);
    return state_->latest;
}

bool JobRunner::isLatest(JobId id) const {
    std::lock_guard lock(state_->mutex);
    return id != 0 && state_->latest == id;
}

}  // namespace aoi
