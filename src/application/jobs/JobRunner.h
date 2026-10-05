#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "application/CancelToken.h"
#include "application/ports/IExecutor.h"
#include "application/ports/IPerfProbe.h"

// application 層。
namespace aoi {

using JobId = std::uint64_t;  // 從 1 起；0 ＝ 沒有工作

enum class JobStatus {
    Completed,  // 工作正常結束，而且結束時它的取消旗標沒有升起
    Cancelled,  // 開始前或結束時旗標已升起（被新工作取代，或呼叫了 cancel()）：呼叫端已經放棄這個結果
    Failed,     // 工作丟出了例外（被 JobRunner 攔下）
};

struct JobReport {
    JobId id = 0;
    JobStatus status = JobStatus::Completed;
    // 工作結束的那一刻，它是不是最新的一項（沒有被更新的 submit 取代，也沒有被 cancel() 作廢）。
    // 這個值是在 worker 執行緒上算的；Presenter 轉回 UI 執行緒後仍要用自己的狀態再檢查一次。
    bool latest = false;
    std::string failure;  // status == Failed 時是例外的 what()
    double queuedMs = 0.0;  // submit → 開始執行
    double runMs = 0.0;     // 開始執行 → 結束

    // 結果可以拿來用：正常結束，而且沒有被取代。
    bool usable() const noexcept { return status == JobStatus::Completed && latest; }
};

// latest-wins 的工作執行器：新工作 submit 時，舊工作的取消旗標立刻升起（不等它停下）。
//
//  - 工作丟出的例外一律攔下，變成 JobStatus::Failed，不會讓 worker 或程式終止。
//  - 結果怎麼傳回去由呼叫端決定：work 與 done 以 lambda 捕獲自己的狀態。done 在 worker 執行緒呼叫
//    （Presenter 在那裡把結果以 queued signal 送回 UI 執行緒），丟出的例外會被丟棄。
//  - 過期的結果靠工作編號與 JobReport::latest 丟棄；UI 不等舊工作真的停下（I4 量的是 UI 回到 Idle 的時間）。
//  - executor 的生命期必須長過 JobRunner。解構時先 cancel() 再等所有已 submit 的工作結束，
//    所以 work 與 done 捕獲的物件只要活得比 JobRunner 久就安全。**不要在 work 或 done 裡解構 JobRunner**（會等自己）。
//  - 可以從任何執行緒呼叫 submit、cancel、latestId、isLatest。
class JobRunner {
public:
    using Work = std::function<void(const CancelToken&)>;
    using Done = std::function<void(const JobReport&)>;

    explicit JobRunner(IExecutor& executor, IPerfProbe* probe = nullptr);
    ~JobRunner();
    JobRunner(const JobRunner&) = delete;
    JobRunner& operator=(const JobRunner&) = delete;

    // 提交一項工作並取代前一項。work 不可為空；done 可以是空的。回傳這項工作的編號。
    JobId submit(Work work, Done done = {});
    // 取消最新的一項（UI 的 Esc），並讓「目前沒有最新的工作」，直到下一次 submit。
    void cancel();

    JobId latestId() const;
    bool isLatest(JobId id) const;

private:
    struct State;
    IExecutor& executor_;
    IPerfProbe* probe_;
    std::shared_ptr<State> state_;
};

}  // namespace aoi
