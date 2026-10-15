#pragma once

#include <string>
#include <vector>

#include "application/CancelToken.h"
#include "application/Result.h"
#include "application/ports/IProgressSink.h"
#include "application/usecases/BoardProposal.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "domain/geometry.h"
#include "domain/image_view.h"
#include "domain/search_params.h"

// application 層。「搜尋」這個動作（ADR-025）：板子上的每個樣本各跑一次 GenerateObjectsFromSample（整張板子、晶格補漏照舊），
// 結果依位置合併成一份板子的提案（mergeSampleProposals）。
//
//  - 樣本依序搜尋；取消在每個樣本之間與 use case 內部都檢查。
//  - 進度：第 i 個樣本（共 n 個）的 use case 進度 f 回報成 (i + f) ÷ n（Stage 照 use case 的）。
//  - 無狀態；引用的 use case 要活得比它久。
namespace aoi {

struct SampleSearch {
    std::string typeId;  // 這個樣本的類型（板子的）
    Rect sample;         // 樣本框（影像像素）
    MatchSpec match;     // 比對區與特徵（ADR-026；預設 ＝ 整個樣本框、灰階）
};

struct BoardSearchRequest {
    ImageView image;
    std::vector<SampleSearch> samples;
    SearchParams search;
    GenerateOptions options;
    std::vector<RectObj> occupied;  // 手動補的元件的主體框：這些位置從結果裡剔除
};

Result<BoardProposal> searchBoardWithSamples(const GenerateObjectsFromSample& useCase, const BoardSearchRequest& request,
                                             const CancelToken& cancel, IProgressSink* progress = nullptr);

}  // namespace aoi
