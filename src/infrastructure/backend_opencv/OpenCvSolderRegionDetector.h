#pragma once
#include "application/ports/ISolderRegionDetector.h"

namespace aoi::infra {
class OpenCvSolderRegionDetector final : public ISolderRegionDetector {
public:
    Result<std::vector<Rect>> detect(const ImageView& image, const Rect& region,
                                    const SolderDetectionOptions& options, const CancelToken& cancel) const override;
};
}  // namespace aoi::infra
