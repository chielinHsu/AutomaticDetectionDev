#include "infrastructure/backend_halcon/HalconCancelWatch.h"

#include <chrono>

namespace aoi::infra {

HalconCancelWatch::HalconCancelWatch(const CancelToken& token) : token_(token) {
    HalconCpp::GetCurrentHthreadId(&threadId_);
    thread_ = std::thread([this] {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!stop_) {
            if (token_.cancelled()) {
                try {
                    HalconCpp::InterruptOperator(threadId_, "cancel");
                    fired_.store(true, std::memory_order_release);
                } catch (const HalconCpp::HException&) {
                    // 打斷失敗（例如執行緒已經沒有在跑運算子）：下一輪再試，範圍結束就停。
                }
            }
            wake_.wait_for(lock, std::chrono::milliseconds(1), [this] { return stop_; });
        }
    });
}

HalconCancelWatch::~HalconCancelWatch() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_one();
    thread_.join();
}

}  // namespace aoi::infra
