#include "infrastructure/threading/ThreadPoolExecutor.h"

#include <algorithm>
#include <utility>

namespace aoi::infra {

ThreadPoolExecutor::ThreadPoolExecutor(std::size_t workers) {
    const std::size_t count = std::max<std::size_t>(1, workers);
    threads_.reserve(count);
    for (std::size_t i = 0; i < count; ++i) threads_.emplace_back([this] { workerLoop(); });
}

ThreadPoolExecutor::~ThreadPoolExecutor() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    for (std::thread& t : threads_) t.join();
}

void ThreadPoolExecutor::post(std::function<void()> task) {
    if (!task) return;
    {
        std::lock_guard lock(mutex_);
        if (stopping_) return;  // 解構開始後 post 是程式錯誤：丟棄
        queue_.push_back(std::move(task));
    }
    wake_.notify_one();
}

void ThreadPoolExecutor::workerLoop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (queue_.empty()) return;  // stopping_ 且佇列已空
            task = std::move(queue_.front());
            queue_.pop_front();
        }
        try {
            task();
        } catch (...) {
            // 契約：工作不可以丟例外。逃出來的丟棄，不讓 worker 死掉（JobRunner 已經在自己的邊界攔下例外）。
        }
    }
}

}  // namespace aoi::infra
