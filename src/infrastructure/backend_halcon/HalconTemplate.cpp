#include "infrastructure/backend_halcon/HalconTemplate.h"

#include <utility>

namespace aoi::infra {

HalconTemplate::HalconTemplate(Size2D size, double angleMinDeg, double angleMaxDeg, HalconCpp::HShapeModel model, int numLevels,
                               std::optional<NoMatchReason> problem, HalconTemplateTuning tuning)
    : size_(size),
      angleMinDeg_(angleMinDeg),
      angleMaxDeg_(angleMaxDeg),
      model_(std::move(model)),
      numLevels_(numLevels),
      problem_(problem),
      tuning_(std::move(tuning)) {}

}  // namespace aoi::infra
