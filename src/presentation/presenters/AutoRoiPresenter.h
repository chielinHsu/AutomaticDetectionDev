#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <atomic>
#include <memory>
#include <vector>

#include "application/interaction/InspectionSession.h"
#include "application/jobs/JobRunner.h"
#include "application/ports/IPerfProbe.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "presentation/canvas/ImageViewMetaType.h"
#include "domain/search_params.h"
#include "presentation/canvas/ICanvasController.h"
#include "presentation/canvas/ICanvasView.h"
#include "presentation/presenters/ControllerInterfaces.h"
#include "presentation/presenters/MessageTexts.h"
#include "presentation/presenters/SceneBuilder.h"
#include "presentation/presenters/UiPoster.h"
#include "presentation/presenters/ViewInterfaces.h"
#include "presentation/theme/ThemeTokens.h"
#include "presentation/tools/ToolId.h"

namespace aoi::ui {

// Presenter 看到的所有 View（都是窄介面，不是 MainWindow）。
struct PresenterViews {
    ICanvasView& canvas;
    ICanvasInputGate& gate;
    IStatusView& status;
    IMessageView& message;
    IHistoryView& history;
    IComponentTreeView& tree;
    IParamsView& params;
};

// Presenter 用到的核心（都是 application 層的型別；具體的 back end 在組裝根接好，這裡不認識）。
struct PresenterServices {
    const GenerateObjectsFromSample& generate;
    JobRunner& runner;
    IPerfProbe* probe = nullptr;
    QString backendName;          // 狀態列顯示：「Fake (synthetic grid)」
    QString fallbackBackendName;  // back end 無法使用時，錯誤橫幅建議改用的那一個
};

// 回饋時間規則（ux-spec 第 9 節）與最小樣本。預設取自 tokens.json；測試可以縮短時間，免得等真實的 200 ms、1 s。
struct PresenterSettings {
    int busyAfterMs = 200;     // 搜尋超過這麼久：狀態列顯示忙碌
    int progressAfterMs = 1000;  // 超過這麼久：訊息列顯示進度條與〔取消〕
    int busyMinShowMs = 400;   // 忙碌標示一旦出現，至少停留這麼久（避免閃爍）
    int nudgeMergeMs = 600;    // 方向鍵連續微調，間隔不超過這個就併成一步
    double minSamplePx = 8.0;  // 樣本邊長小於這個值不搜尋
    static PresenterSettings fromTokens(const ThemeTokens& tokens);
};

// 互動的 Presenter：把畫布的事件交給 InspectionSession（規則），把搜尋交給 JobRunner（執行緒），
// 再把結果畫到各個 View。
//
//  - 規則（狀態轉移、復原的路由、接受時發生什麼事）在 application 層的 InspectionSession，沒有 Qt，單元測試逐項驗證；
//    這個類別只負責「Qt 這一側」：worker 執行緒 ↔ UI 執行緒、計時器（忙碌／進度）、把資料變成畫面。
//  - 搜尋在 worker 上跑。結果以不可變的 SampleProposal 經 UiPoster 排回 UI 執行緒；用「世代」丟棄過期的結果
//    （JobReport::latest 是 worker 上算的，轉回 UI 執行緒後仍要用自己的狀態再檢查一次）。
//  - UI 不等 worker：取消時 UI 立刻回到 Idle（I4），worker 的舊結果回來時被丟棄。
//  - 只在 UI 執行緒使用。
class AutoRoiPresenter : public QObject,
                         public ICanvasController,
                         public ISelectionController,
                         public IKeyboardController,
                         public IProposalController,
                         public IMessageController,
                         public IHistoryController,
                         public IParamsSink {
    Q_OBJECT
public:
    AutoRoiPresenter(PresenterViews views, PresenterServices services, PresenterSettings settings, QObject* parent = nullptr);
    ~AutoRoiPresenter() override;

    // ---- 讀（測試與組裝用）----
    const InspectionSession& session() const { return session_; }
    InteractionState state() const { return session_.state(); }
    std::uint64_t generation() const { return generation_; }
    BusyLevel busyLevel() const { return busyLevel_; }
    const PresenterSettings& settings() const { return settings_; }
    void setSettings(const PresenterSettings& settings) { settings_ = settings; }
    // 最近一次 publish 的場景（測試用）。
    const Scene& lastScene() const { return scene_; }

    // ---- ICanvasController ----
    void onRegionDrawn(const QRectF& rect, DraftMode mode) override;
    void onRegionAborted() override;
    void onAddRequested(const QPointF& imagePoint) override;
    void onComponentsMoved(const std::vector<int>& componentIds, const QPointF& delta) override;
    void onOpenTypeEditor(int componentId, const QPointF& imagePoint) override;

    // ---- ISelectionController ----
    void select(const std::vector<int>& sceneIds) override;
    void deleteSelected() override;

    // ---- IKeyboardController ----
    bool onKeyPressed(int key, Qt::KeyboardModifiers modifiers) override;

    // ---- IProposalController ----
    void accept() override;
    void discard() override;
    void cancelSearch() override;

    // ---- IMessageController ----
    void retry() override;
    void dismiss() override;
    void applyAdvice(int actionIndex) override;

    // ---- IHistoryController ----
    void undo() override;
    void redo() override;

    // ---- IParamsSink ----
    aoi::SearchParams searchParams() const override { return searchParams_; }
    void applyThreshold(double threshold) override;
    void applySearchParams(const SearchParams& params) override;
    void applyScanScope(int scopeIndex) override;

    // Esc：退回一層（ux-spec 第 8 節）。搜尋中 → 取消；有選取 → 清除選取；預覽 → 捨棄；無匹配、錯誤 → 關閉。
    // 回傳有沒有處理（畫框中的 Esc 歸工具）。
    bool escape();

public slots:
    // MainWindow 開好（或打不開）影像時呼叫。
    void onImageOpened(const aoi::ImageView& image, const QString& name);
    void onImageOpenFailed(const QString& name, const QString& reason);
    // 畫布上左鍵按下（開始拖出區域）或放開。
    void onPointerDragChanged(bool active, ToolId tool);
    // 語言換了：重新發布所有文字。
    void refreshTexts();

signals:
    void stateChanged(aoi::InteractionState state);
    // 訊息卡的動作要求換工具（〔重畫樣本〕→ 教樣本）。
    void toolRequested(aoi::ui::ToolId tool);
    // 一次搜尋的結果已經套用到畫面（世代編號）；測試與效能量測等它。
    void searchApplied(quint64 generation);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct JobOutcome;
    class ProgressBridge;
    friend class ProgressBridge;

    // 規則的結果 → 畫面
    void publishAll();
    void publishScene();
    void publishChrome();
    void publishSelection();
    void publishDraft();
    void publishMessage();
    void publishStatus();
    void publishHistory();
    void publishGate();
    void notifyStateIfChanged();

    // 搜尋
    void startSearch();
    void finishRegionScan();
    void onJobFinished(std::uint64_t generation, const JobReport& report, const std::shared_ptr<JobOutcome>& outcome);
    void onProgress(std::uint64_t generation, double fraction);
    void invalidateJobs();

    // 忙碌與進度
    void beginBusy();
    void endBusy(bool immediate);
    void showBusyNow();
    void showProgressNow();
    void hideBusyLabelNow();
    void rebuildLabels();
    ParamsState currentParams() const;

    // 小工具
    std::vector<int> toSessionIds(const std::vector<int>& sceneIds) const;
    std::vector<int> toSceneIds(const std::vector<int>& sessionIds) const;
    bool proposalShown() const { return session_.proposal() != nullptr; }
    QString hintText() const;
    QString summaryText() const;
        void runCardAction(CardAction action);
    bool nudge(double dx, double dy);

    PresenterViews views_;
    PresenterServices services_;
    PresenterSettings settings_;
    InspectionSession session_;
    aoi::ImageView image_;
    QString imageName_;
    SearchParams searchParams_;
    int scanScope_ = 0;

    // 非同步
    std::shared_ptr<UiPoster> poster_;
    std::uint64_t generation_ = 0;
    double progress_ = 0.0;
    double lastMs_ = -1.0;  // 最近一次搜尋的耗時；-1 ＝ 還沒有

    // 忙碌與進度
    QTimer busyTimer_;
    QTimer progressTimer_;
    QElapsedTimer busyShownClock_;
    BusyLevel busyLevel_ = BusyLevel::None;
    std::uint64_t busyEpoch_ = 0;

    // 畫面
    Scene scene_;
    CardSpec card_;
    bool cardVisible_ = false;
    InteractionState lastNotified_ = InteractionState::Empty;
    SceneLabels labels_;
    RegionMode drawMode_ = RegionMode::Sample;
    QElapsedTimer nudgeClock_;
};

}  // namespace aoi::ui
