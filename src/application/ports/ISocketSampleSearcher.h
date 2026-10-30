#pragma once

#include <vector>
#include "application/CancelToken.h"
#include "application/Result.h"
#include "domain/image_view.h"
#include "domain/geometry.h"

namespace aoi {
struct SocketSampleRequest {
    ImageView image;
    std::vector<Rect> samples;
    double minScore = 0.90;
};
struct SocketSampleMatch {
    Rect rect;  // 原圖座標；全數使用第一個樣本的寬高。
    double score = 0.0;  // 比對相似度，不是缺陷合格率。
};
class ISocketSampleSearcher {
public:
    virtual ~ISocketSampleSearcher() = default;
    virtual Result<std::vector<SocketSampleMatch>> search(const SocketSampleRequest& request,
                                                         const CancelToken& cancel) const = 0;
};
}  // namespace aoi
