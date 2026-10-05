#pragma once

#include <QString>
#include <vector>

#include "application/Error.h"
#include "application/interaction/InspectionSession.h"
#include "domain/object_state.h"
#include "presentation/presenters/SceneBuilder.h"
#include "presentation/presenters/ViewInterfaces.h"

namespace aoi::ui {

// 訊息卡上每個按鈕做什麼。Presenter 記得 actions[i] 對應 card.actions[i]。
enum class CardAction {
    Accept,
    Discard,
    Dismiss,
    Retry,
    Redraw,          // 重畫樣本：關掉訊息、切到「教樣本」工具
    TeachNew,        // 教一個新樣本：同上
    LowerThreshold,  // 降低門檻到建議值（不重新搜尋）
    WidenScope,      // 改用整個樣本庫（重新搜尋）
};

struct CardSpec {
    MessageCard card;
    std::vector<CardAction> actions;  // 與 card.actions 一一對應
    double suggestedThreshold = 0.0;  // LowerThreshold 用
};

// 全部是純函式：輸入是資料，輸出是已翻譯的文字與動作表，所以可以不開視窗直接測。
// 文案的 key 都在 docs/ux/copy.json（tst_messages 驗證每個用到的 key 都存在、兩種語言都有）。

// 預覽的動作列：摘要、操作提示、信心偏低的提醒，以及〔接受 (Enter)〕〔捨棄 (Esc)〕。
CardSpec previewCard(const SceneCounts& counts);

// 沒有結果時的卡片：原因、下一步建議，以及對應的動作按鈕，最後一定有〔關閉〕（只用滑鼠也要走得完）。
CardSpec noMatchCard(const NoMatchInfo& info);

// 錯誤橫幅。imageName 只用在影像開啟失敗；fallbackBackend 只用在 back end 無法使用。
CardSpec errorCard(const Error& error, ErrorSource source, const QString& imageName, const QString& fallbackBackend);

// 復原／重做按鈕上的說明：「接受 4 個元件」「刪除 12 個元件」…
QString historyLabel(CommandKind kind, std::size_t count);

}  // namespace aoi::ui
