#pragma once

#include <string_view>
#include <vector>

#include "application/CancelToken.h"
#include "application/Result.h"
#include "application/ports/ITemplateBuilder.h"
#include "domain/image_view.h"
#include "domain/match.h"
#include "domain/search_params.h"

// application 層：port。實作（adapter）在 infrastructure。
namespace aoi {

// 契約（S09 起由 tests/contract/tst_contract_searcher 對每個 back end 跑同一組；違反時的後果見 docs/steps/S09-contract-ab/spec.md）：
//  - 回傳的每個匹配：分數在 [0, 1] 且 ≥ params.minScore；角度在 [angleMinDeg, angleMaxDeg] 內（座標與角度慣例見 domain/geometry.h）。
//  - 依分數由高到低排序（同分時依 y、再依 x），最多 params.maxMatches 個。
//  - 匹配的「中心」落在 region 內：WholeImage ＝ 影像範圍內；Windows ＝ 任一視窗內（視窗搜尋只掃視窗）。
//    中心是 CAD **主體中心**：範本有比對區（ADR-026）時，搜尋器自己把比對區的位置換成主體中心（ITemplateBuilder.h）。
//  - 找不到就回空的 vector（成功），不是錯誤。
//  - 範本不是同一個 back end 建的，回 ErrorCode::BackendMismatch。影像無效回 ErrorCode::InvalidArgument。
//    參數不合法（minScore 不在 0..1 或不是數字、角度範圍不是有限值或 min > max）也回 InvalidArgument（S09 的契約測試確認三個 back end 一致）。
//  - 不丟例外。cancel 被觸發時盡快回 ErrorCode::Cancelled（單次函式庫呼叫不能中斷，所以延遲有上限）。
//  - 可以被多個 worker 執行緒同時呼叫：實作要無狀態或自己同步。
class IPatternSearcher {
public:
    virtual ~IPatternSearcher() = default;
    virtual std::string_view backendId() const noexcept = 0;
    virtual Result<std::vector<Match>> find(const ImageView& image, const ITemplate& tmpl, const SearchRegion& region,
                                            const SearchParams& params, const CancelToken& cancel) = 0;
};

}  // namespace aoi
