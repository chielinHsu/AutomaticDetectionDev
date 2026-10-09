#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "application/CancelToken.h"
#include "application/Result.h"
#include "application/ports/IProgressSink.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "domain/geometry.h"
#include "domain/match.h"
#include "domain/object_state.h"

// application 層（S09）：A/B 比較。同一個樣本、同一張影像，兩個 back end 各跑一次流程 A 的 use case，再把兩份結果配對：
// 兩邊都找到、只有 A、只有 B，以及「另一邊為什麼沒有」（分數低於門檻、只是晶格推算、附近沒有任何候選）。
//
// 這裡只有規則（純 C++，沒有 Qt）：比較畫面（presentation/compare）與效能量測（tests/contract/perf_ab）用同一份，
// 所以畫面上看到的差異和量測報告的數字是同一個定義。分數門檻變動時只重新比較（不重新搜尋），和 classify 一樣便宜。
namespace aoi {

enum class CompareKind {
    Both,   // 兩邊都找到（Sample 或分數 ≥ 該邊門檻的 Found）
    OnlyA,  // 只有 A 找到
    OnlyB,  // 只有 B 找到
};

// 另一邊為什麼沒有找到（OnlyA 時說 B、OnlyB 時說 A）。
enum class CompareGap {
    None,            // Both
    BelowThreshold,  // 另一邊在這裡有候選，但分數低於它的門檻（Rejected）：可能只是門檻的問題（兩邊的分數不是同一把尺）
    InferredOnly,    // 另一邊在這裡只有晶格推算（Predicted）：它預測這裡有元件，但視窗驗證沒有找到
    NotFound,        // 另一邊在這附近沒有任何候選：搜尋本身沒有找到
};

// 一邊在某個位置的候選（配對到的那一個）。
struct CompareHit {
    bool present = false;  // 這一邊在這個位置有候選（任何狀態）；false 時其他欄位沒有意義
    Pose2D pose;
    ObjectState state = ObjectState::Rejected;
    std::optional<double> score;  // Predicted 沒有分數
};

struct ComparedItem {
    CompareKind kind = CompareKind::Both;
    CompareGap gap = CompareGap::None;
    CompareHit a;
    CompareHit b;
    double distancePx = 0.0;    // 兩邊都有候選時的中心距離
    double angleDiffDeg = 0.0;  // 兩邊都有候選時的角度差（B − A，正規化到 (−180, 180]）
    // 畫在哪裡：找到的那一邊的位姿（Both 用 A 的）。
    Pose2D pose() const noexcept { return kind == CompareKind::OnlyB ? b.pose : a.pose; }
};

struct CompareSideSummary {
    std::size_t candidates = 0;  // use case 回傳的全部候選
    std::size_t found = 0;       // Sample ＋ Found（分數 ≥ 門檻）
    std::size_t inferred = 0;
    std::size_t rejected = 0;
    bool sampleProblem = false;  // 搜尋之前就發現樣本有問題（這時沒有候選）
    bool lattice = false;        // 有沒有晶格
    StageTimings timings;
};

struct CompareSummary {
    std::size_t both = 0;
    std::size_t onlyA = 0;
    std::size_t onlyB = 0;
    // 差異的原因（OnlyA 依 B 的情況分、OnlyB 依 A 的情況分）。
    std::size_t onlyABelowThreshold = 0;
    std::size_t onlyAInferredOnly = 0;
    std::size_t onlyANotFound = 0;
    std::size_t onlyBBelowThreshold = 0;
    std::size_t onlyBInferredOnly = 0;
    std::size_t onlyBNotFound = 0;
    // Both 的位置與角度差（兩邊對同一個元件的位姿差多少）。
    double meanDistancePx = 0.0;
    double maxDistancePx = 0.0;
    double maxAngleDiffDeg = 0.0;  // 絕對值
    CompareSideSummary a;
    CompareSideSummary b;
};

struct CompareOptions {
    // 兩個候選的中心相距多少以內算「同一個元件」：max(minTolerancePx, 元件短邊 × toleranceFraction)。
    // 元件的格距至少是短邊的 1.5 倍（相鄰元件不會被當成同一個）；位置差遠小於它（T6b：0.5 px）。
    double toleranceFraction = 0.25;
    double minTolerancePx = 3.0;
};

struct BackendComparison {
    // 差異在前（OnlyA、再 OnlyB），然後是 Both；同一類依 y、再依 x。兩邊都沒找到的位置（都 Rejected 或都只是推算）不列。
    std::vector<ComparedItem> items;
    CompareSummary summary;
    double tolerancePx = 0.0;
};

// 純函式。a、b 是兩個 back end 對同一個請求的提案；門檻各自套（兩邊的分數不是同一把尺：NCC 與邊緣方向的一致度）。
// 配對：兩邊的所有候選（含 Rejected 與 Predicted）在容差內、由近到遠一對一配對。
BackendComparison compareProposals(const SampleProposal& a, double thresholdA, const SampleProposal& b, double thresholdB,
                                   const CompareOptions& options = {});

// 兩個 back end 對同一個請求的提案。
struct SideBySide {
    SampleProposal a;
    SampleProposal b;
};

// 依序跑 A、再跑 B（不同時跑：每一邊都用得到所有執行緒，耗時才能比）。cancel 在兩次之間與 use case 內部檢查。
// progress（可為空）：A 回報 [0, 0.5)、B 回報 [0.5, 1]（同一個 Stage；每個階段各自從 0 到 1）。任一邊失敗就回那個 Error。
Result<SideBySide> runSideBySide(const GenerateObjectsFromSample& a, const GenerateObjectsFromSample& b, const SampleRequest& request,
                                 const CancelToken& cancel, IProgressSink* progress = nullptr);

}  // namespace aoi
