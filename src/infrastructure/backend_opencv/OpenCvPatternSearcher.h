#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "application/ports/IPatternSearcher.h"
#include "infrastructure/backend_opencv/OpenCvSearchOptions.h"
#include "infrastructure/backend_opencv/OpenCvTemplate.h"

// infrastructure/backend_opencv：IPatternSearcher 的 OpenCV 版（S07）。契約見 IPatternSearcher.h。
//
// 由粗到細（docs/steps/S07-opencv/diagram.md）：
//  1. 搜尋區域轉成 8 bit 灰階（Gray8 零複製；Gray16 依有效位元數換算、彩色轉灰階，各算一次複製）、建金字塔。
//     整張影像是一塊；視窗清單是每個視窗一塊（視窗外面加範本一半的邊，只轉換、只掃這些像素）。
//  2. 頂層：每個角度做一次 matchTemplate（TM_CCOEFF_NORMED，正規化的相關係數），取每個位置最好的角度，
//     分數 ≥ minScore − coarseScoreDrop 的局部最大值是候選；候選先做一次 NMS。
//  3. 往下每一層：每個候選在換算過來的位置 ± refineRadiusPx 像素、上一層角度 ± 一步之內，用那一層的範本再比一次。
//  4. 第 0 層：位置用拋物線內插到次像素（x、y 各自三點），角度也用相鄰兩個角度的分數內插。
//  5. 分數 < minScore、中心不在區域內的丟掉；domain 的 NMS（範本大小的旋轉矩形，IoU > nmsMaxOverlap）合併重複的；
//     依分數排序（同分依 y、再依 x）、最多 maxMatches 個。
//
// 座標：回傳的位姿是 ADR-010 的連續座標（像素中心在 +0.5），角度在螢幕上逆時針為正、相對於樣本。
// 已知限制（spec.md）：強度相關法對光照變化、反光、縮放敏感；範本有一部分在影像外的元件找不到（matchTemplate 只看完整的位置）；
// 角度範圍越大越慢（頂層的角度數與範圍成正比）。
//
// 執行緒：頂層的各個角度、精修的各個候選、視窗清單的各個視窗用 cv::parallel_for_ 平行（OpenCV 自己的執行緒池；
// 執行緒數由組裝根用 cv::setNumThreads 設定，見 OpenCvBackend.h）。巢狀的 parallel_for_ 會在呼叫的執行緒上依序執行。
// 無狀態，可以被多個 worker 同時呼叫。不丟例外。
namespace aoi::infra {

// 一次搜尋的內部數字（量測用，docs/steps/S07-opencv/perf.md）。時間是各階段的牆鐘時間；視窗清單是各個視窗的總和（平行時會大於總時間）。
struct OpenCvSearchStats {
    double convertMs = 0.0;   // 轉成 8 bit 灰階
    double pyramidMs = 0.0;   // 建金字塔
    double coarseMs = 0.0;    // 頂層的 matchTemplate 與找局部最大值
    double refineMs = 0.0;    // 逐層精修
    double nmsMs = 0.0;       // 最後的 NMS 與排序
    double totalMs = 0.0;
    std::size_t areas = 0;            // 幾塊區域（整張影像是 1，視窗清單是視窗數）
    std::size_t coarseCandidates = 0; // 頂層的候選（NMS 之後）
    std::size_t matches = 0;          // 回傳的匹配數
    int topLevel = 0;                 // 從第幾層開始（0 ＝ 原尺寸）
    std::size_t topAngles = 0;        // 頂層比了幾個角度
};

class OpenCvPatternSearcher final : public IPatternSearcher {
public:
    explicit OpenCvPatternSearcher(OpenCvSearchOptions options = {}) : options_(options) {}

    std::string_view backendId() const noexcept override { return kOpenCvBackendId; }
    Result<std::vector<Match>> find(const ImageView& image, const ITemplate& tmpl, const SearchRegion& region,
                                    const SearchParams& params, const CancelToken& cancel) override;

    // 同上，另外填 stats（可以是空指標）。
    Result<std::vector<Match>> find(const ImageView& image, const ITemplate& tmpl, const SearchRegion& region,
                                    const SearchParams& params, const CancelToken& cancel, OpenCvSearchStats* stats) const;

    const OpenCvSearchOptions& options() const noexcept { return options_; }

private:
    OpenCvSearchOptions options_;
};

}  // namespace aoi::infra
