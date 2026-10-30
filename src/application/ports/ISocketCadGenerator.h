#pragma once

#include <memory>
#include <string>
#include <vector>
#include "application/CancelToken.h"
#include "application/Result.h"
#include "domain/geometry.h"
#include "domain/image_view.h"

namespace aoi {
enum class SocketPadSide { Left, Right };
struct SocketJointObservation {
    Point2D center;
    Size2D observedSize;
    SocketPadSide padSide = SocketPadSide::Left;
    double structuralQuality = 0;
};
struct SocketRegionObservation {
    std::vector<SocketJointObservation> joints;
    Size2D suggestedFrame;
    std::size_t evaluated = 0;
};
class ISocketRegionRecognizer {
public:
    virtual ~ISocketRegionRecognizer() = default;
    // 可使用周邊 context 辨識，但 observations 均為原圖座標；輸出區域由 generator 嚴格限制。
    virtual Result<SocketRegionObservation> recognize(const ImageView& image, const Rect& region,
                                                     const CancelToken& cancel) const = 0;
};
struct SocketCadRequest {
    ImageView image;
    Rect region;
    Size2D frameSize;  // 0,0 = 自動由尺度族群估計，正值 = 使用既有／指定 CAD 規格。
    bool snapPositions = true;
};
struct SocketCadProfile {
    std::string id;
    std::string name;
    Size2D bodyPx;
    double windowWidthRatio = 0.8;
    double windowHeightRatio = 0.6;
};
struct SocketCadPlacement {
    std::string profileId;
    Pose2D pose;
    double structuralQuality = 0;
    bool snapped = false;
};
struct SocketCadResult {
    Rect region;
    std::vector<SocketCadProfile> profiles;
    std::vector<SocketCadPlacement> placements;
    std::size_t evaluated = 0;
};
class ISocketCadGenerator {
public:
    virtual ~ISocketCadGenerator() = default;
    virtual Result<SocketCadResult> generate(const SocketCadRequest& request, const CancelToken& cancel) const = 0;
};
}  // namespace aoi
