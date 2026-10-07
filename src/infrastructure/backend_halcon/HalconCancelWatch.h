#pragma once

#include <halconcpp/HalconCpp.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "application/CancelToken.h"

// infrastructure/backend_halcon：把協作式的取消（CancelToken 只是一個旗標）接到 HALCON 的中斷（S08）。
//
// 一次 find_shape_model 可能要幾十到幾百毫秒，中間沒有地方讓我們看旗標。HALCON 有 interrupt_operator(HThreadID, 'cancel')：
// 從別的執行緒打斷某個執行緒正在跑的運算子（運算子要支援；find_shape_model、create_shape_model 支援與否用
// get_operator_info(…, 'interrupt_mode') 實際查過，見 docs/steps/S08-halcon/spec.md），被打斷的運算子回 H_ERR_CANCEL（22）。
//
// 用法：把要能取消的 HALCON 呼叫包在這個物件的生命週期裡（範圍越小越好）：
//     { HalconCancelWatch watch(cancel); model.FindShapeModel(...); }
// 它建一個小執行緒，每毫秒看一次旗標；旗標升起後，每次都對呼叫端的執行緒送一次中斷，直到範圍結束
// （中斷只對「正在跑」的運算子有效：旗標剛好在兩個運算子之間升起時，下一次會打到下一個運算子）。
// 延遲的上限約是 Windows 的計時器解析度（約 16 ms，和 S03 量的取消延遲同一個來源）。
// 範圍結束時（解構）停掉小執行緒，之後不會再送中斷。
namespace aoi::infra {

class HalconCancelWatch {
public:
    explicit HalconCancelWatch(const CancelToken& token);
    ~HalconCancelWatch();
    HalconCancelWatch(const HalconCancelWatch&) = delete;
    HalconCancelWatch& operator=(const HalconCancelWatch&) = delete;

    // 送過中斷沒有。
    bool fired() const noexcept { return fired_.load(std::memory_order_acquire); }

private:
    CancelToken token_;
    HalconCpp::HTuple threadId_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stop_ = false;
    std::atomic<bool> fired_{false};
    std::thread thread_;
};

}  // namespace aoi::infra
