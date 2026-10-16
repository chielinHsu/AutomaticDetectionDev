#include "presentation/presenters/AppPresenters.h"

#include "presentation/canvas/ImageCanvas.h"
#include "presentation/compare/CompareWindow.h"
#include "presentation/guidance/GuidancePanel.h"
#include "presentation/library/SamplePanel.h"
#include "presentation/objects/ObjectsPanel.h"
#include "presentation/params/ParamsPanel.h"

namespace aoi::ui {

AppPresenters::AppPresenters(MainWindow& window, PresenterServices services, BackendServices backend, const ThemeTokens& tokens)
    : window_(window) {
    ImageCanvas* canvas = window.canvas();
    // 物件面板同時是物件樹與選取資訊表單（2026-10-14）；樣本庫 dock 的面板列出板子上的樣本。
    PresenterViews views{*canvas,  *canvas, window, window, window, *window.objectsPanel(), *window.paramsPanel(),
                         *window.objectsPanel(), *window.samplePanel(), *window.guidancePanel()};
    autoRoi_ = std::make_unique<AutoRoiPresenter>(views, std::move(services), PresenterSettings::fromTokens(tokens));
    params_ = std::make_unique<ParamsPresenter>(*autoRoi_, tokens.timing.paramDebounce);
    // A/B 比較（S09）：同一份 back end 的清單與工廠、同一個執行緒池；啟動時建好的那一個直接用。A 預設是主視窗目前的 back end。
    CompareServices compare;
    compare.backends = backend.backends;
    compare.create = backend.create;
    compare.executor = backend.executor;
    if (backend.current) compare.ready.push_back(backend.current);
    backend_ = std::make_unique<BackendPresenter>(window, *autoRoi_, std::move(backend));
    BackendPresenter* backendPresenter = backend_.get();
    compare.currentBackend = [backendPresenter] { return backendPresenter->current(); };
    compare_ = std::make_unique<ComparePresenter>(window, *window.compareWindow(), *autoRoi_, std::move(compare));

    MainWindow::Controllers controllers;
    controllers.canvas = autoRoi_.get();
    controllers.selection = autoRoi_.get();
    controllers.selectionEdit = autoRoi_.get();
    controllers.matchEdit = autoRoi_.get();
    controllers.keyboard = autoRoi_.get();
    controllers.proposal = autoRoi_.get();
    controllers.message = autoRoi_.get();
    controllers.history = autoRoi_.get();
    controllers.params = params_.get();
    controllers.backend = backend_.get();
    controllers.compare = compare_.get();
    window.setControllers(controllers);

    AutoRoiPresenter* presenter = autoRoi_.get();
    QObject::connect(&window, &MainWindow::imageOpened, presenter, &AutoRoiPresenter::onImageOpened);
    QObject::connect(&window, &MainWindow::imageOpenFailed, presenter, &AutoRoiPresenter::onImageOpenFailed);
    QObject::connect(canvas, &ImageCanvas::pointerDragChanged, presenter, &AutoRoiPresenter::onPointerDragChanged);
    QObject::connect(presenter, &AutoRoiPresenter::toolRequested, &window, &MainWindow::selectTool);
    // 有沒有樣本可以比較，跟著主視窗的狀態走（〔A/B 比較…〕能不能按）。
    QObject::connect(presenter, &AutoRoiPresenter::stateChanged, compare_.get(), &ComparePresenter::refreshEntry);
    // 錯誤卡片：名稱以 BackendPresenter 為準；〔改用 OpenCV〕交給它換（ADR-021）。
    presenter->setBackendLabels(backendPresenter->labels());
    QObject::connect(presenter, &AutoRoiPresenter::backendSwitchRequested, backendPresenter,
                     [backendPresenter](const QString& id) { backendPresenter->selectBackend(id.toStdString()); });
}

AppPresenters::~AppPresenters() {
    window_.setControllers(MainWindow::Controllers{});  // 視窗不再呼叫已經（即將）解構的 Presenter
}

}  // namespace aoi::ui
