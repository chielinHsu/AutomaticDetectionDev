#include "presentation/tools/ToolRegistry.h"

#include "presentation/tools/Tools.h"

namespace aoi::ui {

void ToolRegistry::registerTool(std::unique_ptr<ITool> tool) {
    const ToolId id = tool->id();
    tools_[id] = std::move(tool);
}

ITool* ToolRegistry::find(ToolId id) const {
    const auto it = tools_.find(id);
    return it == tools_.end() ? nullptr : it->second.get();
}

std::vector<ToolId> ToolRegistry::ids() const {
    std::vector<ToolId> out;
    out.reserve(tools_.size());
    for (const auto& entry : tools_) out.push_back(entry.first);
    return out;
}

ToolRegistry ToolRegistry::makeDefault() {
    ToolRegistry r;
    r.registerTool(std::make_unique<SelectTool>());
    r.registerTool(std::make_unique<SampleRoiTool>());
    r.registerTool(std::make_unique<RegionScanTool>());
    r.registerTool(std::make_unique<AddComponentTool>());
    r.registerTool(std::make_unique<BoxTool>());
    r.registerTool(std::make_unique<MatchRegionTool>());
    return r;
}

}  // namespace aoi::ui
