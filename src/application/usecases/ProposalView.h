#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "application/usecases/BoardProposal.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "domain/match.h"
#include "domain/object_state.h"

// application 層。把 use case 回傳的候選依「分數門檻」分成物件與狀態。
//
// 為什麼分開：分數門檻是使用者會一直拖的滑桿（預算 U3：16 ms）。use case 一次回傳所有候選與分數，
// 門檻變動時 Presenter 只呼叫 classify（純函式、不搜尋、不碰 port），不必重新跑搜尋。
namespace aoi {

struct ProposalView {
    std::vector<InspectionObject> objects;  // 依候選順序，id 從 1 起
    std::size_t found = 0;                  // 狀態為 Found 的數量（不含 Sample）
    std::size_t rejected = 0;
    std::size_t inferred = 0;
    std::size_t offLattice = 0;             // Found 與 Inferred 之中沒有晶格位姿的（不在陣列的格點上：單獨的元件、偏離陣列的；ADR-026）
    // 沒有可用結果的原因；有結果時是 nullopt。
    //  - 搜尋前就知道的樣本問題：直接取自 proposal.sampleProblem。
    //  - OnlyOne：板上只有樣本自己（沒有任何其他候選）。
    //  - ThresholdTooHigh：有候選，但分數都低於門檻；附最高分與「降低到 {suggested}」的建議值。
    std::optional<NoMatchInfo> noMatch;
};

// threshold：0..1。分數 ≥ threshold 是 Found，否則 Rejected；Predicted 一律是 Inferred（沒有分數）；Sample 一律是 Sample。
// candidateFloor：建議門檻的下限，要與產生這份提案時的 GenerateOptions::candidateFloor 一致。
// 板子的提案（幾個樣本合併之後，ADR-025）：每個物件帶著它的類型（typeId）。
// mode（ADR-026）：Lattice ＝ 有晶格位姿的物件用格點當 rect.pose（CAD 的理論位置），其餘用匹配位置；Matched ＝ 一律匹配位置。
ProposalView classify(const BoardProposal& proposal, double threshold, double candidateFloor = 0.30,
                      PositionMode mode = PositionMode::Lattice);
// 單一樣本的提案（A/B 比較與測試）：同一套規則，typeId 是空字串。
ProposalView classify(const SampleProposal& proposal, double threshold, double candidateFloor = 0.30,
                      PositionMode mode = PositionMode::Lattice);

}  // namespace aoi
