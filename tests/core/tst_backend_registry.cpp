// BackendRegistry 與組裝根：新增 back end 不改核心；不可用的 back end 給明確的錯誤。
#include <QtTest>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "CoreTestSupport.h"
#include "app/CompositionRoot.h"
#include "application/backend/BackendRegistry.h"

using namespace aoi;
using namespace aoi::test;
using namespace std::chrono_literals;

namespace {

// 一個「全新的 back end」：不是假 back end，也不是 OpenCV。它只回報固定的兩個匹配（樣本本身，加一個鄰居）。
// 它只實作兩個 port；use case、domain、BackendRegistry、JobRunner 都不認識它。
class ScriptedTemplate final : public ITemplate {
public:
    explicit ScriptedTemplate(Size2D s) : size_(s) {}
    Size2D size() const noexcept override { return size_; }
    std::string_view backendId() const noexcept override { return "scripted"; }

private:
    Size2D size_;
};

class ScriptedBuilder final : public ITemplateBuilder {
public:
    std::string_view backendId() const noexcept override { return "scripted"; }
    Result<TemplatePtr> build(const ImageView&, const Rect& sample, const SearchParams&, const CancelToken&) override {
        return TemplatePtr(std::make_shared<const ScriptedTemplate>(sample.size()));
    }
};

class ScriptedSearcher final : public IPatternSearcher {
public:
    std::string_view backendId() const noexcept override { return "scripted"; }
    Result<std::vector<Match>> find(const ImageView&, const ITemplate& tmpl, const SearchRegion&, const SearchParams&,
                                    const CancelToken&) override {
        if (tmpl.backendId() != "scripted") return makeError(ErrorCode::BackendMismatch, "not mine");
        return std::vector<Match>{{{400.0, 300.0, 0.0}, 0.99}, {{460.0, 300.0, 0.0}, 0.93}};
    }
};

Backend makeScriptedBackend() {
    Backend b;
    b.id = "scripted";
    b.displayName = "Scripted";
    b.templateBuilder = std::make_unique<ScriptedBuilder>();
    b.searcher = std::make_unique<ScriptedSearcher>();
    return b;
}

}  // namespace

class TstBackendRegistry : public QObject {
    Q_OBJECT

private slots:
    void registeredBackendsAreListedInOrderWithTheirAvailability() {
        BackendRegistry registry;
        QVERIFY(registry.registerBackend("a", "Alpha", [] { return Result<Backend>(makeScriptedBackend()); }).ok());
        QVERIFY(registry.registerUnavailable("b", "Beta", "no licence").ok());
        const std::vector<BackendInfo> list = registry.list();
        QCOMPARE(list.size(), std::size_t{2});
        QCOMPARE(list[0].id, std::string("a"));
        QVERIFY(list[0].available);
        QCOMPARE(list[1].id, std::string("b"));
        QVERIFY(!list[1].available);
        QVERIFY(list[1].unavailable.code == ErrorCode::BackendUnavailable);
        QCOMPARE(list[1].unavailable.message, std::string("no licence"));
    }

    void duplicateAndEmptyIdsAreRefused() {
        BackendRegistry registry;
        QVERIFY(registry.registerBackend("a", "Alpha", [] { return Result<Backend>(makeScriptedBackend()); }).ok());
        const Result<void> duplicate = registry.registerBackend("a", "Again", [] { return Result<Backend>(makeScriptedBackend()); });
        QVERIFY(!duplicate.ok());
        QVERIFY(duplicate.error().code == ErrorCode::InvalidArgument);
        QVERIFY(!registry.registerUnavailable("a", "Again", "x").ok());
        QVERIFY(!registry.registerBackend("", "Empty", [] { return Result<Backend>(makeScriptedBackend()); }).ok());
        QVERIFY(!registry.registerBackend("c", "NoFactory", {}).ok());
        QCOMPARE(registry.list().size(), std::size_t{1});  // 失敗的註冊沒有留下東西
    }

    void creatingAnUnknownIdIsNotFound() {
        const BackendRegistry registry;
        const Result<Backend> r = registry.create("nope");
        QVERIFY(!r.ok());
        QVERIFY(r.error().code == ErrorCode::NotFound);
    }

    void creatingAnUnavailableBackendReturnsItsReason() {
        BackendRegistry registry;
        QVERIFY(registry.registerUnavailable("halcon", "HALCON", "no licence on this PC").ok());
        const Result<Backend> r = registry.create("halcon");
        QVERIFY(!r.ok());
        QVERIFY(r.error().code == ErrorCode::BackendUnavailable);
        QCOMPARE(r.error().message, std::string("no licence on this PC"));
    }

    void everyCreateBuildsAFreshBackend() {
        BackendRegistry registry;
        QVERIFY(registry.registerBackend("a", "Alpha", [] { return Result<Backend>(makeScriptedBackend()); }).ok());
        Result<Backend> first = registry.create("a");
        Result<Backend> second = registry.create("a");
        QVERIFY(first.ok() && second.ok());
        QVERIFY(first.value().searcher.get() != second.value().searcher.get());
    }

    void aFailingFactoryIsReportedNotThrown() {
        BackendRegistry registry;
        QVERIFY(registry.registerBackend("bad", "Bad", [] { return Result<Backend>(makeError(ErrorCode::BackendFailed, "init failed")); }).ok());
        const Result<Backend> r = registry.create("bad");
        QVERIFY(!r.ok());
        QVERIFY(r.error().code == ErrorCode::BackendFailed);
    }

    void anIncompleteBackendIsAnInternalError() {
        BackendRegistry registry;
        QVERIFY(registry.registerBackend("half", "Half", [] {
                    Backend b;
                    b.id = "half";
                    b.templateBuilder = std::make_unique<ScriptedBuilder>();  // 沒有 searcher
                    return Result<Backend>(std::move(b));
                }).ok());
        const Result<Backend> r = registry.create("half");
        QVERIFY(!r.ok());
        QVERIFY(r.error().code == ErrorCode::Internal);
    }

    // ---- 組裝根 ----

    // OpenCV 從 S07 起有實作：AOI_WITH_OPENCV=ON 的建置是可用的，否則登記成不可用（附怎麼編進來）。HALCON 等 S08。
    void theProgramRegistersFakeOpenCvAndHalcon() {
        const BackendRegistry registry = app::makeBackendRegistry();
        const std::vector<BackendInfo> list = registry.list();
        QCOMPARE(list.size(), std::size_t{3});
        QCOMPARE(list[0].id, std::string("fake"));
        QVERIFY(list[0].available);
        QCOMPARE(list[1].id, std::string("opencv"));
#ifdef AOI_WITH_OPENCV
        QVERIFY(list[1].available);
#else
        QVERIFY(!list[1].available);
        QVERIFY(list[1].unavailable.message.find("AOI_WITH_OPENCV=ON") != std::string::npos);
#endif
        QCOMPARE(list[2].id, std::string("halcon"));
#ifdef AOI_WITH_HALCON
        // S08：編進程式時，PATH 上找得到 halconcpp.dll 就登記成可用（授權等到建立時才檢查）；找不到就是不可用、附原因。
        if (!list[2].available) QVERIFY(list[2].unavailable.message.find("halconcpp.dll") != std::string::npos);
#else
        QVERIFY(!list[2].available);
        QVERIFY(list[2].unavailable.message.find("AOI_WITH_HALCON=ON") != std::string::npos);
#endif
    }

#ifdef AOI_WITH_OPENCV
    void composingTheOpenCvBackendWorks() {
        const BackendRegistry registry = app::makeBackendRegistry();
        Result<app::CoreServices> core = app::composeCore(registry, "opencv");
        QVERIFY2(core.ok(), core.ok() ? "" : core.error().message.c_str());
        QCOMPARE(core.value().backend.id, std::string("opencv"));
        QVERIFY(core.value().backend.templateBuilder->backendId() == "opencv");
        QVERIFY(core.value().backend.searcher->backendId() == "opencv");
    }
#endif

    void composingTheFakeBackendWorks() {
        const BackendRegistry registry = app::makeBackendRegistry();
        Result<app::CoreServices> core = app::composeCore(registry, "fake");
        QVERIFY2(core.ok(), core.ok() ? "" : core.error().message.c_str());
        QVERIFY(core.value().runner != nullptr);
        QVERIFY(core.value().generateObjects != nullptr);
        QVERIFY(core.value().executor->concurrency() >= 1);
        QVERIFY2(!core.value().probe->enabled(), "the probe is off unless asked for");
    }

    // 對應 --backend=opencv、--backend=halcon：沒有編進程式的 back end 要明確失敗，而且訊息要說有哪些可用。
    void composingAnUnavailableBackendFailsWithAClearMessage() {
        const BackendRegistry registry = app::makeBackendRegistry();
        std::vector<const char*> ids;
#ifndef AOI_WITH_OPENCV
        ids.push_back("opencv");
#endif
#ifndef AOI_WITH_HALCON
        ids.push_back("halcon");  // S08：編進程式時，可不可用看授權（tst_cli、tst_halcon_backend 驗證）
#endif
        if (ids.empty()) QSKIP("every back end is built into this program");
        for (const char* id : ids) {
            const Result<app::CoreServices> core = app::composeCore(registry, id);
            QVERIFY(!core.ok());
            QVERIFY(core.error().code == ErrorCode::BackendUnavailable);
            QVERIFY2(core.error().message.find("not built into this program") != std::string::npos, core.error().message.c_str());
            QVERIFY2(core.error().message.find("Available back ends: fake") != std::string::npos, core.error().message.c_str());
        }
    }

    void composingAnUnknownBackendFailsWithNotFound() {
        const BackendRegistry registry = app::makeBackendRegistry();
        const Result<app::CoreServices> core = app::composeCore(registry, "turbo");
        QVERIFY(!core.ok());
        QVERIFY(core.error().code == ErrorCode::NotFound);
        QVERIFY(core.error().message.find("turbo") != std::string::npos);
    }

    // 開放封閉的證明：註冊一個全新的 back end，use case 與 JobRunner 一個字都不用改，直接可用。
    void aBrandNewBackendRunsTheWholeUseCaseUnchanged() {
        BackendRegistry registry = app::makeBackendRegistry();
        QVERIFY(registry.registerBackend("scripted", "Scripted", [] { return Result<Backend>(makeScriptedBackend()); }).ok());

        Result<app::CoreServices> core = app::composeCore(registry, "scripted");
        QVERIFY(core.ok());
        QCOMPARE(core.value().backend.id, std::string("scripted"));

        const TestImage image(800, 600);
        const SampleRequest request = makeRequest(image, {380.0, 285.0, 40.0, 30.0});
        std::atomic<bool> finished{false};
        std::atomic<bool> usable{false};
        Result<SampleProposal> outcome = makeError(ErrorCode::Internal, "not run");
        const GenerateObjectsFromSample* useCase = core.value().generateObjects.get();
        core.value().runner->submit(
            [&](const CancelToken& token) { outcome = useCase->run(request, token); },
            [&](const JobReport& report) {
                usable = report.usable();  // QVERIFY 只能在測試執行緒用，所以先記下來
                finished = true;
            });
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!finished && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(1ms);
        QVERIFY(finished);
        QVERIFY(usable);
        QVERIFY2(outcome.ok(), outcome.ok() ? "" : outcome.error().message.c_str());
        QCOMPARE(outcome.value().candidates.size(), std::size_t{2});  // 樣本 ＋ 它唯一的鄰居
        QVERIFY(outcome.value().candidates[0].source == CandidateSource::Sample);
        QVERIFY(outcome.value().candidates[1].source == CandidateSource::Seed);
    }
};

QTEST_APPLESS_MAIN(TstBackendRegistry)
#include "tst_backend_registry.moc"
