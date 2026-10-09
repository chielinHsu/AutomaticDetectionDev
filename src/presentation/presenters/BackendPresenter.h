#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "application/Error.h"
#include "application/Result.h"
#include "application/backend/BackendRegistry.h"
#include "application/backend/BackendSwitch.h"
#include "application/backend/SearchEngine.h"
#include "application/ports/IExecutor.h"
#include "presentation/presenters/ControllerInterfaces.h"
#include "presentation/presenters/UiPoster.h"
#include "presentation/presenters/ViewInterfaces.h"

namespace aoi::ui {

// 依 id 建立 back end ＋ use case。在背景執行緒呼叫（HALCON 第一次約 1.5 秒）；組裝根給的是 makeSearchEngine。
using SearchEngineFactory = std::function<Result<SearchEnginePtr>(const std::string& id)>;

// back end 這一塊需要的核心。
struct BackendServices {
    std::vector<BackendInfo> backends;  // 註冊順序（＝選單的順序），含不可用的
    SearchEnginePtr current;            // 啟動時建好的那一個（它的提醒在 current->backend.notices）
    SearchEngineFactory create;         // 執行中切換時在背景呼叫
    IExecutor* executor = nullptr;      // 背景執行緒（和 JobRunner 共用）；空 ＝ 在 UI 執行緒建立（測試）
};

// back end 的 Presenter：上方的 back end 選單、狀態列最左邊的 back end 標示、訊息列的提醒卡片，以及執行中切換（ADR-021）。
//
// ux-spec 第 12.4 節把 IBackendController 放在 ParamsPresenter；這裡獨立成一個 Presenter（S：back end 的選擇與提醒
// 和比對參數是兩個變更理由）。
//
// 切換（規則在 application 的 BackendSwitch，這裡只做 Qt 這一側）：
//   選單選了 → BackendSwitch::request → 搜尋中就先作廢（ISearchEngineSink::pauseSearch）→ 選單停用、狀態列「切換到 HALCON…」
//   → 在背景執行緒建立（UI 不會卡住）→ 結果經 UiPoster 回到 UI 執行緒 → BackendSwitch::finish
//   → 成功：之後的搜尋用新的（useSearchEngine），有提醒就顯示提醒卡片，發出 backendChanged（組裝根記住它）；
//     失敗：留在原本的 back end，訊息列的卡片說原因（ui.error.backend.*），發出 switchFailed
//   → 依 BackendSwitch::researchAfter 對同一個區域重新搜尋（使用者 2026-10-08 決定）。
//
// 提醒卡片（使用者 2026-10-07、2026-10-08 決定）：不會自己消失，按〔關閉〕才收起；搜尋的卡片與進度列優先，它們收起之後
// 再出現（「讓位」是 MessageBar 的顯示規則）。狀態列的 back end 名稱後面一直標示（「HALCON · runtime 授權」），
// 滑鼠移上去看完整說明。stderr 的那一行由組裝根寫（app/main.cpp）。
// 只在 UI 執行緒使用。
class BackendPresenter : public QObject, public IBackendController {
    Q_OBJECT
public:
    BackendPresenter(IBackendView& view, ISearchEngineSink& sink, BackendServices services, QObject* parent = nullptr);
    ~BackendPresenter() override;

    // ---- IBackendController ----
    std::vector<BackendInfo> availableBackends() const override { return switch_.backends(); }
    void selectBackend(const std::string& id) override;
    void applyNoticeAction(int actionIndex) override;

    // ---- 讀（測試與組裝用）----
    const std::string& current() const { return switch_.current(); }
    bool switching() const { return switch_.switching(); }
    // 提醒卡片要不要顯示（有提醒或切換失敗、而且還沒按〔關閉〕）。被搜尋的卡片蓋過時仍是 true。
    bool noticePending() const { return noticePending_; }
    // 目前的 back end 與錯誤卡片建議改用的那一個（可用、不是目前的第一個；BackendSwitch::fallback）。
    BackendLabels labels() const;

signals:
    // 換成功（id 是新的 back end）；noticeMessages 是它的提醒的英文說明（stderr）。
    void backendChanged(const QString& id, const QStringList& noticeMessages);
    // 換失敗，留在原本的 back end；reason 是英文說明。
    void switchFailed(const QString& id, const QString& reason);
    // 一次切換結束（成功或失敗，含不必嘗試就失敗的）；測試等它。
    void switchFinished(bool succeeded);

public slots:
    // 語言換了：重新發布所有文字（關掉的提醒不會因此回來）。
    void refreshTexts();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void onCreated(const std::string& id, Result<SearchEnginePtr> result);
    // retryable：再試一次可能會成功（工廠失敗，例如授權檢查失敗；插上 dongle 之後再試就會成功）。
    // 登記成不可用、或沒有這個 id 的，再試也一樣，卡片上就沒有〔再試一次〕。
    void fail(const std::string& id, const Error& error, bool retryable);
    void publish();
    QString displayName(const std::string& id) const;

    IBackendView& view_;
    ISearchEngineSink& sink_;
    BackendServices services_;
    BackendSwitch switch_;
    SearchEnginePtr engine_;  // 目前的 back end（提醒在 engine_->backend.notices）
    struct Failure {
        std::string id;
        Error error;
        bool retryable = false;
    };
    // 提醒卡片上的按鈕（publish 依內容排，applyNoticeAction 依索引找回來）。
    enum class NoticeAction { Retry, Close };
    std::vector<NoticeAction> noticeActions_;
    std::optional<Failure> failure_;  // 最近一次切換失敗（卡片顯示它，直到按〔關閉〕或下一次切換）
    bool noticePending_ = false;
    std::shared_ptr<UiPoster> poster_;
};

}  // namespace aoi::ui
