#pragma once

#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

#include "application/ports/IPatternSearcher.h"
#include "infrastructure/backend_halcon/HalconSearchOptions.h"
#include "infrastructure/backend_halcon/HalconTemplate.h"

// infrastructure/backend_halcon：IPatternSearcher 的 HALCON 版（S08）。契約見 IPatternSearcher.h。
//
// 一次 find_shape_model（docs/steps/S08-halcon/diagram.md）：
//  1. 影像轉成 HALCON 的影像（HalconImages.h：Gray8 緊密排列時零複製）。視窗清單只轉換「所有視窗外加範本一半」的外接矩形。
//  2. 搜尋區域 ＝ 影像的區域（domain）：整張影像，或所有視窗的聯集。HALCON 在區域裡找「參考點」（樣本框的中心）。
//     每個視窗往外多 2^(層數−1) 個像素（說明：區域太緊時，高層的金字塔可能找不到）；之後再用視窗本身過濾。
//  3. find_shape_model：角度範圍來自 SearchParams（度 → 弧度），分數下限 ＝ minScore，NumMatches ＝ 0（全部），
//     次像素、greediness、HALCON 自己的重疊抑制照 HalconSearchOptions。
//  4. 結果換成 ADR-010 的位姿（HalconPose.h：考慮旋轉的 ±0.5 修正；角度只換單位）。
//  5. 分數 < minScore、中心不在區域內的丟掉；domain 的 NMS（範本大小的旋轉矩形，IoU > nmsMaxOverlap）合併重複的；
//     依分數排序（同分依 y、再依 x）、最多 maxMatches 個。和 OpenCV 版同一套規則（契約）。
//
// 已知限制（spec.md）：範本有一部分在影像外的元件找不到（說明：模型要整個在影像內，除非開 border_shape_models）；
// 分數是「梯度方向的相似度」，和 OpenCV 的 NCC 不是同一把尺：同一個 minScore 在兩個 back end 的意義不同（S09 比較）。
//
// 執行緒：HALCON 自己平行化運算子（執行緒數由組裝根用 set_system('thread_num') 設定，見 HalconBackend.h）。
// 取消：find_shape_model 包在 HalconCancelWatch 裡（interrupt_operator）。無狀態，可以被多個 worker 同時呼叫。不丟例外。
namespace aoi::infra {

// 一次搜尋的內部數字（量測用，docs/steps/S08-halcon/perf.md）。
struct HalconSearchStats {
    double convertMs = 0.0;  // 影像轉換與搜尋區域
    double findMs = 0.0;     // find_shape_model
    double nmsMs = 0.0;      // 換算、過濾、NMS、排序
    double totalMs = 0.0;
    std::size_t areas = 0;       // 整張影像是 1，視窗清單是用到的視窗數
    std::size_t rawMatches = 0;  // find_shape_model 回傳的個數
    std::size_t matches = 0;     // 最後回傳的個數
    int numLevels = 0;           // 模型的金字塔層數
    bool copied = false;         // 影像有沒有複製
};

class HalconPatternSearcher final : public IPatternSearcher {
public:
    explicit HalconPatternSearcher(HalconSearchOptions options = {}) : options_(std::move(options)) {}

    std::string_view backendId() const noexcept override { return kHalconBackendId; }
    Result<std::vector<Match>> find(const ImageView& image, const ITemplate& tmpl, const SearchRegion& region,
                                    const SearchParams& params, const CancelToken& cancel) override;

    // 同上，另外填 stats（可以是空指標）。
    Result<std::vector<Match>> find(const ImageView& image, const ITemplate& tmpl, const SearchRegion& region,
                                    const SearchParams& params, const CancelToken& cancel, HalconSearchStats* stats) const;

    const HalconSearchOptions& options() const noexcept { return options_; }

private:
    HalconSearchOptions options_;
};

}  // namespace aoi::infra
