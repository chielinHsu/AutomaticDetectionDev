#pragma once

#include <QPointF>
#include <QRectF>
#include <optional>

#include "presentation/tools/ITool.h"

namespace aoi::ui {

// V：點擊選取元件（Shift＋點擊加入或移除；點空白處清除）；雙擊開啟元件類型編輯器。
// 拖曳移動元件要有 Presenter 與模型才有意義（S04），本步驟不做。
class SelectTool final : public ITool {
public:
    ToolId id() const override { return ToolId::Select; }
    Qt::CursorShape cursor() const override { return Qt::ArrowCursor; }
    void activate(IToolHost&) override {}
    void deactivate(IToolHost&) override {}
    bool handle(const ToolEvent& event, IToolHost& host) override;
};

// 橡皮筋框選的共用部分：按下 → 拖曳（即時顯示框）→ 放開時回報；拖曳中按 Esc 放棄。
// 不論框多小，放開都回報，由 Presenter 判斷「太小」；這樣工具不必知道最小尺寸規則。
class RubberBandTool : public ITool {
public:
    Qt::CursorShape cursor() const override { return Qt::CrossCursor; }
    void activate(IToolHost& host) override;
    void deactivate(IToolHost& host) override;
    bool handle(const ToolEvent& event, IToolHost& host) override;

    bool dragging() const { return anchor_.has_value(); }

protected:
    explicit RubberBandTool(DraftMode mode) : mode_(mode) {}

private:
    QRectF currentRect(const ToolEvent& event) const;
    void abort(IToolHost& host);

    DraftMode mode_;
    std::optional<QPointF> anchor_;  // 按下的點，已限制在影像內
    QRectF bounds_;
};

// S：教樣本，框一個元件。
class SampleRoiTool final : public RubberBandTool {
public:
    SampleRoiTool() : RubberBandTool(DraftMode::Sample) {}
    ToolId id() const override { return ToolId::Sample; }
};

// R：區域掃描，框任意區域。
class RegionScanTool final : public RubberBandTool {
public:
    RegionScanTool() : RubberBandTool(DraftMode::Region) {}
    ToolId id() const override { return ToolId::Region; }
};

// A：在影像上點一下，補一個元件。
class AddComponentTool final : public ITool {
public:
    ToolId id() const override { return ToolId::Add; }
    Qt::CursorShape cursor() const override { return Qt::CrossCursor; }
    void activate(IToolHost&) override {}
    void deactivate(IToolHost&) override {}
    bool handle(const ToolEvent& event, IToolHost& host) override;
};

}  // namespace aoi::ui
