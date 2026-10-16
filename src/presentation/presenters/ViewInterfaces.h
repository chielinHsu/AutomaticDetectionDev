#pragma once

#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>
#include <cstddef>
#include <memory>
#include <vector>

#include "application/interaction/StepGuide.h"
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

// back end 選單的一項。
struct BackendChoice {
    QString name;
    bool available = true;
    QString note;  // 不可用的原因，或可用時的提醒；空 ＝ 沒有
    QString id;    // BackendRegistry 的 id（選單選了它時交給 IBackendController）
};

// back end 這一塊畫面（BackendPresenter）：上方的選單、狀態列最左邊的標示、訊息列的提醒卡片。
// 提醒卡片比 IMessageView 的卡片低一級：那些卡片或進度列顯示時它讓位，收起之後再出現，直到 hideNotice。
// 按鈕被按下時，View 把索引交給 IBackendController::applyNoticeAction；選單選了一項時交給 selectBackend。
class IBackendView {
public:
    virtual ~IBackendView() = default;
    // switching ＝ 正在換（選單停用）；current 是選單上顯示為選取的那一項。
    virtual void setBackends(const QList<BackendChoice>& choices, int current, bool switching) = 0;
    // 狀態列：「HALCON · runtime 授權」；detail 是滑鼠移上去看的完整說明。
    virtual void setBackendStatus(const QString& text, const QString& detail) = 0;
    virtual void showNotice(const MessageCard& card) = 0;
    virtual void hideNotice() = 0;
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
    // 位置（ADR-026）：true ＝ 吸附到陣列（晶格的格點，CAD 的理論位置），false ＝ 用 back end 的匹配位置。
    bool snapToLattice = true;
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

// 選取資訊的表單（物件樹下方，ux-spec 第 4 節）：選了幾個；恰好一個時它的名稱、類型、中心、大小、角度、框數，
// 以及現在能不能改位置、改大小（規則由 application 的 InspectionSession::selectionInfo 決定，這裡只是顯示用的複本）。
// 數字都是影像像素；umPerPixel > 0 時 View 在旁邊同時顯示 µm。多選時 View 停用表單、只顯示「已選 N 個」。
struct SelectionInfoView {
    std::size_t count = 0;
    int sceneId = 0;
    QString name;       // 「X1」；提案物件是「#3」
    QString typeLabel;  // 料號；暫時類型沒有料號時是類型編號
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    double angleDeg = 0.0;
    std::size_t boxCount = 0;
    double umPerPixel = 0.0;
    bool canMove = false;
    bool canResize = false;
    // 晶格（ADR-026；提案的物件才有）：是不是晶格的成員、使用者有沒有移過、現在的位置是不是格點；
    // 匹配位置離最近格點多遠（< 0 ＝ 不在任何陣列範圍內、或板子上的元件）。
    bool proposal = false;
    bool inLattice = false;
    bool moved = false;
    bool onLattice = false;
    double latticeDeviationPx = -1.0;
};

class ISelectionInfoView {
public:
    virtual ~ISelectionInfoView() = default;
    virtual void setSelectionInfo(const SelectionInfoView& info) = 0;
};

// 樣本庫 dock 裡「板子上的樣本」清單（2026-10-14）：每一筆是從目前影像裁下來的縮圖（疊上主體框與子框）、名稱、大小、框數。
// 資料只來自板子（InspectionDocument::samples），不讀寫樣本庫檔案。點一筆 ＝ 在畫布上選取它（ISelectionController::select）。
struct SampleEntry {
    int sceneId = 0;
    QString name;
    QString typeLabel;
    QImage thumbnail;  // 已縮到固定寬度
    double width = 0.0;   // 主體框（影像像素）
    double height = 0.0;
    std::size_t boxCount = 0;
};

struct SampleList {
    std::vector<SampleEntry> entries;
    double umPerPixel = 0.0;
};

using SampleListPtr = std::shared_ptr<const SampleList>;

class ISampleListView {
public:
    virtual ~ISampleListView() = default;
    // 同一個指標再交一次 ＝ 沒有變（View 可以略過）。
    virtual void setSamples(SampleListPtr samples) = 0;
    virtual void setSelection(const std::vector<int>& sceneIds) = 0;
};

// 「操作步驟」dock（2026-10-15，ADR-027）：五步的清單、目前這一步標亮、每一步的示範。
// 哪一步是目前、哪些做完了由 application 的 guideStatus（InspectionSession 推導）決定，Presenter 只在它變了的時候才交過來，
// 所以分數門檻拖動（U3）不會碰到這個 View。文字由 View 自己從 copy.json 設定（步驟是固定的，不是資料）。
class IGuidanceView {
public:
    virtual ~IGuidanceView() = default;
    virtual void setGuideStatus(const GuideStatus& status) = 0;
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
