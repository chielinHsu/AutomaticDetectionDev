#pragma once

#include <cstddef>
#include <optional>
#include <vector>

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
    // 沒有可用結果的原因；有結果時是 nullopt。
    //  - 搜尋前就知道的樣本問題：直接取自 proposal.sampleProblem。
    //  - OnlyOne：板上只有樣本自己（沒有任何其他候選）。
    //  - ThresholdTooHigh：有候選，但分數都低於門檻；附最高分與「降低到 {suggested}」的建議值。
    std::optional<NoMatchInfo> noMatch;
};

// threshold：0..1。分數 ≥ threshold 是 Found，否則 Rejected；Predicted 一律是 Inferred（沒有分數）；Sample 一律是 Sample。
// candidateFloor：建議門檻的下限，要與產生這份提案時的 GenerateOptions::candidateFloor 一致。
ProposalView classify(const SampleProposal& proposal, double threshold, double candidateFloor = 0.30);

}  // namespace aoi
