#pragma once

#include <cstddef>
#include <functional>

// application 層：port。實作：infrastructure/threading（std::thread 版）；S04 加 Qt 的 QThreadPool 版。
namespace aoi {

// 契約：
//  - post 把工作交給某個 worker 執行緒，不在呼叫端執行緒阻塞（InlineExecutor 是測試用的例外，它同步執行）。
//  - 工作不可以丟例外（JobRunner 保證它交進來的工作不會丟）；逃出來的例外會被丟棄，不會讓 worker 死掉。
//  - 工作依 post 的順序開始執行（FIFO）；有多個 worker 時可能同時執行。
//  - 解構時把已經排入的工作執行完再結束。解構開始之後呼叫 post 是程式錯誤（工作會被丟棄）。
class IExecutor {
public:
    virtual ~IExecutor() = default;
    virtual void post(std::function<void()> task) = 0;
    // worker 的數量（至少 1）。
    virtual std::size_t concurrency() const noexcept = 0;
};

}  // namespace aoi
