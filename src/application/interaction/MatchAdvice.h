#pragma once

#include <set>
#include <string>
#include <vector>

#include "application/backend/BackendRegistry.h"
#include "application/interaction/InspectionSession.h"

// application 層：只用標準庫（不含 Qt）。

namespace aoi {

// 比對區的建議（2026-10-29，R10、ADR-042）：back end 說它的範本用整個主體框會把相鄰的結構也當成元件
// （BackendTraits::suggestsMatchMask，目前只有 HALCON）時，建好樣本、還沒設定比對區（ComponentType::match 是預設 ＝
// 整個主體框）就要搜尋的使用者，應該先用 M 工具點一下端子、用自動擷取的遮罩當比對區（ADR-028）。
//
// 「現在該提醒誰」是規則，從 InspectionSession 的狀態與資料推導（ADR-011 的做法：沒有 Qt，tst_match_advice 逐條驗證）；
// Presenter 只把結果變成訊息列的卡片。使用者按〔關閉〕的樣本記在這裡，之後不再提醒（同一個樣本只提醒一次）；
// 設了比對區的樣本自然不再符合條件。
//
// 規則：
//  - back end 的 traits.suggestsMatchMask 是 false → 永遠沒有（OpenCV、假 back end）。
//  - 只在 Idle：建完樣本、接受、捨棄、取消、切換 back end 之後都會回到 Idle；預覽、搜尋中由那些狀態的卡片說話。
//  - 板上每一個樣本：它的類型的 match 是預設（沒有比對區、灰階）而且還沒按過〔關閉〕→ 列進去（依樣本在板上的順序）。
//    類型有比對區（矩形或遮罩）、或只改了特徵（邊緣）的樣本不列：使用者已經處理過比對區了。
//  - dismiss 記的是類型 id：同類型的樣本一起不再提醒；再框一個新樣本是新的類型，會再提醒一次。
class MatchAdvice {
public:
    // 現在該提醒的樣本（空 ＝ 不提醒）。
    std::vector<SampleOnBoard> pending(const InspectionSession& session, const BackendTraits& traits) const;

    // 使用者按〔關閉〕：這些樣本（的類型）之後不再提醒。
    void dismiss(const std::vector<SampleOnBoard>& samples);
    bool dismissed(const std::string& typeId) const { return dismissed_.count(typeId) > 0; }
    // 換了板子（開新影像）：關閉過的記錄沒有意義了。
    void reset() { dismissed_.clear(); }

private:
    std::set<std::string> dismissed_;
};

}  // namespace aoi
