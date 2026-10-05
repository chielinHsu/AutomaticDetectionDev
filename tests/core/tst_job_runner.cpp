// JobRunner：latest-wins、取消、例外邊界、解構時等待。
// 排程的行為用「閘門」（原子旗標）控制，不靠睡眠猜時間；只有「等某件事發生」才用逾時（5 秒，失敗時才會用到）。
#include <QtTest>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

#include "application/jobs/JobRunner.h"
#include "infrastructure/threading/ThreadPoolExecutor.h"

using namespace aoi;
using namespace std::chrono_literals;

namespace {

// 等條件成立，最多 5 秒。
template <typename Pred>
bool waitUntil(Pred pred) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (!pred()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

// 收集 done 的報告（可以從多個 worker 呼叫）。
class Reports {
public:
    JobRunner::Done sink() {
        return [this](const JobReport& r) {
            std::lock_guard lock(mutex_);
            reports_.push_back(r);
        };
    }
    std::size_t count() const {
        std::lock_guard lock(mutex_);
        return reports_.size();
    }
    JobReport byId(JobId id) const {
        std::lock_guard lock(mutex_);
        for (const JobReport& r : reports_) {
            if (r.id == id) return r;
        }
        return {};
    }
    std::vector<JobReport> all() const {
        std::lock_guard lock(mutex_);
        return reports_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<JobReport> reports_;
};

}  // namespace

class TstJobRunner : public QObject {
    Q_OBJECT

private slots:
    void aJobRunsAndReportsCompletion() {
        infra::InlineExecutor executor;
        JobRunner runner(executor);
        Reports reports;
        bool ran = false;
        const JobId id = runner.submit([&](const CancelToken&) { ran = true; }, reports.sink());
        QVERIFY(ran);
        QCOMPARE(reports.count(), std::size_t{1});
        const JobReport r = reports.byId(id);
        QVERIFY(r.status == JobStatus::Completed);
        QVERIFY(r.latest);
        QVERIFY(r.usable());
        QVERIFY(r.queuedMs >= 0.0 && r.runMs >= 0.0);
    }

    void idsIncreaseFromOne() {
        infra::InlineExecutor executor;
        JobRunner runner(executor);
        QCOMPARE(runner.latestId(), JobId{0});
        const JobId a = runner.submit([](const CancelToken&) {});
        const JobId b = runner.submit([](const CancelToken&) {});
        QCOMPARE(a, JobId{1});
        QCOMPARE(b, JobId{2});
        QCOMPARE(runner.latestId(), b);
        QVERIFY(runner.isLatest(b));
        QVERIFY(!runner.isLatest(a));
        QVERIFY(!runner.isLatest(0));
    }

    // 例外邊界：work 丟例外不會讓 worker 或程式終止，變成 Failed，之後 runner 仍可用。
    void anExceptionInTheWorkBecomesAFailedReport() {
        infra::ThreadPoolExecutor executor(1);
        JobRunner runner(executor);
        Reports reports;
        const JobId bad = runner.submit([](const CancelToken&) { throw std::runtime_error("boom"); }, reports.sink());
        QVERIFY(waitUntil([&] { return reports.count() == 1; }));
        const JobReport r = reports.byId(bad);
        QVERIFY(r.status == JobStatus::Failed);
        QCOMPARE(r.failure, std::string("boom"));
        QVERIFY(!r.usable());

        const JobId good = runner.submit([](const CancelToken&) {}, reports.sink());
        QVERIFY(waitUntil([&] { return reports.count() == 2; }));
        QVERIFY(reports.byId(good).usable());
    }

    void anUnknownExceptionIsAlsoCaught() {
        infra::InlineExecutor executor;
        JobRunner runner(executor);
        Reports reports;
        const JobId id = runner.submit([](const CancelToken&) { throw 42; }, reports.sink());
        const JobReport r = reports.byId(id);
        QVERIFY(r.status == JobStatus::Failed);
        QVERIFY(!r.failure.empty());
    }

    void anExceptionInDoneIsSwallowed() {
        infra::ThreadPoolExecutor executor(1);
        JobRunner runner(executor);
        std::atomic<int> doneCalls{0};
        runner.submit([](const CancelToken&) {}, [&](const JobReport&) {
            ++doneCalls;
            throw std::runtime_error("done must not throw");
        });
        QVERIFY(waitUntil([&] { return doneCalls == 1; }));
        Reports reports;  // worker 還活著：後面的工作照常執行
        const JobId next = runner.submit([](const CancelToken&) {}, reports.sink());
        QVERIFY(waitUntil([&] { return reports.count() == 1; }));
        QVERIFY(reports.byId(next).usable());
    }

    // latest-wins 的核心：新工作 submit 時，舊工作的旗標立刻升起；舊工作被報告為 Cancelled 且不是最新。
    void aNewJobCancelsTheRunningOne() {
        infra::ThreadPoolExecutor executor(2);
        JobRunner runner(executor);
        Reports reports;
        std::atomic<bool> firstStarted{false};
        const JobId first = runner.submit(
            [&](const CancelToken& token) {
                firstStarted = true;
                const auto deadline = std::chrono::steady_clock::now() + 5s;
                while (!token.cancelled() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
            },
            reports.sink());
        QVERIFY(waitUntil([&] { return firstStarted.load(); }));

        const JobId second = runner.submit([](const CancelToken&) {}, reports.sink());
        QVERIFY(waitUntil([&] { return reports.count() == 2; }));

        const JobReport r1 = reports.byId(first);
        QVERIFY(r1.status == JobStatus::Cancelled);
        QVERIFY(!r1.latest);
        QVERIFY(!r1.usable());
        const JobReport r2 = reports.byId(second);
        QVERIFY(r2.status == JobStatus::Completed);
        QVERIFY(r2.latest);
        QVERIFY(r2.usable());
        QVERIFY2(r1.runMs < 4000.0, "the first job must stop soon after its flag is raised, not run to its timeout");
    }

    // 排隊中的舊工作被取代：根本不必執行（旗標在開始前就升起了）。
    void queuedJobsThatAreSupersededNeverRun() {
        infra::ThreadPoolExecutor executor(1);
        JobRunner runner(executor);
        Reports reports;
        std::atomic<bool> gate{false};
        std::atomic<bool> firstStarted{false};
        std::atomic<bool> secondRan{false};
        std::atomic<bool> thirdRan{false};

        const JobId first = runner.submit(
            [&](const CancelToken&) {  // 佔住唯一的 worker，直到閘門打開（故意不看取消旗標）
                firstStarted = true;
                const auto deadline = std::chrono::steady_clock::now() + 5s;
                while (!gate && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
            },
            reports.sink());
        QVERIFY(waitUntil([&] { return firstStarted.load(); }));
        const JobId second = runner.submit([&](const CancelToken&) { secondRan = true; }, reports.sink());
        const JobId third = runner.submit([&](const CancelToken&) { thirdRan = true; }, reports.sink());
        gate = true;
        QVERIFY(waitUntil([&] { return reports.count() == 3; }));

        QVERIFY(!secondRan);  // 排隊時就被第三個取代
        QVERIFY(thirdRan);
        QVERIFY(reports.byId(first).status == JobStatus::Cancelled);  // 它被取代了，卻不看旗標、跑到結束
        QVERIFY(!reports.byId(first).usable());
        QVERIFY(reports.byId(second).status == JobStatus::Cancelled);
        QVERIFY(reports.byId(third).usable());
    }

    // 呼叫 cancel()（UI 的 Esc）：目前這項被報告為 Cancelled，之後沒有「最新」的工作，直到下一次 submit。
    void cancelMakesTheCurrentJobUnusableAndClearsLatest() {
        infra::ThreadPoolExecutor executor(1);
        JobRunner runner(executor);
        Reports reports;
        std::atomic<bool> started{false};
        const JobId id = runner.submit(
            [&](const CancelToken& token) {
                started = true;
                const auto deadline = std::chrono::steady_clock::now() + 5s;
                while (!token.cancelled() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
            },
            reports.sink());
        QVERIFY(waitUntil([&] { return started.load(); }));
        runner.cancel();
        QCOMPARE(runner.latestId(), JobId{0});
        QVERIFY(!runner.isLatest(id));
        QVERIFY(waitUntil([&] { return reports.count() == 1; }));
        QVERIFY(reports.byId(id).status == JobStatus::Cancelled);
        QVERIFY(!reports.byId(id).usable());

        const JobId next = runner.submit([](const CancelToken&) {}, reports.sink());
        QVERIFY(waitUntil([&] { return reports.count() == 2; }));
        QVERIFY(reports.byId(next).usable());
    }

    // 工作不看旗標、跑到結束：結束時旗標已升起，所以報告是 Cancelled（呼叫端已經放棄這個結果）。
    void aJobThatIgnoresItsFlagIsStillReportedCancelled() {
        infra::ThreadPoolExecutor executor(2);
        JobRunner runner(executor);
        Reports reports;
        std::atomic<bool> started{false};
        std::atomic<bool> release{false};
        const JobId slow = runner.submit(
            [&](const CancelToken&) {
                started = true;
                const auto deadline = std::chrono::steady_clock::now() + 5s;
                while (!release && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
            },
            reports.sink());
        QVERIFY(waitUntil([&] { return started.load(); }));
        runner.submit([](const CancelToken&) {}, reports.sink());  // 取代 slow
        release = true;
        QVERIFY(waitUntil([&] { return reports.count() == 2; }));
        QVERIFY(reports.byId(slow).status == JobStatus::Cancelled);
        QVERIFY(!reports.byId(slow).latest);
    }

    // 已經結束的舊工作不受影響：它結束時仍是最新的，所以是可用的。
    void aJobThatFinishedBeforeTheNextSubmitStaysUsable() {
        infra::InlineExecutor executor;
        JobRunner runner(executor);
        Reports reports;
        const JobId a = runner.submit([](const CancelToken&) {}, reports.sink());
        runner.submit([](const CancelToken&) {}, reports.sink());
        QVERIFY(reports.byId(a).usable());
    }

    // 解構時先 cancel() 再等所有已 submit 的工作結束：work 與 done 捕獲的物件只要活得比 runner 久就安全。
    void destroyingTheRunnerWaitsForPendingJobs() {
        infra::ThreadPoolExecutor executor(1);
        std::atomic<bool> finished{false};
        std::atomic<bool> doneCalled{false};
        {
            JobRunner runner(executor);
            std::atomic<bool> started{false};
            runner.submit(
                [&](const CancelToken&) {
                    started = true;
                    std::this_thread::sleep_for(60ms);  // 不看旗標
                    finished = true;
                },
                [&](const JobReport&) { doneCalled = true; });
            QVERIFY(waitUntil([&] { return started.load(); }));
        }  // 這裡解構
        QVERIFY2(finished, "the destructor must wait until the running job ends");
        QVERIFY(doneCalled);
    }

    void queuedJobsAlsoFinishBeforeTheRunnerIsGone() {
        infra::ThreadPoolExecutor executor(1);
        std::atomic<int> doneCalls{0};
        {
            JobRunner runner(executor);
            for (int i = 0; i < 20; ++i) runner.submit([](const CancelToken&) { std::this_thread::sleep_for(1ms); }, [&](const JobReport&) { ++doneCalls; });
        }
        QCOMPARE(doneCalls.load(), 20);
    }

    // 多執行緒同時 submit：每項工作的 done 恰好被呼叫一次；編號不重複；最後提交的那一項是最新的。
    void concurrentSubmissionsAreAllAccountedFor() {
        infra::ThreadPoolExecutor executor(3);
        JobRunner runner(executor);
        Reports reports;
        constexpr int kThreads = 4;
        constexpr int kPerThread = 100;
        std::vector<std::thread> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&] {
                for (int i = 0; i < kPerThread; ++i) runner.submit([](const CancelToken&) {}, reports.sink());
            });
        }
        for (std::thread& th : threads) th.join();
        QVERIFY(waitUntil([&] { return reports.count() == static_cast<std::size_t>(kThreads * kPerThread); }));

        std::set<JobId> ids;
        JobId maxId = 0;
        for (const JobReport& r : reports.all()) {
            ids.insert(r.id);
            maxId = std::max(maxId, r.id);
        }
        QCOMPARE(ids.size(), static_cast<std::size_t>(kThreads * kPerThread));
        QCOMPARE(maxId, static_cast<JobId>(kThreads * kPerThread));
        QCOMPARE(runner.latestId(), maxId);
        QVERIFY(reports.byId(maxId).usable());
    }

    void theProbeSeesQueueAndRunTimes() {
        struct Spy final : IPerfProbe {
            bool enabled() const noexcept override { return true; }
            void record(const char* name, std::int64_t ns) noexcept override {
                std::lock_guard lock(mutex);
                names.push_back(name);
                total += ns;
            }
            std::mutex mutex;
            std::vector<std::string> names;
            std::int64_t total = 0;
        } spy;
        infra::InlineExecutor executor;
        JobRunner runner(executor, &spy);
        runner.submit([](const CancelToken&) {});
        QCOMPARE(spy.names.size(), std::size_t{2});
        QCOMPARE(spy.names[0], std::string("job.queue"));
        QCOMPARE(spy.names[1], std::string("job.run"));
    }

    // 執行緒池本身：FIFO、解構時執行完已排入的工作。
    void theThreadPoolRunsEveryPostedTaskBeforeItDies() {
        std::atomic<int> ran{0};
        {
            infra::ThreadPoolExecutor pool(2);
            QCOMPARE(pool.concurrency(), std::size_t{2});
            for (int i = 0; i < 50; ++i) pool.post([&] { ++ran; });
        }
        QCOMPARE(ran.load(), 50);
    }

    void aThrowingTaskDoesNotKillTheWorker() {
        std::atomic<int> ran{0};
        {
            infra::ThreadPoolExecutor pool(1);
            pool.post([] { throw std::runtime_error("escaped"); });
            pool.post([&] { ++ran; });
        }
        QCOMPARE(ran.load(), 1);
    }

    void aSingleWorkerRunsTasksInPostOrder() {
        std::vector<int> order;
        {
            infra::ThreadPoolExecutor pool(1);
            for (int i = 0; i < 20; ++i) pool.post([&order, i] { order.push_back(i); });
        }
        QCOMPARE(order.size(), std::size_t{20});
        for (int i = 0; i < 20; ++i) QCOMPARE(order[static_cast<std::size_t>(i)], i);
    }
};

QTEST_APPLESS_MAIN(TstJobRunner)
#include "tst_job_runner.moc"
