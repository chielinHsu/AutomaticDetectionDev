// QtThreadPoolExecutor：IExecutor 的契約（src/application/ports/IExecutor.h），以及放在 JobRunner 下面能不能用。
#include <QtTest>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "application/jobs/JobRunner.h"
#include "infrastructure/threading/QtThreadPoolExecutor.h"

using namespace aoi;
using namespace std::chrono_literals;

namespace {

template <typename Pred>
bool waitUntil(Pred pred, std::chrono::milliseconds timeout = 5000ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!pred()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

}  // namespace

class TstQtExecutor : public QObject {
    Q_OBJECT

private slots:
    void tasksRunOnAWorkerThreadNotTheCaller() {
        infra::QtThreadPoolExecutor executor(1);
        std::atomic<bool> ran{false};
        std::thread::id workerId;
        executor.post([&] {
            workerId = std::this_thread::get_id();
            ran = true;
        });
        QVERIFY(waitUntil([&] { return ran.load(); }));
        QVERIFY(workerId != std::this_thread::get_id());
    }

    void postDoesNotBlockTheCallerWhileATaskRuns() {
        infra::QtThreadPoolExecutor executor(1);
        std::atomic<bool> release{false};
        std::atomic<bool> started{false};
        executor.post([&] {
            started = true;
            while (!release) std::this_thread::sleep_for(1ms);
        });
        QVERIFY(waitUntil([&] { return started.load(); }));
        const auto t0 = std::chrono::steady_clock::now();
        executor.post([] {});  // 排在忙碌的 worker 後面
        const auto elapsed = std::chrono::steady_clock::now() - t0;
        QVERIFY(elapsed < 200ms);
        release = true;
    }

    void withOneWorkerTasksStartInPostOrder() {
        std::vector<int> order;
        std::mutex mutex;
        {
            infra::QtThreadPoolExecutor executor(1);
            for (int i = 0; i < 2000; ++i) {
                executor.post([&, i] {
                    std::lock_guard lock(mutex);
                    order.push_back(i);
                });
            }
        }  // 解構等全部做完
        QCOMPARE(order.size(), std::size_t{2000});
        for (int i = 0; i < 2000; ++i) {
            if (order[static_cast<std::size_t>(i)] != i) QFAIL(qPrintable(QString("position %1 holds %2").arg(i).arg(order[static_cast<std::size_t>(i)])));
        }
    }

    void concurrencyIsTheWorkerCountAndAtLeastOne() {
        QCOMPARE(infra::QtThreadPoolExecutor(3).concurrency(), std::size_t{3});
        QCOMPARE(infra::QtThreadPoolExecutor(0).concurrency(), std::size_t{1});
    }

    void twoWorkersReallyRunTwoTasksAtTheSameTime() {
        infra::QtThreadPoolExecutor executor(2);
        std::atomic<int> arrived{0};
        std::atomic<int> sawBoth{0};
        for (int i = 0; i < 2; ++i) {
            executor.post([&] {
                ++arrived;
                // 一個任務要等到另一個也開始，才算同時；單一 worker 時會逾時。
                if (waitUntil([&] { return arrived.load() == 2; }, 2000ms)) ++sawBoth;
            });
        }
        QVERIFY(waitUntil([&] { return sawBoth.load() == 2; }));
    }

    void destructionRunsEverythingAlreadyQueued() {
        std::atomic<int> done{0};
        {
            infra::QtThreadPoolExecutor executor(2);
            for (int i = 0; i < 100; ++i) {
                executor.post([&] {
                    std::this_thread::sleep_for(100us);
                    ++done;
                });
            }
        }
        QCOMPARE(done.load(), 100);
    }

    void anExceptionInATaskIsSwallowedAndTheWorkerKeepsGoing() {
        infra::QtThreadPoolExecutor executor(1);
        std::atomic<int> after{0};
        executor.post([] { throw std::runtime_error("boom"); });
        executor.post([] { throw 42; });
        executor.post([&] { ++after; });
        QVERIFY(waitUntil([&] { return after.load() == 1; }));
    }

    void aNullTaskIsIgnored() {
        infra::QtThreadPoolExecutor executor(1);
        executor.post({});
        std::atomic<bool> ran{false};
        executor.post([&] { ran = true; });
        QVERIFY(waitUntil([&] { return ran.load(); }));
    }

    void workersDoNotExpireWhileIdle() {
        // 搜尋之間隔幾秒是常態：worker 閒置之後不能消失，否則下一個工作要多等一次執行緒啟動。
        infra::QtThreadPoolExecutor executor(1);
        std::atomic<bool> first{false};
        std::thread::id firstId;
        executor.post([&] {
            firstId = std::this_thread::get_id();
            first = true;
        });
        QVERIFY(waitUntil([&] { return first.load(); }));
        std::this_thread::sleep_for(300ms);
        std::atomic<bool> second{false};
        std::thread::id secondId;
        executor.post([&] {
            secondId = std::this_thread::get_id();
            second = true;
        });
        QVERIFY(waitUntil([&] { return second.load(); }));
        QVERIFY(firstId == secondId);
    }

    void jobRunnerWorksOnTopOfIt() {
        infra::QtThreadPoolExecutor executor(2);
        std::atomic<int> completed{0};
        std::atomic<bool> firstSawCancel{false};
        std::atomic<bool> firstStarted{false};
        {
            JobRunner runner(executor);
            runner.submit(
                [&](const CancelToken& cancel) {
                    firstStarted = true;
                    for (int i = 0; i < 2000 && !cancel.cancelled(); ++i) std::this_thread::sleep_for(1ms);
                    firstSawCancel = cancel.cancelled();
                },
                [&](const JobReport&) {});
            // 第一項要真的開始了：還沒開始就被取代的工作，JobRunner 會直接跳過它的 work（那是另一條規則，tst_job_runner 驗證）。
            QVERIFY(waitUntil([&] { return firstStarted.load(); }));
            const JobId second = runner.submit([](const CancelToken&) {}, [&](const JobReport& r) {
                if (r.usable()) ++completed;
            });
            QVERIFY(waitUntil([&] { return completed.load() == 1; }));
            QVERIFY(runner.isLatest(second));
        }
        QVERIFY(firstSawCancel.load());  // 新工作 submit 時，進行中的舊工作的取消旗標升起（latest-wins）
    }
};

QTEST_APPLESS_MAIN(TstQtExecutor)
#include "tst_qt_executor.moc"
