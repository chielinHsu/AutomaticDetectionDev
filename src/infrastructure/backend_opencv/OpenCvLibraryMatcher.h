#pragma once

#include <vector>

#include "application/ports/ILibraryMatcher.h"
#include "infrastructure/backend_opencv/OpenCvPatternSearcher.h"
#include "infrastructure/backend_opencv/OpenCvSearchOptions.h"
#include "infrastructure/backend_opencv/OpenCvTemplateBuilder.h"

// infrastructure/backend_opencv：ILibraryMatcher 的 OpenCV 版（S07，流程 B 的區域掃描）。契約見 ILibraryMatcher.h。
//
// 每個候選類型：用它的參考影像整張當樣本建範本（OpenCvTemplateBuilder），在區域裡找（OpenCvPatternSearcher，
// 區域當成一個視窗）。所有類型的匹配依分數由高到低合併：和已有的元件重疊（交集 ÷ 較小的框 > 0.5）就是同一個元件，
// 記下這個類型的分數；否則是新的元件。元件的位姿是分數最高的那個類型的匹配。
//  - 比例：參考影像與板子影像都有校正值（µm／像素）而且差超過 1% 時，參考影像先縮放成板子的比例；有一邊沒有校正值就不縮放。
//  - 預篩選（只有這一種，成本 0）：參考影像比區域還大的類型不可能在區域裡，不比對、不算進 samplesCompared。
//    ADR-006 §7 的「依區域裡零件的外形預篩選」需要先偵測零件，S07 沒有做（docs/steps/S07-opencv/spec.md）。
//  - 每次呼叫都重建範本：成本隨候選數 N 線性成長（T7，perf.md）。在樣本庫載入時快取範本（ADR-006 §7.3）是之後的優化。
//  - 沒有參考影像的類型略過（不算比對過）。沒有比對任何樣本 → noMatch ＝ LibraryEmpty；比對了但沒有元件 → NoLibraryMatch
//    （區域裡到底有沒有零件，這個比對器看不出來，所以不回 RegionEmpty）。
// 無狀態，可以被多個 worker 同時呼叫。不丟例外。
namespace aoi::infra {

class OpenCvLibraryMatcher final : public ILibraryMatcher {
public:
    explicit OpenCvLibraryMatcher(OpenCvSearchOptions options = {}) : builder_(options), searcher_(options) {}

    Result<LibraryMatchResult> match(const ImageView& image, const Rect& region, const std::vector<ComponentType>& candidates,
                                     const SearchParams& params, const CancelToken& cancel, IProgressSink* progress) override;

private:
    OpenCvTemplateBuilder builder_;
    OpenCvPatternSearcher searcher_;
};

}  // namespace aoi::infra
