#pragma once

#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSlider>
#include <QTreeView>
#include <QPushButton>
#include <QSignalSpy>
#include <QStatusBar>
#include <QTest>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "TestSupport.h"
#include "app/CompositionRoot.h"
#include "application/backend/BackendRegistry.h"
#include "infrastructure/backend_fake/FakeBackend.h"
#include "infrastructure/image_qt/QtImageSource.h"
#include "presentation/canvas/ImageCanvas.h"
#include "presentation/dev/FakeScene.h"
#include "presentation/i18n/Translations.h"
#include "presentation/objects/ObjectsPanel.h"
#include "presentation/shell/EmptyCard.h"
#include "presentation/params/ParamsPanel.h"
#include "presentation/presenters/AppPresenters.h"
#include "presentation/shell/MainWindow.h"
#include "presentation/shell/MessageBar.h"
#include "presentation/theme/ThemeManager.h"
#include "presentation/theme/ThemeTokens.h"

// Presenter 的測試台：和產品一模一樣的接線（core → MainWindow → AppPresenters），只是 back end 換成可以計數、
// 可以讓搜尋變慢或失敗的假 back end 包裝。測試走的就是使用者走的路徑：滑鼠事件進畫布，結果從畫面讀出來。
namespace aoi::test {

// 包在假 back end 外面：記下被呼叫的次數，必要時讓搜尋失敗。
struct BackendProbe {
    std::atomic<int> builds{0};
    std::atomic<int> finds{0};
    std::atomic<bool> fail{false};
    // 搜尋時 back end 無法使用（例如執行中 HALCON 的授權不見了，HalconErrors 回 BackendUnavailable）。
    std::atomic<bool> unavailable{false};
    // 視窗驗證（use case 的最後一個階段）的閘門：開著的時候，每一次視窗搜尋依到達順序領一張號碼牌，
    // 要等 windowReleased 大於等於號碼才返回。測試用它讓兩個搜尋「同時卡在最後一步」。
    std::atomic<bool> gateWindows{false};
    std::atomic<int> windowEntered{0};
    std::atomic<int> windowReleased{0};
    // 建立 back end 的閘門（模擬 HALCON 約 1.5 秒的授權檢查）：開著的時候，工廠等 createReleased 才返回。
    std::atomic<bool> gateCreate{false};
    std::atomic<bool> createEntered{false};
    std::atomic<bool> createReleased{false};
    std::atomic<int> creates{0};
    std::atomic<bool> createdOnUiThread{false};
    // 建立 back end 失敗（像授權檢查失敗）；清掉之後再建立就會成功（像插上 dongle）。
    std::atomic<bool> failCreate{false};
};

class ProbedBuilder final : public aoi::ITemplateBuilder {
public:
    ProbedBuilder(std::shared_ptr<BackendProbe> probe, aoi::fake::FakeSpec spec) : probe_(std::move(probe)), inner_(std::move(spec)) {}
    std::string_view backendId() const noexcept override { return inner_.backendId(); }
    aoi::Result<aoi::TemplatePtr> build(const aoi::ImageView& image, const aoi::Rect& sample, const aoi::SearchParams& params,
                                        const aoi::CancelToken& cancel) override {
        ++probe_->builds;
        return inner_.build(image, sample, params, cancel);
    }

private:
    std::shared_ptr<BackendProbe> probe_;
    aoi::fake::FakeTemplateBuilder inner_;
};

class ProbedSearcher final : public aoi::IPatternSearcher {
public:
    ProbedSearcher(std::shared_ptr<BackendProbe> probe, aoi::fake::FakeSpec spec) : probe_(std::move(probe)), inner_(std::move(spec)) {}
    std::string_view backendId() const noexcept override { return inner_.backendId(); }
    aoi::Result<std::vector<aoi::Match>> find(const aoi::ImageView& image, const aoi::ITemplate& tmpl, const aoi::SearchRegion& region,
                                              const aoi::SearchParams& params, const aoi::CancelToken& cancel) override {
        ++probe_->finds;
        const bool gated = region.kind == aoi::SearchRegion::Kind::Windows && probe_->gateWindows;
        if (gated) {
            const int ticket = ++probe_->windowEntered;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (probe_->windowReleased < ticket && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (probe_->fail) return aoi::makeError(aoi::ErrorCode::BackendFailed, "the test back end was told to fail");
        if (probe_->unavailable) return aoi::makeError(aoi::ErrorCode::BackendUnavailable, "the test license is gone");
        // 閘門後面模擬「不可中斷的函式庫呼叫」：它不看取消旗標，照樣成功返回。取消的結果怎麼處理，是 Presenter 的事。
        return inner_.find(image, tmpl, region, params, gated ? aoi::CancelToken{} : cancel);
    }

private:
    std::shared_ptr<BackendProbe> probe_;
    aoi::fake::FakeSearcher inner_;
};

// 預設的假世界：樣本左右各 4 格、上下各 2 格，其中約 20% 分數低（會被 0.70 的門檻拒絕）、約 10% 缺件（推算）。
inline aoi::fake::FakeSpec defaultWorld() {
    aoi::fake::FakeSpec spec;
    spec.halfCols = 4;
    spec.halfRows = 2;
    spec.weakFraction = 0.2;
    spec.missingFraction = 0.1;
    spec.seed = 3;
    return spec;
}

// 執行中切換的對象（ADR-021）：Rig 的 registry 除了 fake 還有這三個。
//   broken 「Broken (test)」：登記成可用，但工廠失敗（像授權檢查失敗）
//   other  「Other (test)」：同樣的假世界，自己的探針（otherProbe），建立可以卡在閘門上或失敗；有一個提醒（像 HALCON 的 runtime 授權）
//   off    「Off (test)」：登記成不可用（像沒編進程式）
// 註冊順序和產品一樣由簡單到完整：fake 的錯誤卡片建議改用的（可用、不是目前的最後一個）是 other。
inline aoi::BackendNotice otherNotice() {
    return {"ui.backend.halcon.sdkDongleMissing", "ui.backend.halcon.runtimeLicense",
            {{"inUse", "3-7614034"}, {"missing", "3-7081033"}}, "Other runs with the runtime license."};
}

struct Rig {
    std::shared_ptr<BackendProbe> probe = std::make_shared<BackendProbe>();
    std::shared_ptr<BackendProbe> otherProbe = std::make_shared<BackendProbe>();
    aoi::ui::ThemeManager theme{*qApp, aoi::ui::ThemeTokens::builtin()};
    aoi::BackendRegistry registry;
    aoi::Result<aoi::app::CoreServices> core;
    aoi::infra::QtImageSource imageSource;  // 產品用 makeImageSource()；這裡固定 Qt 的，測試不受 AOI_WITH_OPENCV 影響
    aoi::ui::MainWindow window;
    std::unique_ptr<aoi::ui::AppPresenters> presenters;
    aoi::dev::FakeBoard board;

    // notices：假 back end 的提醒（例如 HALCON 的授權；BackendPresenter 的卡片與狀態列標示）。
    explicit Rig(aoi::fake::FakeSpec world = defaultWorld(), bool fastTimings = true,
                 aoi::app::ExecutorKind executor = aoi::app::ExecutorKind::QtThreadPool,
                 std::vector<aoi::BackendNotice> notices = {})
        : core(makeCore(registry, probe, otherProbe, std::move(world), executor, std::move(notices))), window(theme) {
        aoi::ui::Translations::install(QStringLiteral("zh_TW"));
        window.setImageSource(&imageSource);
        aoi::app::CoreServices& services = core.value();
        aoi::ui::PresenterServices presenterServices{aoi::useCaseOf(services.engine), *services.runner, services.probe.get()};
        // 和產品一樣：切換在 core 的執行緒池上建立（main.cpp）。
        aoi::ui::BackendServices backendServices;
        backendServices.backends = registry.list();
        backendServices.current = services.engine;
        backendServices.create = [this](const std::string& id) { return aoi::makeSearchEngine(registry, id); };
        backendServices.executor = services.executor.get();
        presenters = std::make_unique<aoi::ui::AppPresenters>(window, presenterServices, std::move(backendServices),
                                                              aoi::ui::ThemeTokens::builtin());
        if (fastTimings) {
            aoi::ui::PresenterSettings s = presenter().settings();
            s.busyAfterMs = 40;
            s.progressAfterMs = 150;
            s.busyMinShowMs = 120;
            presenter().setSettings(s);
        }
        // 操作步驟 dock（ADR-027）預設關著：它的版面與重畫在 Debug 每次狀態變化約 100 ms，每個測試台都開一個，
        // tst_presenter 整組從 231 秒變成 297 秒、貼著 300 秒的逾時。產品是開著的；要測它的測試自己 showGuidance()。
        window.guidanceDock()->hide();
        window.show();
        Q_UNUSED(QTest::qWaitForWindowExposed(&window));
    }

    void showGuidance() {
        window.guidanceDock()->show();
        QCoreApplication::processEvents();
    }

    ~Rig() {
        // 先停掉進行中的工作，再解構（JobRunner 解構會等工作結束）。建立中的 back end 放行，執行緒池解構時不必等到逾時。
        presenter().cancelSearch();
        otherProbe->createReleased = true;
        probe->windowReleased = 1 << 20;
        otherProbe->windowReleased = 1 << 20;
    }

    static aoi::Result<aoi::app::CoreServices> makeCore(aoi::BackendRegistry& registry, const std::shared_ptr<BackendProbe>& probe,
                                                         const std::shared_ptr<BackendProbe>& otherProbe, aoi::fake::FakeSpec world,
                                                         aoi::app::ExecutorKind executor, std::vector<aoi::BackendNotice> notices) {
        (void)registry.registerBackend("fake", "Fake (test)", [probe, world, notices]() -> aoi::Result<aoi::Backend> {
            aoi::Backend backend;
            backend.id = "fake";
            backend.displayName = "Fake (test)";
            backend.templateBuilder = std::make_unique<ProbedBuilder>(probe, world);
            backend.searcher = std::make_unique<ProbedSearcher>(probe, world);
            backend.notices = notices;
            return aoi::Result<aoi::Backend>(std::move(backend));
        });
        (void)registry.registerBackend("broken", "Broken (test)", []() -> aoi::Result<aoi::Backend> {
            return aoi::makeError(aoi::ErrorCode::BackendUnavailable, "no license (test)");
        });
        const std::thread::id uiThread = std::this_thread::get_id();
        (void)registry.registerBackend("other", "Other (test)", [otherProbe, world, uiThread]() -> aoi::Result<aoi::Backend> {
            ++otherProbe->creates;
            otherProbe->createdOnUiThread = std::this_thread::get_id() == uiThread;
            otherProbe->createEntered = true;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (otherProbe->gateCreate && !otherProbe->createReleased && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (otherProbe->failCreate) return aoi::makeError(aoi::ErrorCode::BackendUnavailable, "no license (test)");
            aoi::Backend backend;
            backend.id = "other";
            backend.displayName = "Other (test)";
            backend.templateBuilder = std::make_unique<ProbedBuilder>(otherProbe, world);
            backend.searcher = std::make_unique<ProbedSearcher>(otherProbe, world);
            backend.notices = {otherNotice()};
            return aoi::Result<aoi::Backend>(std::move(backend));
        });
        (void)registry.registerUnavailable("off", "Off (test)", "not built in (test)");
        return aoi::app::composeCore(registry, "fake", false, executor);
    }

    // 選單上 id 那一項的索引（-1 ＝ 沒有）。
    int comboIndex(const QString& id) { return window.backendCombo()->findData(id); }
    // 像使用者一樣在選單上選一項（activated 只在使用者操作時發出）。
    void chooseBackend(const QString& id) { emit window.backendCombo()->activated(comboIndex(id)); }

    aoi::ui::AutoRoiPresenter& presenter() { return presenters->autoRoi(); }
    aoi::ui::ImageCanvas* canvas() { return window.canvas(); }
    QWidget* vp() { return window.canvas()->viewport(); }
    aoi::InteractionState state() { return presenter().state(); }
    QPointF at(double x, double y) { return aoi::test::toView(*window.canvas(), QPointF(x, y)); }

    void open(int w = 1200, int h = 800) {
        board = aoi::dev::makeFakeBoard(w, h);
        window.setImage(board.view(), QStringLiteral("board.png"));
        QCoreApplication::processEvents();
    }

    // 選一個工具，在畫布上拖出一個框（場景座標），放開。
    void drag(aoi::ui::ToolId tool, const QRectF& scene) {
        window.toolAction(tool)->trigger();
        aoi::test::press(vp(), at(scene.left(), scene.top()));
        aoi::test::move(vp(), at(scene.center().x(), scene.center().y()));
        aoi::test::move(vp(), at(scene.right(), scene.bottom()));
        aoi::test::release(vp(), at(scene.right(), scene.bottom()));
    }

    // 建立一個樣本（S 工具放開；2026-10-14 起不搜尋，ADR-025）：回到 Idle，板上多一個樣本元件。
    bool drawSample(const QRectF& scene = QRectF(560, 360, 60, 32)) {
        drag(aoi::ui::ToolId::Sample, scene);
        return waitState(aoi::InteractionState::Idle, 2000) && presenter().session().hasSamples();
    }
    // 按〔搜尋〕（F5），等結果出來。
    bool searchAndWait() {
        window.searchAction()->trigger();
        return waitState(aoi::InteractionState::Preview) || state() == aoi::InteractionState::NoMatch;
    }
    // 畫樣本並按 F5，不等結果（舊的 drag(Sample) 的替代：之後的斷言從 Searching 開始）。
    void searchByDrag(const QRectF& scene = QRectF(560, 360, 60, 32)) {
        drawSample(scene);
        window.searchAction()->trigger();
    }
    // 教一個樣本並搜尋：畫樣本、按 F5、等結果（舊的「放開就搜尋」的替代）。
    bool teachSample(const QRectF& scene = QRectF(560, 360, 60, 32)) {
        if (!drawSample(scene)) return false;
        return searchAndWait();
    }

    bool waitState(aoi::InteractionState s, int timeoutMs = 8000) {
        return QTest::qWaitFor([&] { return state() == s; }, timeoutMs);
    }

    QList<QPushButton*> cardButtons() {
        QWidget* card = window.messageBar()->findChild<QWidget*>(QStringLiteral("messageCard"));
        return card ? card->findChildren<QPushButton*>() : QList<QPushButton*>();
    }

    QString summary() { return window.summaryLabel()->text(); }
    QString backendStatus() { return window.backendStatusLabel()->text(); }
    QString hint() { return window.messageBar()->hintText(); }
    const aoi::ui::Scene& scene() { return presenter().lastScene(); }
    int boxCount() { return canvas()->layerItem()->boxCount(); }
    int componentCount() { return canvas()->layerItem()->componentCount(); }
    int treeRows() { return window.objectsPanel()->model()->visibleCount(); }
    const aoi::InspectionDocument& document() { return presenter().session().document(); }

    // 場景裡第 index 個元件的外框中心（場景座標）。
    QPointF centerOfComponent(int index) {
        return scene().snapshot->components[static_cast<std::size_t>(index)].outline.center();
    }
    void clickComponent(int index, Qt::KeyboardModifiers mods = {}) {
        window.toolAction(aoi::ui::ToolId::Select)->trigger();
        const QPointF c = centerOfComponent(index);
        aoi::test::press(vp(), at(c.x(), c.y()), Qt::LeftButton, mods);
        aoi::test::release(vp(), at(c.x(), c.y()));
    }
    void key(int k, Qt::KeyboardModifiers mods = {}) { aoi::test::sendKey(canvas(), k, QEvent::KeyPress, mods); }
};

}  // namespace aoi::test
