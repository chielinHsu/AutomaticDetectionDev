#include "presentation/presenters/AppPresenters.h"

#include "presentation/canvas/ImageCanvas.h"
#include "presentation/objects/ObjectsPanel.h"
#include "presentation/params/ParamsPanel.h"

namespace aoi::ui {

AppPresenters::AppPresenters(MainWindow& window, PresenterServices services, BackendServices backend, const ThemeTokens& tokens)
    : window_(window) {
    ImageCanvas* canvas = window.canvas();
    PresenterViews views{*canvas, *canvas, window, window, window, *window.objectsPanel(), *window.paramsPanel()};
    autoRoi_ = std::make_unique<AutoRoiPresenter>(views, std::move(services), PresenterSettings::fromTokens(tokens));
    params_ = std::make_unique<ParamsPresenter>(*autoRoi_, tokens.timing.paramDebounce);
    backend_ = std::make_unique<BackendPresenter>(window, *autoRoi_, std::move(backend));

    MainWindow::Controllers controllers;
    controllers.canvas = autoRoi_.get();
    controllers.selection = autoRoi_.get();
    controllers.keyboard = autoRoi_.get();
    controllers.proposal = autoRoi_.get();
    controllers.message = autoRoi_.get();
    controllers.history = autoRoi_.get();
    controllers.params = params_.get();
    controllers.backend = backend_.get();
    window.setControllers(controllers);

    AutoRoiPresenter* presenter = autoRoi_.get();
    QObject::connect(&window, &MainWindow::imageOpened, presenter, &AutoRoiPresenter::onImageOpened);
    QObject::connect(&window, &MainWindow::imageOpenFailed, presenter, &AutoRoiPresenter::onImageOpenFailed);
    QObject::connect(canvas, &ImageCanvas::pointerDragChanged, presenter, &AutoRoiPresenter::onPointerDragChanged);
    QObject::connect(presenter, &AutoRoiPresenter::toolRequested, &window, &MainWindow::selectTool);
}

AppPresenters::~AppPresenters() {
    window_.setControllers(MainWindow::Controllers{});  // 視窗不再呼叫已經（即將）解構的 Presenter
}

}  // namespace aoi::ui
