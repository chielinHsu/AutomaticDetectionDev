#pragma once

#include <halconcpp/HalconCpp.h>

#include <vector>

#include "domain/geometry.h"
#include "domain/image_view.h"
#include "domain/match.h"
#include "domain/search_params.h"
#include "infrastructure/backend_halcon/HalconImages.h"

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
// 區域：整張影像，或所有視窗（往外 grow）的聯集。
HalconSearchInput prepareHalconSearch(const ImageView& image, const std::vector<Rect>& accepts, bool windows, double halfExtent,
                                      int grow, bool nativeGray16);

// 每個視窗往外多幾個像素：HALCON 的說明建議 2^(層數−1)（區域太緊時高層可能找不到），至少 2（次像素的結果可能跨過像素邊界）。
int halconWindowGrow(int numLevels) noexcept;

// find_shape_model(s) 的結果（只取 indices 指到的那幾個；空 ＝ 全部）→ 契約的匹配：
// 換算位姿（HalconPose.h）、角度放回範圍、分數 < minScore 與中心不在任何 accept 的丟掉、
// 範本大小（size）的旋轉矩形做 NMS（IoU > nmsMaxOverlap）、依分數排序（同分依 y、再依 x）、最多 maxMatches 個。
std::vector<Match> halconMatches(const HalconCpp::HTuple& rows, const HalconCpp::HTuple& columns, const HalconCpp::HTuple& angles,
                                 const HalconCpp::HTuple& scores, const std::vector<Hlong>& indices, const HalconGray& gray,
                                 const std::vector<Rect>& accepts, const SearchParams& params, Size2D size, double nmsMaxOverlap);

}  // namespace aoi::infra
