// S06：RecipeSaver（背景存配方）。假的 store 可以卡住、失敗或丟例外；executor 是真的執行緒池。
#include <QtTest>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "QtTestPrinters.h"
#include "application/document/RecipeSaver.h"
#include "infrastructure/threading/ThreadPoolExecutor.h"

using namespace aoi;
using namespace std::chrono_literals;

namespace {

Recipe recipeNamed(const std::string& name) {
    Recipe r;
    r.board.name = name;
    return r;
}

// 記下每次 save（路徑、配方名稱、執行緒）。gate 關著時，路徑在 blocked 裡的存檔會停在 save 裡等。
class FakeStore final : public IRecipeStore {
public:
    Result<void> save(const Recipe& recipe, std::string_view path) override {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            ++active_[std::string(path)];
            maxActiveSamePath_ = std::max(maxActiveSamePath_, active_[std::string(path)]);
            entered_.notify_all();
            released_.wait(lock, [&] { return blocked_.count(std::string(path)) == 0; });
        }
        if (delay.count() > 0) std::this_thread::sleep_for(delay);
        std::lock_guard<std::mutex> lock(mutex_);
        --active_[std::string(path)];
        writes.push_back({std::string(path), recipe.board.name});
        threads.insert(std::this_thread::get_id());
        if (throwOn == recipe.board.name) throw std::runtime_error("boom");
        if (failOn == recipe.board.name) return makeError(ErrorCode::IoFailed, "disk full");
        return {};
    }
    Result<Recipe> load(std::string_view) override { return makeError(ErrorCode::NotFound, "not used"); }

    void block(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        blocked_.insert(path);
    }
    void release(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        blocked_.erase(path);
        released_.notify_all();
    }
    // 等到 path 有一個存檔正在 save 裡。
    bool waitUntilWriting(const std::string& path) {
        std::unique_lock<std::mutex> lock(mutex_);
        return entered_.wait_for(lock, 5s, [&] { return active_[path] > 0; });
    }
    std::vector<std::pair<std::string, std::string>> writesSnapshot() {
        std::lock_guard<std::mutex> lock(mutex_);
        return writes;
    }

    std::vector<std::pair<std::string, std::string>> writes;
    std::set<std::thread::id> threads;
    std::string failOn;
    std::string throwOn;
    std::chrono::milliseconds delay{0};
    int maxActiveSamePath_ = 0;

private:
    std::mutex mutex_;
    std::condition_variable entered_;
    std::condition_variable released_;
    std::set<std::string> blocked_;
    std::map<std::string, int> active_;
};

// 收集結果（done 在 worker 上被呼叫）。
struct Outcomes {
    std::mutex mutex;
    std::map<std::uint64_t, RecipeSaver::Outcome> byTicket;
    std::map<std::uint64_t, int> calls;
    std::set<std::thread::id> threads;

    RecipeSaver::Done sink() {
        return [this](const RecipeSaver::Outcome& o) {
            std::lock_guard<std::mutex> lock(mutex);
            byTicket.insert_or_assign(o.ticket, o);
            ++calls[o.ticket];
            threads.insert(std::this_thread::get_id());
        };
    }
    ErrorCode code(std::uint64_t ticket) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto& r = byTicket.at(ticket).result;
        return r.ok() ? ErrorCode::Internal : r.error().code;  // 呼叫端先確認 ok
    }
    bool ok(std::uint64_t ticket) {
        std::lock_guard<std::mutex> lock(mutex);
        return byTicket.at(ticket).result.ok();
    }
};

}  // namespace

class TstRecipeSaver : public QObject {
    Q_OBJECT

private slots:
    // 呼叫端（UI）不等寫檔：store 卡住時 save() 照樣立刻回來；done 在 worker 執行緒上。
    void saveReturnsWhileTheStoreIsStillWriting() {
        FakeStore store;
        infra::ThreadPoolExecutor executor(2);
        Outcomes outcomes;
        RecipeSaver saver(store, executor);
        store.block("a.json");
        const std::uint64_t t = saver.save(recipeNamed("v1"), "a.json", outcomes.sink());
        QVERIFY(store.waitUntilWriting("a.json"));
        QCOMPARE(saver.pending(), std::size_t{1});
        QVERIFY(outcomes.byTicket.empty());  // 還沒寫完
        store.release("a.json");
        saver.waitIdle();
        QVERIFY(outcomes.ok(t));
        QCOMPARE(saver.pending(), std::size_t{0});
        QCOMPARE(outcomes.threads.count(std::this_thread::get_id()), std::size_t{0});
    }

    // 同一個路徑：寫入中又來兩份，中間那份還沒開始就被最新的取代（Cancelled），磁碟上最後是最新的。
    void aNewerSaveReplacesAQueuedOneOfTheSameFile() {
        FakeStore store;
        infra::ThreadPoolExecutor executor(4);
        Outcomes outcomes;
        RecipeSaver saver(store, executor);
        store.block("a.json");
        const std::uint64_t t1 = saver.save(recipeNamed("v1"), "a.json", outcomes.sink());
        QVERIFY(store.waitUntilWriting("a.json"));
        const std::uint64_t t2 = saver.save(recipeNamed("v2"), "a.json", outcomes.sink());
        const std::uint64_t t3 = saver.save(recipeNamed("v3"), "a.json", outcomes.sink());
        store.release("a.json");
        saver.waitIdle();

        QVERIFY(t1 < t2 && t2 < t3);
        QVERIFY(outcomes.ok(t1));
        QCOMPARE_CODE(outcomes.code(t2), ErrorCode::Cancelled);
        QVERIFY(outcomes.ok(t3));
        const auto writes = store.writesSnapshot();
        QCOMPARE(writes.size(), std::size_t{2});
        QCOMPARE(writes[0].second, std::string("v1"));
        QCOMPARE(writes[1].second, std::string("v3"));
        QCOMPARE(store.maxActiveSamePath_, 1);  // 同一個路徑從來沒有兩個同時在寫
        for (const auto& [ticket, n] : outcomes.calls) QCOMPARE(n, 1);
    }

    // 不同路徑互不等待：a 卡住時 b 照樣寫完。
    void differentFilesDoNotWaitForEachOther() {
        FakeStore store;
        infra::ThreadPoolExecutor executor(2);
        Outcomes outcomes;
        RecipeSaver saver(store, executor);
        store.block("a.json");
        const std::uint64_t ta = saver.save(recipeNamed("a1"), "a.json", outcomes.sink());
        QVERIFY(store.waitUntilWriting("a.json"));
        const std::uint64_t tb = saver.save(recipeNamed("b1"), "b.json", outcomes.sink());
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            std::lock_guard<std::mutex> lock(outcomes.mutex);
            return outcomes.byTicket.count(tb) == 1;
        }(), 5000);
        QVERIFY(outcomes.ok(tb));
        {
            std::lock_guard<std::mutex> lock(outcomes.mutex);
            QCOMPARE(outcomes.byTicket.count(ta), std::size_t{0});
        }
        store.release("a.json");
        saver.waitIdle();
        QVERIFY(outcomes.ok(ta));
    }

    void storeErrorsAndExceptionsBecomeOutcomes() {
        FakeStore store;
        store.failOn = "bad";
        store.throwOn = "boom";
        infra::ThreadPoolExecutor executor(1);
        Outcomes outcomes;
        RecipeSaver saver(store, executor);
        const std::uint64_t tBad = saver.save(recipeNamed("bad"), "a.json", outcomes.sink());
        saver.waitIdle();
        const std::uint64_t tBoom = saver.save(recipeNamed("boom"), "b.json", outcomes.sink());
        saver.waitIdle();
        const std::uint64_t tGood = saver.save(recipeNamed("good"), "a.json", outcomes.sink());
        saver.waitIdle();
        QCOMPARE_CODE(outcomes.code(tBad), ErrorCode::IoFailed);
        QCOMPARE_CODE(outcomes.code(tBoom), ErrorCode::Internal);
        QVERIFY(outcomes.ok(tGood));
    }

    // done 丟例外不影響之後的存檔。
    void aThrowingCallbackDoesNotStopLaterSaves() {
        FakeStore store;
        infra::ThreadPoolExecutor executor(1);
        Outcomes outcomes;
        RecipeSaver saver(store, executor);
        saver.save(recipeNamed("v1"), "a.json", [](const RecipeSaver::Outcome&) { throw std::runtime_error("ui gone"); });
        const std::uint64_t t2 = saver.save(recipeNamed("v2"), "a.json", outcomes.sink());
        saver.waitIdle();
        QVERIFY(outcomes.ok(t2));
        QCOMPARE(store.writesSnapshot().back().second, std::string("v2"));
    }

    // 解構時等寫完：使用者交出的資料不會因為程式關閉而丟掉。
    void destructionWaitsForPendingWrites() {
        FakeStore store;
        store.delay = 60ms;
        infra::ThreadPoolExecutor executor(2);
        {
            RecipeSaver saver(store, executor);
            saver.save(recipeNamed("last words"), "a.json", nullptr);
        }
        const auto writes = store.writesSnapshot();
        QCOMPARE(writes.size(), std::size_t{1});
        QCOMPARE(writes[0].second, std::string("last words"));
    }

    // 很多執行緒同時存三個檔案：每個檔案最後寫下的是編號最大的那一份；每個編號恰好一個結果；
    // 同一個檔案從來沒有兩個同時在寫。
    void concurrentSavesKeepTheLatestOfEachFile() {
        FakeStore store;
        store.delay = 1ms;
        infra::ThreadPoolExecutor executor(4);
        Outcomes outcomes;
        std::mutex submittedMutex;
        std::map<std::uint64_t, std::pair<std::string, std::string>> submitted;  // 編號 → (路徑, 名稱)
        {
            RecipeSaver saver(store, executor);
            std::vector<std::thread> callers;
            for (int c = 0; c < 4; ++c) {
                callers.emplace_back([&, c] {
                    for (int i = 0; i < 150; ++i) {
                        const std::string path = std::string("f") + std::to_string((c + i) % 3) + ".json";
                        const std::string name = std::to_string(c) + "-" + std::to_string(i);
                        // 編號在 save 的鎖內決定；先記在這個執行緒，save 回來再登記（登記晚不影響比對）。
                        const std::uint64_t t = saver.save(recipeNamed(name), path, outcomes.sink());
                        std::lock_guard<std::mutex> lock(submittedMutex);
                        submitted[t] = {path, name};
                    }
                });
            }
            for (std::thread& t : callers) t.join();
            saver.waitIdle();
            QCOMPARE(saver.pending(), std::size_t{0});
        }
        QCOMPARE(submitted.size(), std::size_t{600});
        QCOMPARE(outcomes.calls.size(), std::size_t{600});
        for (const auto& [ticket, n] : outcomes.calls) QCOMPARE(n, 1);
        QCOMPARE(store.maxActiveSamePath_, 1);

        std::map<std::string, std::uint64_t> newest;  // 路徑 → 最大的編號
        for (const auto& [ticket, pn] : submitted) newest[pn.first] = std::max(newest[pn.first], ticket);
        std::map<std::string, std::string> lastWritten;
        for (const auto& [path, name] : store.writesSnapshot()) lastWritten[path] = name;
        for (const auto& [path, ticket] : newest) QCOMPARE(lastWritten[path], submitted[ticket].second);
        // 寫了的都成功、被取代的都是 Cancelled，沒有別的結果
        std::size_t written = 0, replaced = 0;
        for (const auto& [ticket, o] : outcomes.byTicket) {
            if (o.result.ok()) ++written;
            else if (o.result.error().code == ErrorCode::Cancelled) ++replaced;
        }
        QCOMPARE(written + replaced, std::size_t{600});
        QCOMPARE(written, store.writesSnapshot().size());
    }
};

QTEST_GUILESS_MAIN(TstRecipeSaver)
#include "tst_recipe_saver.moc"
