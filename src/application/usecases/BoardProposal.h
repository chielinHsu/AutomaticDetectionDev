#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "application/usecases/GenerateObjectsFromSample.h"
#include "domain/geometry.h"
#include "domain/lattice/non_max_suppression.h"
#include "domain/match.h"
#include "domain/object_state.h"

// application 層：只用標準庫。
//
// 2026-10-14 起「建立樣本」與「搜尋」分開（ADR-025）：使用者在板子上畫幾個樣本（每個樣本一個類型），按〔搜尋〕一次用
// 全部樣本各搜尋一次整張板子，結果**依位置合併**成一份「板子的提案」：同一個位置被幾個樣本找到時，只留分數最高的那一筆。
// 這個檔案是合併的規則與合併後的型別；每個樣本自己的搜尋仍是 GenerateObjectsFromSample（晶格補漏照舊）。
namespace aoi {

// 合併後的一個候選：和 Candidate 一樣，多了「它是哪個類型（樣本）的」與主體框的大小（接受時框由類型展開；這裡只是分類與畫面要用的矩形）。
struct BoardCandidate {
    std::string typeId;
    Pose2D pose;
    double score = 0.0;
    CandidateSource source = CandidateSource::Seed;
    Size2D partSize;

    bool hasScore() const noexcept { return source != CandidateSource::Predicted; }
};

// 板子的提案：不可變的快照（執行緒之間只傳這個）。
struct BoardProposal {
    // 順序：各樣本自己（Sample，依樣本順序；合併時通常已拿掉，見 mergeSampleProposals）、有分數的依分數由高到低、最後是預測的（Predicted）。
    std::vector<BoardCandidate> candidates;
    // 沒有任何樣本能搜尋時的原因（每個樣本都有問題時取第一個的）；有候選時是 nullopt。
    std::optional<NoMatchInfo> sampleProblem;
    std::size_t samplesSearched = 0;  // 實際搜尋了幾個樣本（有問題的不算）
    std::size_t merged = 0;           // 合併時被壓掉的候選數（同一個位置被不只一個樣本找到）
    std::size_t excluded = 0;         // 被手動補的元件佔住、剔除的候選數
    StageTimings timings;             // 全部樣本合計
};

// 一個樣本的搜尋結果：它的類型，加上 use case 的輸出。
struct SampleRun {
    std::string typeId;
    SampleProposal proposal;
};

// 單一樣本的提案 → 板子的提案（候選順序不變、每個都標成 typeId）。測試與 A/B 比較用；不合併、不剔除。
BoardProposal boardProposalFrom(const SampleProposal& proposal, std::string typeId);

// 合併幾個樣本的搜尋結果（ADR-025 的規則）：
//  1. occupied（手動補的元件的主體框）先佔位：和它們重疊的候選剔除（excluded）。
//  2. 各樣本自己（CandidateSource::Sample）**不放進結果**（樣本已經是板子上的元件），但它們的框佔位：找到樣本自己的匹配會被壓掉。
//  3. 有分數的候選依分數由高到低；和已留下的任何一個重疊超過 nms.maxOverlap 的壓掉（merged）。
//  4. 預測的（沒有分數）最後；和已留下的重疊就壓掉。
//  每個樣本有問題（sampleProblem）的不搜尋；全部都有問題（或沒有樣本）時 sampleProblem 是第一個問題，候選是空的。
BoardProposal mergeSampleProposals(const std::vector<SampleRun>& runs, const std::vector<RectObj>& occupied,
                                   const NmsOptions& nms);

}  // namespace aoi
