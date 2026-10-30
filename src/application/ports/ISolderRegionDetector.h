#pragma once

#include <vector>
#include "application/CancelToken.h"
#include "application/Result.h"
#include "domain/geometry.h"
#include "domain/image_view.h"

namespace aoi {

// 可調亮塊定位，回傳原圖像素的軸對齊框；不判定焊接品質，也不回傳模型信心。
struct SolderDetectionOptions {
    int threshold = 0;  // 0 = Otsu，1..255 = 8-bit 正規化亮度
    bool darkForeground = false;
    int minArea = 40;
    int maxArea = 3000;
    int padding = 2;
    int maxBoxes = 5000;
};

class ISolderRegionDetector {
public:
    virtual ~ISolderRegionDetector() = default;
    // 可重入、不丟例外；不修改 image；取消回 Cancelled；候選過多回錯誤，不能靜默截斷。
    virtual Result<std::vector<Rect>> detect(const ImageView& image, const Rect& region,
                                            const SolderDetectionOptions& options, const CancelToken& cancel) const = 0;
};

}  // namespace aoi
