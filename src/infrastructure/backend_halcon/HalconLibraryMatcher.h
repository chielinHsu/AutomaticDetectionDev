#pragma once

#include <vector>

#include "application/ports/ILibraryMatcher.h"
#include "infrastructure/backend_halcon/HalconPatternSearcher.h"
#include "infrastructure/backend_halcon/HalconSearchOptions.h"
#include "infrastructure/backend_halcon/HalconTemplateBuilder.h"

// infrastructure/backend_halcon：ILibraryMatcher 的 HALCON 版（S08，流程 B 的區域掃描）。契約見 ILibraryMatcher.h。
//
// 每個候選類型：用它的參考影像整張當樣本建一個 shape model（HalconTemplateBuilder）。然後：
//  - libraryOneCall（預設）：所有模型一次 find_shape_models（ADR-006 預期的做法，S08 驗證）。MaxOverlap 給「每個模型一個值」：
//    說明：這樣只有同一個模型的結果互相抑制，不同模型的結果可以完全重疊 —— 同一個元件要拿到每個類型的分數，就要這樣。
//  - 否則：每個類型一次 find_shape_model（和 OpenCV 版的流程一樣）。兩種做法的結果與時間在 perf.md（T7）。
// 結果合併成元件的規則和 OpenCV 版完全一樣（兩個 adapter 不能共用程式碼，所以抄一份；S09 的契約測試鎖住兩邊一致）：
// 分數由高到低，和已有的元件重疊（交集 ÷ 較小的框 > 0.5）就是同一個元件，記下這個類型的分數；否則是新的元件。
//  - 比例：參考影像與板子影像都有校正值、而且差超過 1% 時，參考影像先縮放成板子的比例（zoom_image_factor，'constant'）。
//  - 預篩選：參考影像比區域還大的類型不比對、不算進 samplesCompared。沒有參考影像的類型略過。
//  - 沒有比對任何樣本 → noMatch ＝ LibraryEmpty；比對了但沒有元件 → NoLibraryMatch。
//  - progress：每個候選類型回報一次（Stage::Match）。一次搜尋的模式，最後一個類型的回報在搜尋完成之後（fraction ＝ 1）。
// 無狀態，可以被多個 worker 同時呼叫。不丟例外。
namespace aoi::infra {

class HalconLibraryMatcher final : public ILibraryMatcher {
public:
    explicit HalconLibraryMatcher(HalconSearchOptions options = {}) : options_(options), builder_(options), searcher_(options) {}

    Result<LibraryMatchResult> match(const ImageView& image, const Rect& region, const std::vector<ComponentType>& candidates,
                                     const SearchParams& params, const CancelToken& cancel, IProgressSink* progress) override;

private:
    HalconSearchOptions options_;
    HalconTemplateBuilder builder_;
    HalconPatternSearcher searcher_;
};

}  // namespace aoi::infra
