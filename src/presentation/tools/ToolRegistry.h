#pragma once

#include <map>
#include <memory>
#include <vector>

#include "presentation/tools/ITool.h"

namespace aoi::ui {

class ToolRegistry {
public:
    // 同一個 id 註冊第二次會取代前一個。
    void registerTool(std::unique_ptr<ITool> tool);
    ITool* find(ToolId id) const;
    std::vector<ToolId> ids() const;

    // 內建的四個工具：V 選取、S 教樣本、R 區域掃描、A 補一個。
    static ToolRegistry makeDefault();

private:
    std::map<ToolId, std::unique_ptr<ITool>> tools_;
};

}  // namespace aoi::ui
