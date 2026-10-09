#pragma once

#include <memory>

#include "presentation/compare/ComparePresenter.h"
#include "presentation/presenters/AutoRoiPresenter.h"
#include "presentation/presenters/BackendPresenter.h"
#include "presentation/presenters/ParamsPresenter.h"
#include "presentation/shell/MainWindow.h"
#include "presentation/theme/ThemeTokens.h"

namespace aoi::ui {

// 組裝：把 Presenter 接到 MainWindow（它同時是幾個 View，也持有畫布、物件樹、參數 dock）。
// 組裝根（app/main.cpp）與測試都用這一個，所以測試走的就是產品的接線。
//
// 生命期：必須比 services 裡引用的核心（use case、JobRunner）先解構，比 MainWindow 後建立（成員順序已保證
// main.cpp 的區域變數：核心 → 視窗 → 這個物件，解構時反過來）。解構時先把視窗的 Controller 清掉，視窗就不會再呼叫已死的 Presenter。
class AppPresenters {
public:
    AppPresenters(MainWindow& window, PresenterServices services, BackendServices backend, const ThemeTokens& tokens);
    ~AppPresenters();
    AppPresenters(const AppPresenters&) = delete;
    AppPresenters& operator=(const AppPresenters&) = delete;

    AutoRoiPresenter& autoRoi() { return *autoRoi_; }
    ParamsPresenter& params() { return *params_; }
    BackendPresenter& backend() { return *backend_; }
    ComparePresenter& compare() { return *compare_; }

private:
    MainWindow& window_;
    std::unique_ptr<AutoRoiPresenter> autoRoi_;
    std::unique_ptr<ParamsPresenter> params_;
    std::unique_ptr<BackendPresenter> backend_;
    std::unique_ptr<ComparePresenter> compare_;  // A/B 比較（S09）：和 BackendPresenter 同一份 back end 的來源
};

}  // namespace aoi::ui
