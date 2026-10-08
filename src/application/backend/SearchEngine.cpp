#include "application/backend/SearchEngine.h"

#include <utility>

namespace aoi {

Result<SearchEnginePtr> makeSearchEngine(const BackendRegistry& registry, std::string_view id, IPerfProbe* probe) {
    Result<Backend> backend = registry.create(id);
    if (!backend) return backend.error();
    auto engine = std::make_shared<SearchEngine>();
    engine->backend = std::move(backend.value());
    engine->generate =
        std::make_unique<GenerateObjectsFromSample>(*engine->backend.templateBuilder, *engine->backend.searcher, probe);
    return SearchEnginePtr(std::move(engine));
}

}  // namespace aoi
