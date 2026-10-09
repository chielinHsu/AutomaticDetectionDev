#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "application/CancelToken.h"
#include "application/Result.h"
#include "application/ports/IProgressSink.h"
#include "domain/component.h"
#include "domain/geometry.h"
#include "domain/image_view.h"
#include "domain/match.h"
#include "domain/object_state.h"
#include "domain/search_params.h"

// application 層：port。實作在 S07（OpenCV）與 S08（HALCON）；S03 只定義介面與回傳型別。
namespace aoi {

struct LibraryMatchResult {
    std::vector<ComponentCandidate> components;
    // 實際比對了幾個樣本（狀態列的「比對 N 個樣本」）。比對範圍由呼叫端決定（ADR-006 §7），matcher 不自己擴大。
    std::size_t samplesCompared = 0;
    std::optional<NoMatchInfo> noMatch;  // components 為空時的原因
};

// 流程 B（區域掃描）：框任意區域，與候選類型比對，求每個元件的類型、位姿與分數。
// 契約（S09 起由 tests/contract/tst_contract_library_matcher 對每個有實作的 back end 跑同一組）：
//  - candidates 是呼叫端挑好的清單（先板子已有的類型、不夠再整個庫）；matcher 不自己掃整個庫（ux-spec §12.1 第 11 項）。
//  - 每個 ComponentCandidate 的 typeScores 由高到低排序；分數在 [0, 1] 且 ≥ params.minScore。
//  - 取消在「每比對一個樣本之間」生效（回 ErrorCode::Cancelled）；progress（可為空）每比對完一個樣本回報一次（Stage::Match）。
//  - 不丟例外；可以被多個 worker 執行緒同時呼叫。
class ILibraryMatcher {
public:
    virtual ~ILibraryMatcher() = default;
    virtual Result<LibraryMatchResult> match(const ImageView& image, const Rect& region,
                                             const std::vector<ComponentType>& candidates, const SearchParams& params,
                                             const CancelToken& cancel, IProgressSink* progress) = 0;
};

}  // namespace aoi
