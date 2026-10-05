#pragma once

#include <QString>
#include <cstddef>

#include "application/interaction/InspectionSession.h"
#include "presentation/canvas/ComponentSnapshot.h"
#include "presentation/objects/ObjectRows.h"

namespace aoi::ui {

// 畫面上有哪些文字是建構場景時要用的（已翻譯）。語言切換時由 Presenter 重新算。
struct SceneLabels {
    QString sample;    // 「樣本」
    QString inferred;  // 「推算」
    QString manual;    // 「手動」
    QString board;     // 「板子 board.png」的格式化函式由呼叫端準備：這裡直接放最後的標題
};

struct SceneCounts {
    std::size_t components = 0;
    std::size_t boxes = 0;
    std::size_t found = 0;       // 找到的（含使用者框的樣本，它也是一個找到的零件）
    std::size_t inferred = 0;
    std::size_t rejected = 0;
    std::size_t lowConfidence = 0;  // 推算的，加上分數離門檻不到 0.10 的找到的（S01 原型的 LOWCONF）
    bool fromProposal = false;
};

struct Scene {
    SnapshotPtr snapshot;  // 畫布要畫的：文件裡的元件，加上目前的提案（若有）
    ObjectRowsPtr rows;    // 物件樹要列的：有提案時是提案的物件，否則是文件裡的元件
    SceneCounts counts;    // 同上：統計的是 rows 的那一邊
};

// 把工作階段目前的內容轉成畫面要的資料。純函式：不碰 Qt Widgets，只做資料轉換，所以可以直接測、直接量。
//
// 框是從元件類型的框群組與元件位姿推導出來的（domain/box_expander.h）；提案物件還沒有類型，每個只有一個主體框。
// 提案物件的編號加上 kProposalIdBase，與文件裡的元件編號分開。
Scene buildScene(const InspectionSession& session, const SceneLabels& labels);

}  // namespace aoi::ui
