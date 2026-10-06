#include "application/document/RecipeSaver.h"

#include <utility>

namespace aoi {

RecipeSaver::RecipeSaver(IRecipeStore& store, IExecutor& executor) : store_(store), executor_(executor) {}

RecipeSaver::~RecipeSaver() { waitIdle(); }

std::uint64_t RecipeSaver::save(Recipe recipe, std::string path, Done done) {
    std::optional<Request> superseded;
    bool start = false;
    std::uint64_t ticket = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ticket = nextTicket_++;
        ++pending_;
        PathState& state = paths_[path];
        if (state.next) superseded = std::move(state.next);
        state.next = Request{ticket, std::move(recipe), std::move(done)};
        if (!state.writing) {
            state.writing = true;
            start = true;
        }
    }

    if (superseded) {
        // 被取代的存檔也在 worker 上收到結果（done 永遠在 worker 執行緒上）。
        Outcome outcome{superseded->ticket, path,
                        makeError(ErrorCode::Cancelled, "A newer save of the same file replaced this one before it started.")};
        auto notify = [this, d = std::move(superseded->done), outcome = std::move(outcome)] {
            finish(d, outcome);
            std::lock_guard<std::mutex> lock(mutex_);
            --pending_;
            idle_.notify_all();
        };
        try {
            executor_.post(std::move(notify));
        } catch (...) {
            notify();
        }
    }
    if (start) {
        try {
            executor_.post([this, path] { drain(path); });
        } catch (...) {
            drain(path);  // 排不進去（記憶體不足）就在這裡寫：不能讓這個路徑永遠停在「寫入中」
        }
    }
    return ticket;
}

// 寫完一份就在同一段鎖內看下一份；最後一次碰 this 是在持有鎖的時候（解構端要等這把鎖才會醒來）。
void RecipeSaver::drain(const std::string& path) {
    std::unique_lock<std::mutex> lock(mutex_);
    for (;;) {
        auto it = paths_.find(path);
        if (it == paths_.end()) return;  // 不會發生：寫入中的路徑只由這裡拿掉
        if (!it->second.next) {
            paths_.erase(it);
            idle_.notify_all();
            return;
        }
        Request request = std::move(*it->second.next);
        it->second.next.reset();
        lock.unlock();

        Result<void> result;
        try {
            result = store_.save(request.recipe, path);
        } catch (...) {
            result = makeError(ErrorCode::Internal, "The recipe store threw an exception.");
        }
        finish(request.done, Outcome{request.ticket, path, std::move(result)});

        lock.lock();
        --pending_;
        idle_.notify_all();
    }
}

void RecipeSaver::finish(const Done& done, const Outcome& outcome) noexcept {
    try {
        if (done) done(outcome);
    } catch (...) {
    }
}

void RecipeSaver::waitIdle() {
    std::unique_lock<std::mutex> lock(mutex_);
    idle_.wait(lock, [this] { return pending_ == 0 && paths_.empty(); });
}

std::size_t RecipeSaver::pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_;
}

}  // namespace aoi
