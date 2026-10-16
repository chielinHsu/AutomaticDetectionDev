#pragma once

#include <halconcpp/HalconCpp.h>

#include <string>
#include <vector>

#include "domain/geometry.h"
#include "domain/image_view.h"
#include "domain/match.h"
#include "domain/search_params.h"
#include "infrastructure/backend_halcon/HalconImages.h"
#include "infrastructure/backend_halcon/HalconPose.h"
#include "infrastructure/backend_halcon/HalconSearchOptions.h"

// infrastructure/backend_halcon：搜尋器與樣本庫比對器共用的步驟（S08）。只在這個 adapter 裡用。
namespace aoi::infra {

// 搜尋的輸入：轉換好的影像與搜尋區域。searchImage 從 gray 衍生（同一塊像素），所以兩個一起保存、一起解構。
struct HalconSearchInput {
    HalconGray gray;
    HalconCpp::HImage searchImage;
};

// accepts：匹配中心要落在的矩形（整張影像，或裁到影像內的視窗）。windows ＝ false 時 accepts 只有整張影像。
// halfExtent：範本的半對角線（最大的那個，流程 B 有幾個模型）；grow：每個視窗往外多幾個像素。
// 影像：Gray8（或 nativeGray16 的 Gray16）緊密排列時整張零複製；否則視窗清單只轉換「視窗的外接矩形 ＋ 範本一半」。
// smoothRadius > 0（範本的 HalconTemplateTuning）：轉換好的影像再做 mean_image（2r+1）smoothPasses 次（HALCON 自己配置新影像；
// 視窗清單時外接矩形多留濾波器的範圍，邊界碰不到搜尋區域）。
// 區域：整張影像，或所有視窗（往外 grow）的聯集。
HalconSearchInput prepareHalconSearch(const ImageView& image, const std::vector<Rect>& accepts, bool windows, double halfExtent,
                                      int grow, bool nativeGray16, int smoothRadius = 0, int smoothPasses = 3);

// mean_image（2r+1 × 2r+1）做 passes 次（3 次方框模糊 ≈ 高斯）；radius ≤ 0 時原樣回傳。
HalconCpp::HImage smoothHalconImage(const HalconCpp::HImage& image, int radius, int passes);

// 每個視窗往外多幾個像素：HALCON 的說明建議 2^(層數−1)（區域太緊時高層可能找不到），至少 2（次像素的結果可能跨過像素邊界）。
int halconWindowGrow(int numLevels) noexcept;

// 樣本的兩個統計量（rect：HALCON 的 row1、column1、row2、column2，含；在 rect 往外一個像素的範圍算，裁到影像內）：
//   edgeWidthPx：灰階的 p95 − p5 ÷ 3×3 Sobel 梯度大小（÷ 8）的 p99 ＝ 邊有幾個像素寬（合成板子上，halcon-accuracy-probe.md §7：
//     銳利 ≈ 2.0、模糊半徑 1 ≈ 3.9–4.3、2 ≈ 5.3–5.7、4 ≈ 7.9）；
//   noiseRatio：像素和它 3×3 平均的差的絕對值的中位數 × 1.4826 ÷（p95 − p5）＝ 雜訊 σ 佔灰階範圍的比例（邊只佔樣本一小部分，中位數不受影響）。
// 'byte' 與 'uint2' 都可以（兩個比值都和灰階的倍數無關）。影像沒有初始化、矩形空、或整塊是平的時全部回傳 0。
struct HalconSampleStatistics {
    double edgeWidthPx = 0.0;
    double noiseRatio = 0.0;
};
HalconSampleStatistics estimateSampleStatistics(const HalconCpp::HImage& gray, const HalconRect1& rect);

// 依樣本的統計量與選項決定 mean_image 的半徑、SubPixel 與最低追蹤的層（HalconSearchOptions.h 的「預先平滑」）。
// adaptiveSmoothing 關掉時不平滑、SubPixel 照選項、追蹤到底。
struct HalconSmoothing {
    int smoothRadius = 0;
    std::string subPixel;
    int lowestLevel = 1;  // find_shape_model 最低追蹤到哪一層（1 ＝ 到底）
};
HalconSmoothing chooseHalconSmoothing(const HalconSampleStatistics& stats, const HalconSearchOptions& options);

// find_shape_model(s) 的結果（只取 indices 指到的那幾個；空 ＝ 全部）→ 契約的匹配：
// 換算位姿（HalconPose.h）、角度放回範圍、分數 < minScore 與中心不在任何 accept 的丟掉、
// 範本大小（size）的旋轉矩形做 NMS（IoU > nmsMaxOverlap）、依分數排序（同分依 y、再依 x）、最多 maxMatches 個。
// 比對區（ADR-028）：originOffset 不是 0 時，參考點加上 rotate(originOffset, 角度) ＝ 主體中心，accept 看主體中心；
// NMS 的框（size ＝ 比對區）放在參考點。整框時 originOffset 是 0，和之前相同。
std::vector<Match> halconMatches(const HalconCpp::HTuple& rows, const HalconCpp::HTuple& columns, const HalconCpp::HTuple& angles,
                                 const HalconCpp::HTuple& scores, const std::vector<Hlong>& indices, const HalconGray& gray,
                                 const std::vector<Rect>& accepts, const SearchParams& params, Size2D size, double nmsMaxOverlap,
                                 Point2D originOffset = {});

// 比對區（ADR-028）：主體中心要落在 accepts 裡 → 參考點（＝ 主體中心 − rotate(originOffset, θ)）要落在哪裡。
// θ 在 [angleMin, angleMax]：半範圍 < 90° 時平移 −rotate(o, 中間角度)，再往外擴 |o|·2·sin(半範圍／2) ＋ 1；否則不平移、擴 |o|。
std::vector<Rect> referenceWindows(const std::vector<Rect>& accepts, Point2D originOffset, double angleMinDeg, double angleMaxDeg);

}  // namespace aoi::infra
