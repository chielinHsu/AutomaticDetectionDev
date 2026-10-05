#include "infrastructure/threading/QtThreadPoolExecutor.h"

#include <QRunnable>
#include <algorithm>
#include <limits>
#include <utility>

namespace aoi::infra {

QtThreadPoolExecutor::QtThreadPoolExecutor(std::size_t workers) : workers_(std::max<std::size_t>(1, workers)) {
    pool_.setMaxThreadCount(static_cast<int>(workers_));
    // worker 不會閒置到期。Qt 文件說負值是「永不到期」，但實測（Qt 6.8.0）-1 讓每個工作都換一條新執行緒；INT_MAX（約 24 天）才會保留。
    pool_.setExpiryTimeout(std::numeric_limits<int>::max());
}

QtThreadPoolExecutor::~QtThreadPoolExecutor() {
    pool_.waitForDone();  // 已排入的工作執行完再結束（契約）
}

void QtThreadPoolExecutor::post(std::function<void()> task) {
    if (!task) return;
    // QRunnable::create 回傳 autoDelete 的 runnable：pool 執行完會刪掉它。
    pool_.start(QRunnable::create([task = std::move(task)]() mutable {
        try {
            task();
        } catch (...) {
            // 契約：工作不可以丟例外。逃出來的丟棄；QThreadPool 對 runnable 丟出的例外不會自己處理，會終止程式。
        }
    }));
}

}  // namespace aoi::infra
