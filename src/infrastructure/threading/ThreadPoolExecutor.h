#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "application/ports/IExecutor.h"

// infrastructure/threading：只用標準庫。IExecutor 的實作；S04 另加 Qt 的 QThreadPool 版。
namespace aoi::infra {

// 固定數量的 worker、FIFO 佇列。解構時把已排入的工作執行完再結束（契約見 IExecutor.h）。
class ThreadPoolExecutor final : public IExecutor {
public:
    // workers 至少 1。
    explicit ThreadPoolExecutor(std::size_t workers = 2);
    ~ThreadPoolExecutor() override;
    ThreadPoolExecutor(const ThreadPoolExecutor&) = delete;
    ThreadPoolExecutor& operator=(const ThreadPoolExecutor&) = delete;

    void post(std::function<void()> task) override;
    std::size_t concurrency() const noexcept override { return threads_.size(); }

private:
    void workerLoop();

    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::function<void()>> queue_;
    bool stopping_ = false;
    std::vector<std::thread> threads_;
};

// 在呼叫端執行緒同步執行：測試要「決定性」而不是並行時用（例如測 use case 與 JobRunner 的流程，不測時序）。
class InlineExecutor final : public IExecutor {
public:
    void post(std::function<void()> task) override {
        if (task) task();
    }
    std::size_t concurrency() const noexcept override { return 1; }
};

}  // namespace aoi::infra
