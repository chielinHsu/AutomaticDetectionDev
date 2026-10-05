#pragma once

#include <QThreadPool>
#include <cstddef>
#include <functional>

#include "application/ports/IExecutor.h"

// infrastructure/threading：可以用 Qt Core（不可以用 Qt Widgets）。IExecutor 的 QThreadPool 版（S04）。
namespace aoi::infra {

// 自己擁有一個 QThreadPool（不用全域的那個：全域的 pool 會被別的程式碼搶執行緒，也沒辦法保證 FIFO 的人數）。
// 契約（IExecutor.h）：
//  - post 不在呼叫端執行緒阻塞，工作依 post 的順序開始執行（同優先權的 QRunnable 是 FIFO；測試以單一 worker 驗證）。
//  - 工作丟出的例外被攔下並丟棄，不會讓 worker 終止程式（QThreadPool 自己會把例外往外丟）。
//  - 解構時把已排入的工作執行完再結束；解構開始之後 post 是程式錯誤，工作被丟棄。
// worker 不會閒置到期（setExpiryTimeout(INT_MAX)，見 .cpp 的說明）：搜尋之間隔幾秒是常態，每次都重開執行緒會讓第一個工作多等一次執行緒啟動。
class QtThreadPoolExecutor final : public IExecutor {
public:
    // workers 至少 1。
    explicit QtThreadPoolExecutor(std::size_t workers = 2);
    ~QtThreadPoolExecutor() override;
    QtThreadPoolExecutor(const QtThreadPoolExecutor&) = delete;
    QtThreadPoolExecutor& operator=(const QtThreadPoolExecutor&) = delete;

    void post(std::function<void()> task) override;
    std::size_t concurrency() const noexcept override { return workers_; }

private:
    QThreadPool pool_;
    std::size_t workers_;
};

}  // namespace aoi::infra
