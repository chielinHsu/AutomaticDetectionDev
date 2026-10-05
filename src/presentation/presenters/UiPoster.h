#pragma once

#include <QMetaObject>
#include <QObject>
#include <functional>
#include <mutex>
#include <utility>

namespace aoi::ui {

// worker 執行緒 → UI 執行緒的橋。JobRunner 的 done 與 IProgressSink::report 都在 worker 上被呼叫，
// 而 Presenter 只能在 UI 執行緒動（它碰 Widgets 與工作階段），所以它們把「要做的事」交給這裡，
// 由 UI 執行緒的事件迴圈執行（queued）。**只傳不可變的快照**，不共享可變狀態。
//
// 生命期：worker 的 lambda 持有的是這個物件的 shared_ptr，不是 Presenter 本身。Presenter 解構時呼叫 detach()：
// 之後 worker 再 post 的工作一律丟掉；已排進事件佇列、還沒送達的，會隨 target（QObject）解構而被 Qt 丟掉。
// detach() 與 post() 用同一把鎖，所以不會有「worker 正在對一個正在解構的 QObject 排事件」的競爭。
class UiPoster {
public:
    explicit UiPoster(QObject* target) : target_(target) {}
    UiPoster(const UiPoster&) = delete;
    UiPoster& operator=(const UiPoster&) = delete;

    // 可以從任何執行緒呼叫。fn 在 target 所在的執行緒執行；呼叫端的執行緒就是 target 的執行緒時，也要等下一輪事件迴圈。
    void post(std::function<void()> fn) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (target_ == nullptr) return;
        QMetaObject::invokeMethod(target_, [fn = std::move(fn)] { fn(); }, Qt::QueuedConnection);
    }

    // 只在 UI 執行緒（target 還活著時）呼叫一次。
    void detach() {
        std::lock_guard<std::mutex> lock(mutex_);
        target_ = nullptr;
    }

private:
    std::mutex mutex_;
    QObject* target_;
};

}  // namespace aoi::ui
