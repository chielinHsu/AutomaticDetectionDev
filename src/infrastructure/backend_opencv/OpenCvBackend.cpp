#include "infrastructure/backend_opencv/OpenCvBackend.h"

#include <memory>
#include <opencv2/core/utility.hpp>
#include <string>

#include "infrastructure/backend_opencv/OpenCvPatternSearcher.h"
#include "infrastructure/backend_opencv/OpenCvTemplateBuilder.h"

namespace aoi::infra {

Backend makeOpenCvBackend(const OpenCvSearchOptions& options) {
    Backend backend;
    backend.id = std::string(kOpenCvBackendId);
    backend.displayName = "OpenCV";
    backend.templateBuilder = std::make_unique<OpenCvTemplateBuilder>(options);
    backend.searcher = std::make_unique<OpenCvPatternSearcher>(options);
    return backend;
}

int setOpenCvThreads(int threads) {
    if (threads > 0) cv::setNumThreads(threads);
    return cv::getNumThreads();
}

}  // namespace aoi::infra
