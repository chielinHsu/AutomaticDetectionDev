#pragma once

#include <atomic>
#include <memory>
#include <utility>

// application 層。
//
// 取消的最小模型：一個原子旗標，分成兩個型別。
//   CancelSource  握有取消的權力（cancel()），誰建立工作誰持有。
//   CancelToken   只能查詢（cancelled()），交給 use case 與 adapter。可以複製，所有複本看到同一個旗標。
// 這樣 adapter 拿到 token 也不會不小心取消別人的工作（介面隔離）。
//
// 取消是「協作式」的：工作要自己在安全的地方（迴圈、階段之間）查 cancelled()。
// 單次的函式庫呼叫（例如一次 matchTemplate）不能中途停下，所以取消的延遲上限是「最長一次不可中斷的呼叫」。
// UI 不等工作停止（I4 量的是 UI 回到 Idle 的時間），舊結果由工作編號丟棄（JobRunner）。
namespace aoi {

class CancelToken {
public:
    // 預設建構的 token 永遠不會被取消（不配置記憶體）；測試與同步呼叫用。
    CancelToken() = default;

    bool cancelled() const noexcept { return flag_ && flag_->load(std::memory_order_acquire); }

private:
    friend class CancelSource;
    explicit CancelToken(std::shared_ptr<std::atomic<bool>> flag) : flag_(std::move(flag)) {}
    std::shared_ptr<std::atomic<bool>> flag_;
};

class CancelSource {
public:
    CancelSource() : flag_(std::make_shared<std::atomic<bool>>(false)) {}

    // 冪等，可從任何執行緒呼叫。
    void cancel() noexcept { flag_->store(true, std::memory_order_release); }
    bool cancelled() const noexcept { return flag_->load(std::memory_order_acquire); }
    CancelToken token() const { return CancelToken(flag_); }

private:
    std::shared_ptr<std::atomic<bool>> flag_;
};

}  // namespace aoi
