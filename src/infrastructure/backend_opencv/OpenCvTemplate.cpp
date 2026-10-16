#include "infrastructure/backend_opencv/OpenCvTemplate.h"

#include <cmath>
#include <utility>

namespace aoi::infra {

OpenCvTemplate::OpenCvTemplate(Size2D size, double angleMinDeg, double angleMaxDeg, std::vector<OpenCvTemplateLevel> levels,
                               std::optional<NoMatchReason> problem, OpenCvMatchGeometry geometry)
    : size_(size),
      angleMinDeg_(angleMinDeg),
      angleMaxDeg_(angleMaxDeg),
      fullCircle_(angleMaxDeg - angleMinDeg >= 360.0 - 1e-9),
      levels_(std::move(levels)),
      problem_(problem),
      geometry_(geometry) {
    if (!(geometry_.matchSize.w > 0.0) || !(geometry_.matchSize.h > 0.0)) geometry_.matchSize = size_;
}

std::size_t OpenCvTemplate::bytes() const noexcept {
    std::size_t total = 0;
    for (const OpenCvTemplateLevel& level : levels_) {
        for (const cv::Mat& m : level.rotated) total += m.total() * m.elemSize();
        for (const cv::Mat& m : level.masks) total += m.total() * m.elemSize();
        for (const cv::Mat& m : level.centered) total += m.total() * m.elemSize();
    }
    return total;
}

double OpenCvTemplate::angleDifference(double a, double b) const noexcept {
    double d = a - b;
    if (fullCircle_) {
        d = std::fmod(d, 360.0);
        if (d > 180.0) d -= 360.0;
        if (d <= -180.0) d += 360.0;
    }
    return d;
}

}  // namespace aoi::infra
