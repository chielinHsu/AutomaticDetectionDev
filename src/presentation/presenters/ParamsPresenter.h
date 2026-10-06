#pragma once

#include <QObject>
#include <QTimer>

#include "domain/search_params.h"
#include "presentation/presenters/ControllerInterfaces.h"

namespace aoi::ui {

// 參數 dock 的 Presenter（ux-spec 第 12.4 節）：
//  - 分數門檻 → 立刻交給 IParamsSink（對快取的候選重新過濾，不搜尋；U3 ≤ 16 ms）。
//  - 角度範圍等需要重新搜尋的參數 → 防抖（預設 120 ms，tokens.json 的 timing.paramDebounce）後才交出去，
//    拖曳中不會排滿工作佇列；交出去之後由 JobRunner 的 latest-wins 取代舊的搜尋（T5）。
//  - 比對範圍 → 立刻交出去（只影響流程 B）。
//  - 影像的有效位元數（S07）→ 立刻交出去（不搜尋，只重新換算顯示）。
// 只在 UI 執行緒使用。
class ParamsPresenter : public QObject, public IParamsController {
    Q_OBJECT
public:
    ParamsPresenter(IParamsSink& sink, int debounceMs, QObject* parent = nullptr);

    void setScoreThreshold(double value) override;
    void setAngleRange(double minDeg, double maxDeg) override;
    void setScanScope(int scopeIndex) override;
    void setSignificantBits(int bits) override;

    int debounceMs() const { return timer_.interval(); }
    bool searchPending() const { return timer_.isActive(); }
    // 不等計時器，現在就把待處理的搜尋參數交出去（關視窗前、測試）。
    void flush();

private:
    void apply();

    IParamsSink& sink_;
    QTimer timer_;
    SearchParams pending_;
};

}  // namespace aoi::ui
