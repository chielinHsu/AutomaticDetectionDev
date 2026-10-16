#include "infrastructure/backend_halcon/HalconTemplate.h"

#include <cmath>
#include <utility>

namespace aoi::infra {

HalconTemplate::HalconTemplate(Size2D size, double angleMinDeg, double angleMaxDeg, HalconCpp::HShapeModel model, int numLevels,
                               std::optional<NoMatchReason> problem, HalconTemplateTuning tuning, HalconMatchGeometry geometry)
    : size_(size),
      angleMinDeg_(angleMinDeg),
      angleMaxDeg_(angleMaxDeg),
      model_(std::move(model)),
      numLevels_(numLevels),
      problem_(problem),
      tuning_(std::move(tuning)),
      geometry_(geometry) {
    if (!(geometry_.matchSize.w > 0.0) || !(geometry_.matchSize.h > 0.0)) geometry_.matchSize = size_;
    if (!(geometry_.referenceReach > 0.0)) geometry_.referenceReach = 0.5 * std::hypot(size_.w, size_.h);
}

}  // namespace aoi::infra
