#pragma once

#include <QString>
#include <QStringList>
#include <vector>

#include "presentation/objects/ObjectRows.h"

// Presenter 看到的 View（docs/ux/ux-spec.md 第 12.2 節）。每個介面 ≤ 5 個方法；Presenter 不認識 QWidget。
// 文字都是 Presenter 已經翻好的字串（View 不查文案），所以語言切換時由 Presenter 重新發布。
namespace aoi::ui {

enum class BusyLevel {
    None,
    Busy,      // 搜尋超過 200 ms：狀態列「搜尋中…」，游標變忙碌
    Progress,  // 超過 1 s：訊息列出現進度條與〔取消〕
};

// 狀態列與訊息列的提示（常駐提示、吐司、忙碌、進度）。
class IStatusView {
public:
    virtual ~IStatusView() = default;
    virtual void showSummary(const QString& text) = 0;  // 「back end · 耗時 ms · 數量」
    virtual void showBusy(BusyLevel level) = 0;
    virtual void showProgress(double fraction) = 0;     // 0..1，Progress 時才看得到
    virtual void showToast(const QString& text) = 0;
    virtual void setHint(const QString& text) = 0;      // 空字串 ＝ 沒有提示
};

// 訊息列的卡片：預覽的動作列、無匹配的做法清單、錯誤橫幅都是同一種卡片。按鈕按下時，
// View 把索引交給 IMessageController::applyAdvice，Presenter 自己記得每個索引是什麼動作。
struct MessageCard {
    QString title;
    QString body;
    QStringList advice;
    QStringList actions;
};

class IMessageView {
public:
    virtual ~IMessageView() = default;
    virtual void showCard(const MessageCard& card) = 0;
    virtual void hideCard() = 0;
};

// 復原、重做按鈕。label 是「刪除 12 個元件」之類的說明，空字串 ＝ 沒有可做的。
class IHistoryView {
public:
    virtual ~IHistoryView() = default;
    virtual void setUndo(bool enabled, const QString& label) = 0;
    virtual void setRedo(bool enabled, const QString& label) = 0;
};

struct ParamsState {
    double threshold = 0.70;
    double angleMinDeg = -5.0;
    double angleMaxDeg = 5.0;
    int scopeIndex = 0;  // 0 ＝ 先板子後整庫、1 ＝ 只用板子、2 ＝ 整個樣本庫
    // 影像的有效位元數（S07）：設定值（0 ＝ 自動，8..16）、看最大值推測的值（16 bit 影像才有）、能不能設定（只有 16 bit 灰階）。
    int significantBits = 0;
    int detectedBits = 0;
    bool bitsApplicable = false;
};

class IParamsView {
public:
    virtual ~IParamsView() = default;
    virtual void setParams(const ParamsState& params) = 0;
    virtual void setScoreRange(double floor, double max) = 0;
    virtual void setEnabled(bool enabled) = 0;
};

// 物件樹（Board › Component › 框群組）。選取的方向：使用者點樹 → View 通知 ISelectionController；
// Presenter 要樹跟著動 → setSelection（程式設定的選取不會再回報）。
class IComponentTreeView {
public:
    virtual ~IComponentTreeView() = default;
    virtual void setRows(ObjectRowsPtr rows) = 0;
    virtual void setSelection(const std::vector<int>& sceneIds) = 0;
    virtual void setFilter(StateFilter filter) = 0;
};

// 畫布的輸入閘門：搜尋中、錯誤時畫布暫停（ux-spec 第 5 節），忙碌時游標變 progress。
// 縮放、平移不受影響（它們是畫布自己的檢視狀態）。
class ICanvasInputGate {
public:
    virtual ~ICanvasInputGate() = default;
    virtual void setToolsEnabled(bool enabled) = 0;
    virtual void setBusyCursor(bool busy) = 0;
};

}  // namespace aoi::ui
