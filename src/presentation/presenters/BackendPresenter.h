#pragma once

#include <QObject>
#include <QString>
#include <string>
#include <vector>

#include "application/backend/BackendRegistry.h"
#include "presentation/presenters/ControllerInterfaces.h"
#include "presentation/presenters/ViewInterfaces.h"

namespace aoi::ui {

// back end 這一塊畫面的起點：有哪些、目前用哪一個、它有什麼提醒。
struct BackendSetup {
    std::vector<BackendInfo> backends;  // 註冊順序（＝選單的順序），含不可用的
    std::string current;                // 目前用的 back end 的 id
    std::vector<BackendNotice> notices; // 目前的 back end 的提醒（例如 HALCON 用的是 runtime 授權）
};

// back end 的 Presenter：上方的 back end 選單、狀態列最左邊的 back end 標示、訊息列的提醒卡片。
//
// ux-spec 第 12.4 節把 IBackendController 放在 ParamsPresenter；這裡獨立成一個 Presenter（S：back end 的選擇與提醒
// 和比對參數是兩個變更理由）。提醒的規則（使用者 2026-10-07、2026-10-08 決定）：
//  - 有提醒時，訊息列顯示一張不會自己消失的卡片，按〔關閉〕才收起。搜尋的卡片（預覽、無匹配、錯誤）與進度列優先；
//    它們收起之後提醒卡片再出現，直到按〔關閉〕（「讓位」是 MessageBar 的顯示規則）。
//  - 狀態列的 back end 名稱後面一直標示（「HALCON · runtime 授權」），滑鼠移上去看完整說明。
//  - stderr 的那一行由組裝根寫（app/main.cpp），不在這裡。
// 只在 UI 執行緒使用。
class BackendPresenter : public QObject, public IBackendController {
    Q_OBJECT
public:
    BackendPresenter(IBackendView& view, BackendSetup setup, QObject* parent = nullptr);

    // ---- IBackendController ----
    std::vector<BackendInfo> availableBackends() const override { return setup_.backends; }
    void applyNoticeAction(int actionIndex) override;

    // ---- 讀（測試用）----
    const std::string& current() const { return setup_.current; }
    // 提醒卡片要不要顯示（有提醒、而且還沒按〔關閉〕）。被搜尋的卡片蓋過時仍是 true。
    bool noticePending() const { return noticePending_; }

public slots:
    // 語言換了：重新發布所有文字（關掉的提醒不會因此回來）。
    void refreshTexts();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void publish();
    QString currentName() const;

    IBackendView& view_;
    BackendSetup setup_;
    bool noticePending_ = false;
};

}  // namespace aoi::ui
