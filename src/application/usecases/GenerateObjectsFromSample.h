#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "application/CancelToken.h"
#include "application/Result.h"
#include "application/ports/IPatternSearcher.h"
#include "application/ports/IPerfProbe.h"
#include "application/ports/IProgressSink.h"
#include "application/ports/ITemplateBuilder.h"
#include "domain/geometry.h"
#include "domain/image_view.h"
#include "domain/lattice/lattice_estimator.h"
#include "domain/lattice/non_max_suppression.h"
#include "domain/match.h"
#include "domain/object_state.h"
#include "domain/search_params.h"

// application 層。流程 A「教樣本」的 use case：使用者框一個元件 → 找出板上所有相似的元件 → 補漏。
//
// 流程（docs/steps/S03-core/diagram.md；S05 的晶格部分在 docs/steps/S05-lattice/diagram.md）：
//   檢查樣本 → 建範本 → 種子搜尋（整張影像）→ 合併重複的匹配（NMS）→ 擬合晶格（可能多組）→ 預測缺格
//   → 視窗驗證 → 把驗證找到的加進晶格重新擬合（O(1)／個）→ 候選清單
//
// use case 只依賴 port（ITemplateBuilder、IPatternSearcher）；換 back end 不必改這個類別。晶格與 NMS 是 domain 的純 C++。
namespace aoi {

// 策略參數：use case 自己的門檻與限制。預設值是起點，不是量測出來的事實。
struct GenerateOptions {
    double seedMinScore = 0.70;   // 種子搜尋（整張影像）的分數下限：高，避免整張影像上滿是誤判
    double candidateFloor = 0.30; // 視窗驗證的分數下限：低，因為位置已由晶格限定（ux-spec §12.1 第 3 項）
    double minSamplePx = 8.0;     // 樣本邊長小於這個值就不搜尋（ux-spec 第 8 節「最小樣本」）
    double maxSampleAreaFraction = 0.25;  // 樣本面積超過影像的這個比例，判定比元件大很多
    double minContrast = 3.0;     // 樣本亮度標準差（0..255）低於這個值，判定對比太低
    double windowScale = 1.5;     // 驗證視窗的大小 ＝ 範本大小 × 這個倍數
    std::size_t maxPredictions = 20000;  // 最多預測幾個缺格（超過時留離樣本最近的）
    LatticeOptions lattice;       // 晶格估計（S05，domain/lattice）
    NmsOptions nms;               // 重疊多少算「同一個元件」：種子搜尋的重複匹配、與已有候選重疊的缺格（主體框大小）
};

struct SampleRequest {
    ImageView image;
    Rect sample;  // 影像像素（座標慣例見 domain/geometry.h）
    // 角度範圍與 maxMatches 由這裡來；minScore 會被 use case 依階段覆寫（seedMinScore、candidateFloor）。
    SearchParams search;
    GenerateOptions options;
    // 建範本用的比對區與特徵（ADR-028；影像像素）。預設 ＝ 整個 sample、灰階（和之前相同）。
    MatchSpec match;
};

struct StageTimings {
    double templateMs = 0.0;
    double seedMs = 0.0;
    double fitMs = 0.0;
    double verifyMs = 0.0;
    double totalMs = 0.0;
};

// 晶格的摘要。有多組晶格時，u、v、rmsPx、coverage、confidence、suggestCheck 是主晶格（內點最多）的。
struct GridSummary {
    bool found = false;   // 有沒有晶格（點太少、不成陣列、或信心不足時沒有：只回傳找到的，不補漏）
    bool hasV = false;    // 二維；false ＝ 一維（一排）
    Point2D u;
    Point2D v;
    double rmsPx = 0.0;
    std::size_t holes = 0;     // 預測的缺格數（所有晶格合計，扣掉與已有候選重疊的）
    std::size_t lattices = 0;  // 幾組晶格（例如上下兩排方向不同時是 2）
    double coverage = 0.0;     // 主晶格：找到的格子 ÷ 索引範圍內的格子
    double confidence = 0.0;   // 主晶格的信心（0..1；domain/lattice/lattice_confidence.h）
    bool suggestCheck = false; // 主晶格的信心偏低：結果要人看（UI 的「建議檢查」；S05 只提供，畫面還沒顯示）
    NoLatticeReason noLattice = NoLatticeReason::None;  // found ＝ false 時的原因（2 個點以上才有意義）
};

// use case 的輸出：不可變的快照（執行緒之間只傳這個，不共享可變狀態）。
struct SampleProposal {
    // 所有候選，順序：Sample、Seed（搜尋器的順序，合併重複之後）、Verified 與 Predicted（離樣本由近到遠）。
    // 分數 ≥ 搜尋下限的都在裡面；門檻由 Presenter 套（見 ProposalView.h 的 classify）。
    std::vector<Candidate> candidates;
    Size2D partSize;  // 範本大小（影像像素）：每個候選的主體框用這個大小
    // 搜尋之前就知道的樣本問題（太小、太大、對比太低、框到多個、沒框到元件）：這時 candidates 是空的。
    // 與門檻有關的原因（只有一個、門檻太高）由 classify 算。
    std::optional<NoMatchInfo> sampleProblem;
    GridSummary grid;
    StageTimings timings;
};

// 無狀態，可被多個 worker 同時呼叫 run()。引用的 port 物件要活得比它久。
class GenerateObjectsFromSample {
public:
    // probe 可以是空指標（不量測）。
    GenerateObjectsFromSample(ITemplateBuilder& builder, IPatternSearcher& searcher, IPerfProbe* probe = nullptr);

    // 失敗（影像無效、back end 失敗、被取消）回 Error；「沒有結果」是成功，原因在 proposal.sampleProblem 或 classify。
    // progress 可以是空指標。cancel 在每個階段之間與搜尋器內部檢查。
    Result<SampleProposal> run(const SampleRequest& request, const CancelToken& cancel,
                               IProgressSink* progress = nullptr) const;

private:
    ITemplateBuilder& builder_;
    IPatternSearcher& searcher_;
    IPerfProbe* probe_;
};

}  // namespace aoi
