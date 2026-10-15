#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/geometry.h"
#include "domain/object_state.h"

// domain 層：只用 C++ 標準庫。座標與角度慣例見 geometry.h；本檔所有座標都是影像像素。
namespace aoi {

// 搜尋器回報的一個匹配：中心位姿與分數（0..1）。
struct Match {
    Pose2D pose;
    double score = 0.0;
};

// 一個候選元件的來源（流程 A）。
enum class CandidateSource {
    Sample,     // 使用者框的那一個；分數是 1.0（範本就是從它建的，依定義完全相符）或搜尋器回報的自我匹配分數
    Seed,       // 種子搜尋（整張影像，較高的分數下限）找到
    Verified,   // 晶格預測的位置，在小視窗內以較低的分數下限找到
    Predicted,  // 晶格預測的位置，視窗內沒有找到；沒有分數
};

// use case 回傳的候選。**所有**驗證過的候選都回傳（分數 ≥ 搜尋下限），由 Presenter 套門檻：
// 分數門檻變動時只要重新過濾，不必重新搜尋（ux-spec §12.1 第 3 項、預算 U3）。
struct Candidate {
    Pose2D pose;         // back end 匹配到的位姿（Predicted 是格點）。契約測試與 A/B 比較量的是它，不會被晶格改掉
    double score = 0.0;  // Predicted 沒有分數，固定為 0，不可拿來排序或顯示
    CandidateSource source = CandidateSource::Seed;
    // 晶格的位姿（ADR-026）：這個候選是某一組晶格的成員（內點，或 Predicted）時，它的格點：位置 ＝ origin ＋ i·u ＋ j·v
    // （視窗驗證之後重新擬合的基底），角度 ＝ 樣本的角度（同一個陣列的元件方向一致）。CAD 框的本質是料件的理論位置，
    // 預覽與接受預設用它（PositionMode::Lattice）。不屬於任何晶格時 nullopt：單獨的元件、沒有晶格的板子。
    std::optional<Pose2D> latticePose;
    // 匹配位置離最近格點的距離（像素）。成員是 |pose − latticePose|；落在某組晶格的索引範圍內、但離格點超過內點容差
    // （所以不是成員）的候選也有：它「偏離陣列」，位置保留匹配的、畫面標示。不在任何晶格範圍內的沒有。
    std::optional<double> latticeDeviationPx;

    bool hasScore() const noexcept { return source != CandidateSource::Predicted; }
    bool onLattice() const noexcept { return latticePose.has_value(); }
};

// 提案裡的候選用哪一個位置（ADR-026）。Lattice：有晶格位姿的用格點（CAD 的理論位置）、其餘用匹配位置；Matched：一律用匹配位置。
// 切換只是重新分類（像分數門檻），不重新搜尋。
enum class PositionMode { Lattice, Matched };

// 套過門檻的結果：一個候選元件與它的狀態。rect 是主體框；接受後成為那個類型的一個元件（框由類型展開）。
struct InspectionObject {
    int id = 0;  // 從 1 起，依候選順序；每次 classify 重新編號
    RectObj rect;
    ObjectState state = ObjectState::Found;
    std::optional<double> score;  // 手動補的、Predicted 的沒有
    ObjectOrigin origin = ObjectOrigin::Auto;
    std::string typeId;  // 板子上的類型（2026-10-14 起每個樣本一個類型）；空字串 ＝ 還沒有類型（只有測試會這樣）
    // 晶格（ADR-026）：rect.pose 是不是格點（PositionMode::Lattice 而且候選有晶格位姿；Predicted 一律是）；
    // 匹配位置離最近格點的距離（候選的 latticeDeviationPx；手動補的沒有）。
    bool onLattice = false;
    std::optional<double> latticeDeviationPx;
};

// 流程 B：一個辨識出的元件，附每個候選類型的分數（由高到低）。
struct TypeScore {
    std::string typeId;
    double score = 0.0;
};

struct ComponentCandidate {
    Pose2D pose;
    std::vector<TypeScore> typeScores;  // 空 ＝ 認得這裡有元件，但沒有任何類型的分數 ≥ 下限
};

}  // namespace aoi
