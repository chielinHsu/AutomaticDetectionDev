// 執行中切換 back end 的規則（ADR-021）：BackendSwitch、啟動時用哪一個、SearchEngine 的擁有權。
// 規則都在沒有 Qt 的 application 層，所以每一條都能決定性地驗證；Qt 這一側（背景執行緒、畫面）在 tst_presenter。
#include <QtTest>
#include <atomic>
#include <chrono>
#include <thread>

#include "CoreTestSupport.h"
#include "QtTestPrinters.h"
#include "application/backend/BackendSwitch.h"
#include "application/backend/SearchEngine.h"
#include "application/jobs/JobRunner.h"
#include "infrastructure/threading/ThreadPoolExecutor.h"

using namespace aoi;
using namespace aoi::test;
using namespace std::chrono_literals;
using S = InteractionState;
using Outcome = BackendSwitch::Outcome;
using Request = BackendSwitch::Request;

namespace {

BackendInfo info(const char* id, bool available = true, const char* reason = "") {
    BackendInfo b;
    b.id = id;
    b.displayName = id;
    b.available = available;
    if (!available) b.unavailable = makeError(ErrorCode::BackendUnavailable, reason);
    return b;
}

// fake、opencv（可用）、halcon（沒編進程式）
std::vector<BackendInfo> threeBackends() {
    return {info("fake"), info("opencv"), info("halcon", false, "not built in")};
}

// 會記下自己什麼時候被釋放的 adapter：驗證「舊的 back end 等工作結束才釋放」。
struct Lifetime {
    std::atomic<bool> builderAlive{true};
    std::atomic<bool> searcherAlive{true};
};

class TrackedBuilder final : public ITemplateBuilder {
public:
    explicit TrackedBuilder(std::shared_ptr<Lifetime> life) : life_(std::move(life)) {}
    ~TrackedBuilder() override { life_->builderAlive = false; }
    std::string_view backendId() const noexcept override { return inner_.backendId(); }
    Result<TemplatePtr> build(const ImageView& image, const Rect& sample, const SearchParams& params,
                              const CancelToken& cancel) override {
        return inner_.build(image, sample, params, cancel);
    }

private:
    std::shared_ptr<Lifetime> life_;
    fake::FakeTemplateBuilder inner_;
};

// 搜尋卡在閘門上（模擬不可中斷的函式庫呼叫），直到測試放行；不看取消旗標。
class GatedSearcher final : public IPatternSearcher {
public:
    GatedSearcher(std::shared_ptr<Lifetime> life, std::shared_ptr<std::atomic<bool>> open, std::shared_ptr<std::atomic<bool>> entered)
        : life_(std::move(life)), open_(std::move(open)), entered_(std::move(entered)) {}
    ~GatedSearcher() override { life_->searcherAlive = false; }
    std::string_view backendId() const noexcept override { return inner_.backendId(); }
    Result<std::vector<Match>> find(const ImageView& image, const ITemplate& tmpl, const SearchRegion& region,
                                    const SearchParams& params, const CancelToken&) override {
        *entered_ = true;
        const auto deadline = std::chrono::steady_clock::now() + 10s;
        while (!*open_ && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
        // 還活著才能被呼叫：如果 adapter 已經被釋放，這裡讀到的就是 false（測試會抓到）。
        sawAliveAfterGate = life_->builderAlive && life_->searcherAlive;
        return inner_.find(image, tmpl, region, params, CancelToken{});
    }
    static inline std::atomic<bool> sawAliveAfterGate{false};

private:
    std::shared_ptr<Lifetime> life_;
    std::shared_ptr<std::atomic<bool>> open_;
    std::shared_ptr<std::atomic<bool>> entered_;
    fake::FakeSearcher inner_;
};

}  // namespace

class TstBackendSwitch : public QObject {
    Q_OBJECT

private slots:
    // ---- 要求 ----
    void selectingAnotherAvailableBackendStartsASwitch() {
        BackendSwitch sw(threeBackends(), "fake");
        QVERIFY(!sw.switching());
        const auto r = sw.request("opencv");
        QVERIFY(r.kind == Request::Start);
        QVERIFY(sw.switching());
        QCOMPARE(sw.pending(), std::string("opencv"));
        QCOMPARE(sw.current(), std::string("fake"));  // 建好之前仍用原來的
    }

    void selectingTheCurrentBackendDoesNothing() {
        BackendSwitch sw(threeBackends(), "fake");
        QVERIFY(sw.request("fake").kind == Request::AlreadyCurrent);
        QVERIFY(!sw.switching());
    }

    void anUnavailableBackendIsRefusedWithItsReasonWithoutTrying() {
        BackendSwitch sw(threeBackends(), "fake");
        const auto r = sw.request("halcon");
        QVERIFY(r.kind == Request::Unavailable);
        QVERIFY(r.error.code == ErrorCode::BackendUnavailable);
        QCOMPARE(r.error.message, std::string("not built in"));
        QVERIFY(!sw.switching());
        QCOMPARE(sw.current(), std::string("fake"));
    }

    void anUnknownIdIsRefused() {
        BackendSwitch sw(threeBackends(), "fake");
        const auto r = sw.request("turbo");
        QVERIFY(r.kind == Request::Unknown);
        QVERIFY(r.error.code == ErrorCode::NotFound);
        QVERIFY(r.error.message.find("turbo") != std::string::npos);
        QVERIFY(!sw.switching());
    }

    void onlyOneSwitchAtATime() {
        BackendSwitch sw(threeBackends(), "fake");
        QVERIFY(sw.request("opencv").kind == Request::Start);
        QVERIFY(sw.request("fake").kind == Request::Busy);  // 連「換回目前的」也不收：要等這一次結束
        QCOMPARE(sw.pending(), std::string("opencv"));
    }

    // ---- 完成 ----
    void successMakesTheNewBackendCurrent() {
        BackendSwitch sw(threeBackends(), "fake");
        QVERIFY(sw.request("opencv").kind == Request::Start);
        QVERIFY(sw.finish("opencv", true) == Outcome::Switched);
        QCOMPARE(sw.current(), std::string("opencv"));
        QVERIFY(!sw.switching());
        QVERIFY(sw.request("fake").kind == Request::Start);  // 之後可以再換
    }

    void failureGoesBackToTheOriginalBackend() {
        BackendSwitch sw(threeBackends(), "fake");
        QVERIFY(sw.request("opencv").kind == Request::Start);
        QVERIFY(sw.finish("opencv", false) == Outcome::Failed);
        QCOMPARE(sw.current(), std::string("fake"));
        QVERIFY(!sw.switching());
    }

    void aResultForAnotherIdIsStale() {
        BackendSwitch sw(threeBackends(), "fake");
        QVERIFY(sw.finish("opencv", true) == Outcome::Stale);  // 沒有在換
        QVERIFY(sw.request("opencv").kind == Request::Start);
        QVERIFY(sw.finish("halcon", true) == Outcome::Stale);  // 換的不是它
        QVERIFY(sw.switching());
        QCOMPARE(sw.current(), std::string("fake"));
    }

    // ---- 搜尋怎麼辦（使用者 2026-10-08 決定：畫面上有提案時用新的 back end 重新搜尋）----
    void aSearchInProgressIsPausedWhileSwitching() {
        QVERIFY(BackendSwitch::pausesSearch(S::Searching));
        for (S s : {S::Empty, S::Idle, S::DrawingRegion, S::Preview, S::NoMatch, S::Error, S::EditingType})
            QVERIFY(!BackendSwitch::pausesSearch(s));
    }

    void afterASwitchTheSameRegionIsSearchedAgain() {
        for (S s : {S::Searching, S::Preview, S::NoMatch}) QVERIFY(BackendSwitch::researchAfter(Outcome::Switched, s));
        for (S s : {S::Empty, S::Idle, S::DrawingRegion, S::Error, S::EditingType})
            QVERIFY(!BackendSwitch::researchAfter(Outcome::Switched, s));
    }

    void afterAFailedSwitchOnlyThePausedSearchContinues() {
        QVERIFY(BackendSwitch::researchAfter(Outcome::Failed, S::Searching));  // 用原來的 back end 接著做
        for (S s : {S::Empty, S::Idle, S::DrawingRegion, S::Preview, S::NoMatch, S::Error, S::EditingType})
            QVERIFY(!BackendSwitch::researchAfter(Outcome::Failed, s));  // 維持原狀
        for (S s : {S::Searching, S::Preview, S::NoMatch}) QVERIFY(!BackendSwitch::researchAfter(Outcome::Stale, s));
    }

    void theFallbackIsTheFirstOtherAvailableBackend() {
        QCOMPARE(BackendSwitch(threeBackends(), "fake").fallback(), std::string("opencv"));
        QCOMPARE(BackendSwitch(threeBackends(), "opencv").fallback(), std::string("fake"));
        QCOMPARE(BackendSwitch({info("fake"), info("halcon", false)}, "fake").fallback(), std::string());
    }

    // ---- 啟動（使用者 2026-10-08 決定：記住上次）----
    void theCommandLineWinsAndIsStrict() {
        const StartupPlan plan = planStartupBackend(std::string("halcon"), std::string("opencv"), "fake");
        QCOMPARE(plan.initial, std::string("halcon"));
        QVERIFY(plan.strict);
        QVERIFY(plan.switchTo.empty());
    }

    void theRememberedBackendIsSwitchedToAfterTheWindowAppears() {
        const StartupPlan plan = planStartupBackend(std::nullopt, std::string("halcon"), "fake");
        QCOMPARE(plan.initial, std::string("fake"));  // 視窗馬上出現
        QVERIFY(!plan.strict);
        QCOMPARE(plan.switchTo, std::string("halcon"));
    }

    void withoutAMemoryOrWhenItIsTheDefaultNothingIsSwitched() {
        for (const std::optional<std::string>& remembered : {std::optional<std::string>{}, std::optional<std::string>{""},
                                                             std::optional<std::string>{"fake"}}) {
            const StartupPlan plan = planStartupBackend(std::nullopt, remembered, "fake");
            QCOMPARE(plan.initial, std::string("fake"));
            QVERIFY(!plan.strict);
            QVERIFY(plan.switchTo.empty());
        }
    }

    // ---- SearchEngine ----
    void makingAnEngineWiresTheUseCaseToTheBackend() {
        BackendRegistry registry;
        QVERIFY(registry.registerBackend("fake", "Fake", [] { return Result<Backend>(fake::makeFakeBackend()); }).ok());
        Result<SearchEnginePtr> engine = makeSearchEngine(registry, "fake");
        QVERIFY2(engine.ok(), engine.ok() ? "" : engine.error().message.c_str());
        QCOMPARE(engine.value()->backend.id, std::string("fake"));
        const TestImage image(800, 600);
        const Result<SampleProposal> r = useCaseOf(engine.value())->run(makeRequest(image, {380, 285, 40, 30}), CancelToken());
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        QVERIFY(!r.value().candidates.empty());
    }

    void makingAnEngineReportsTheRegistryError() {
        BackendRegistry registry;
        QVERIFY(registry.registerUnavailable("halcon", "HALCON", "no license").ok());
        const Result<SearchEnginePtr> unavailable = makeSearchEngine(registry, "halcon");
        QVERIFY(!unavailable.ok());
        QVERIFY(unavailable.error().code == ErrorCode::BackendUnavailable);
        QCOMPARE(unavailable.error().message, std::string("no license"));
        QVERIFY(makeSearchEngine(registry, "turbo").error().code == ErrorCode::NotFound);
    }

    // 換掉 back end 時（Presenter 放開它的那一份），進行中的工作仍持有一份：adapter 等工作結束才釋放，不會被用到已釋放的。
    void theOldBackendLivesUntilItsLastJobFinishes() {
        auto life = std::make_shared<Lifetime>();
        auto open = std::make_shared<std::atomic<bool>>(false);
        auto entered = std::make_shared<std::atomic<bool>>(false);
        BackendRegistry registry;
        QVERIFY(registry
                    .registerBackend("gated", "Gated",
                                     [life, open, entered] {
                                         Backend b;
                                         b.id = "gated";
                                         b.displayName = "Gated";
                                         b.templateBuilder = std::make_unique<TrackedBuilder>(life);
                                         b.searcher = std::make_unique<GatedSearcher>(life, open, entered);
                                         return Result<Backend>(std::move(b));
                                     })
                    .ok());
        Result<SearchEnginePtr> made = makeSearchEngine(registry, "gated");
        QVERIFY(made.ok());
        SearchEnginePtr current = std::move(made.value());  // 只剩 Presenter 的這一份（made 不再持有）

        infra::ThreadPoolExecutor executor(2);
        JobRunner runner(executor);
        const TestImage image(800, 600);
        const SampleRequest request = makeRequest(image, {380, 285, 40, 30});
        std::atomic<bool> finished{false};
        GatedSearcher::sawAliveAfterGate = false;
        runner.submit([useCase = useCaseOf(current), request](const CancelToken& token) { (void)useCase->run(request, token); },
                      [&](const JobReport&) { finished = true; });
        QTRY_VERIFY_WITH_TIMEOUT(entered->load(), 5000);

        current.reset();  // 換 back end：Presenter 放開舊的；工作還卡在搜尋器裡
        runner.cancel();
        QVERIFY(life->builderAlive);
        QVERIFY(life->searcherAlive);

        *open = true;  // 放行：工作做完、放開它的那一份
        QTRY_VERIFY_WITH_TIMEOUT(finished.load(), 5000);
        QVERIFY2(GatedSearcher::sawAliveAfterGate, "the adapters were alive while the job used them");
        QTRY_VERIFY_WITH_TIMEOUT(!life->searcherAlive.load(), 5000);  // 最後一份放開之後才釋放
        QVERIFY(!life->builderAlive);
    }
};

QTEST_GUILESS_MAIN(TstBackendSwitch)
#include "tst_backend_switch.moc"
