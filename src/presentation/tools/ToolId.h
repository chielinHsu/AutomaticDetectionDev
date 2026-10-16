#pragma once

namespace aoi::ui {

// 左側工具列：V 選取／移動（含改大小、框選）、S 建立樣本、R 區域掃描、A 補一個元件、B 在所選元件上畫子框（docs/ux/ux-spec.md 第 4 節；ADR-025）。
enum class ToolId { Select, Sample, Region, Add, Box, Match };

}  // namespace aoi::ui
