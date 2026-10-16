#pragma once

#include <QString>
#include <cstddef>
#include <cstdint>
#include <vector>

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
    std::size_t merged = 0;    // 提案：合併時被壓掉的候選（同一個位置被不只一個樣本找到；ADR-025）
    std::size_t excluded = 0;  // 提案：被手動補的元件佔住、剔除的候選
    std::size_t replaced = 0;  // 提案：接受時會被換掉的元件數（板子上上一次自動產生的，ADR-025）
    std::size_t offLattice = 0;  // 提案：找到的與推算的之中不在陣列格點上的（沒有晶格位姿，位置是匹配的；ADR-026）
};

struct Scene {
    SnapshotPtr snapshot;  // 畫布要畫的：文件裡的元件，加上目前的提案（若有）
    ObjectRowsPtr rows;    // 物件樹要列的：有提案時是提案的物件，否則是文件裡的元件
    SceneCounts counts;    // 同上：統計的是 rows 的那一邊
};

// 文件那一部分（已接受的元件）的快取（S11）。門檻一動只有提案變，文件的框、名稱與列不必重算：
// 4 883 個已接受的元件每次重算要 9 ms（展開框、名稱、列）。鍵是文件的修訂號（InspectionDocument::revision）與
// 標籤文字（語言切換時會變）。由呼叫端保存（Presenter 一份），buildScene 自己判斷能不能用、用完更新。
struct DocumentSceneCache {
    bool valid = false;
    std::uint64_t revision = 0;
    SceneLabels labels;
    SnapshotPtr snapshot;  // 只有文件元件的快照；沒有提案時整份直接共用
    ObjectRowsPtr rows;
    SceneCounts counts;
};

// 把工作階段目前的內容轉成畫面要的資料。純函式：不碰 Qt Widgets，只做資料轉換，所以可以直接測、直接量。
//
// 框是從元件類型的框群組與元件位姿推導出來的（domain/box_expander.h）；提案物件還沒有類型，每個只有一個主體框。
// 提案物件的編號加上 kProposalIdBase，與文件裡的元件編號分開。
// cache 給了就用（命中時文件那一部分不重算；沒命中時重算並更新快取）；nullptr ＝ 每次全部重算。
Scene buildScene(const InspectionSession& session, const SceneLabels& labels, DocumentSceneCache* cache = nullptr);

}  // namespace aoi::ui
