#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "application/CancelToken.h"
#include "application/Result.h"
#include "domain/geometry.h"
#include "domain/image_view.h"
#include "domain/match_region.h"
#include "domain/object_state.h"
#include "domain/search_params.h"

// application 層：port。實作（adapter）在 infrastructure。
namespace aoi {

// 範本：由某個 back end 從樣本建出來的比對模型。除了建出它的那一組 adapter，沒有人看得懂裡面。
// （OpenCV：旋轉過的影像金字塔；HALCON：shape model。這裡只暴露 use case 需要的兩三件事。）
class ITemplate {
public:
    virtual ~ITemplate() = default;
    // 範本的大小（影像像素）：每個候選元件的主體框用這個大小。
    virtual Size2D size() const noexcept = 0;
    // 建出它的 back end。搜尋器用它拒絕別家的範本（ErrorCode::BackendMismatch）。
    virtual std::string_view backendId() const noexcept = 0;
    // 建範本的時候就看得出來的樣本問題（框到不只一個元件、沒框到元件…）。看不出來的 adapter 回 nullopt。
    virtual std::optional<NoMatchReason> sampleProblem() const noexcept { return std::nullopt; }
};

using TemplatePtr = std::shared_ptr<const ITemplate>;

// 契約（S09 起由 tests/contract/tst_contract_searcher 對每個 back end 跑同一組；違反時的後果見 docs/steps/S09-contract-ab/spec.md）：
//  - build 與 find 可以被多個 worker 執行緒同時呼叫（latest-wins 時，新舊工作會重疊）：實作要無狀態或自己同步。
//  - 不丟例外；失敗回 Error。cancel 被觸發時回 ErrorCode::Cancelled。
//  - 影像無效或樣本矩形超出影像時回 ErrorCode::InvalidArgument。
// 比對區（ADR-026，buildFromSample）：
//  - 範本只用 spec.match 的像素（矩形或遮罩）與特徵（灰階或邊緣）；spec.body 是 CAD 主體框。
//  - 搜尋器回報的 Match.pose 一律是**主體中心**（origin），不是比對區的中心；ITemplate::size() 仍是主體框的大小。
//    所以 use case、NMS、晶格與 CAD 框都不必知道比對區。
//  - spec.match.isDefault() 時，結果和 build(image, spec.body, …) 完全相同（向下相容）。
//  - 比對區（或遮罩）與影像的交集是空的，回 ErrorCode::InvalidArgument。
class ITemplateBuilder {
public:
    virtual ~ITemplateBuilder() = default;
    virtual std::string_view backendId() const noexcept = 0;
    virtual Result<TemplatePtr> build(const ImageView& image, const Rect& sample, const SearchParams& params,
                                      const CancelToken& cancel) = 0;
    // 預設實作：整框時交給 build；不支援比對區的實作（例如測試替身）明確拒絕，不會默默忽略。
    virtual Result<TemplatePtr> buildFromSample(const ImageView& image, const SampleSpec& spec, const SearchParams& params,
                                                const CancelToken& cancel) {
        if (spec.match.isDefault()) return build(image, spec.body, params, cancel);
        return makeError(ErrorCode::InvalidArgument, "This back end does not support match regions.");
    }
};

}  // namespace aoi
