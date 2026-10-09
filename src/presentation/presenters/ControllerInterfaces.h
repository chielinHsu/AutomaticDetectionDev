#pragma once

#include <QString>
#include <memory>
#include <string>
#include <vector>

#include "application/backend/BackendRegistry.h"
#include "application/interaction/InteractionStateMachine.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "domain/search_params.h"

// View 呼叫的 Controller（docs/ux/ux-spec.md 第 12.3 節）；由 Presenter 實作。每個介面 ≤ 5 個方法。
// 畫布的 ICanvasController、ISelectionController 在 presentation/canvas/ICanvasController.h。
namespace aoi::ui {

// 提案的生命週期。接受並存入樣本庫（acceptAndSave）與改選樣本（chooseAlternative）要等樣本庫（S06、S07）。
class IProposalController {
public:
    virtual ~IProposalController() = default;
    virtual void accept() = 0;
    virtual void discard() = 0;
    virtual void cancelSearch() = 0;
};

// 訊息卡上的動作。applyAdvice 的索引是 View 顯示的按鈕順序（Presenter 自己記得每個索引是什麼）。
class IMessageController {
public:
    virtual ~IMessageController() = default;
    virtual void retry() = 0;
    virtual void dismiss() = 0;
    virtual void applyAdvice(int actionIndex) = 0;
};

class IHistoryController {
public:
    virtual ~IHistoryController() = default;
    virtual void undo() = 0;
    virtual void redo() = 0;
};

// 比對參數（參數 dock）。分數門檻只是對快取候選重新過濾（U3）；角度範圍要重新搜尋（防抖＋最新的優先）。
class IParamsController {
public:
    virtual ~IParamsController() = default;
    virtual void setScoreThreshold(double value) = 0;
    virtual void setAngleRange(double minDeg, double maxDeg) = 0;
    virtual void setScanScope(int scopeIndex) = 0;
    // 影像的有效位元數（S07）：0 ＝ 自動，8..16 ＝ 指定。立刻生效（不搜尋）。
    virtual void setSignificantBits(int bits) = 0;
};

// back end（ux-spec 第 12.3 節的 IBackendController）。availableBackends 是選單的內容（註冊順序，含不可用的）；
// selectBackend 是選單選了一項（執行中切換，ADR-021）；applyNoticeAction 是訊息列提醒卡片上的按鈕（索引是 View 顯示的順序）。
class IBackendController {
public:
    virtual ~IBackendController() = default;
    virtual std::vector<aoi::BackendInfo> availableBackends() const = 0;
    virtual void selectBackend(const std::string& id) = 0;
    virtual void applyNoticeAction(int actionIndex) = 0;
};

// 目前的 back end 與「搜尋時 back end 無法使用」的錯誤卡片建議改用的那一個（顯示用的名稱；fallbackId 空 ＝ 沒有別的可用）。
struct BackendLabels {
    QString name;          // 「HALCON」
    QString fallbackId;    // 「opencv」：卡片上〔改用 OpenCV〕換成它
    QString fallbackName;  // 「OpenCV」
};

// BackendPresenter 對 AutoRoiPresenter 的窄介面（像 IParamsSink）：換 back end 時，搜尋那一側要做的事。
// 什麼時候做什麼由 application 的 BackendSwitch 決定（pausesSearch、researchAfter）；這裡只是執行。
class ISearchEngineSink {
public:
    virtual ~ISearchEngineSink() = default;
    virtual aoi::InteractionState interactionState() const = 0;
    // 作廢進行中的搜尋（latest-wins：不等它停下，它的結果回來時被丟掉）。狀態留在 Searching，忙碌標示照舊。
    virtual void pauseSearch() = 0;
    // 之後的搜尋用這個 use case（它和它的 back end 共用擁有權）；labels 是錯誤卡片用的名稱（目前的、建議改用的）。
    virtual void useSearchEngine(std::shared_ptr<const aoi::GenerateObjectsFromSample> useCase, const BackendLabels& labels) = 0;
    // 用目前的 back end 重做上次的搜尋：Searching、Preview、NoMatch 是同一個區域重新搜尋；搜尋失敗的錯誤（Error）是〔再試一次〕；
    // 其餘狀態不動。
    virtual void researchSameRegion() = 0;
};

// ParamsPresenter 對 AutoRoiPresenter 的窄介面：它只需要「改門檻」與「改搜尋參數」。
class IParamsSink {
public:
    virtual ~IParamsSink() = default;
    virtual aoi::SearchParams searchParams() const = 0;
    virtual void applyThreshold(double threshold) = 0;
    // 需要重新搜尋的參數變了（呼叫端已經防抖）。
    virtual void applySearchParams(const aoi::SearchParams& params) = 0;
    // 比對範圍：目前只影響流程 B（樣本庫），所以只記下來。
    virtual void applyScanScope(int scopeIndex) = 0;
    // 影像的有效位元數（S07）：重新換算顯示與之後的搜尋；已顯示的提案不重新搜尋。
    virtual void applySignificantBits(int bits) = 0;
};

}  // namespace aoi::ui
