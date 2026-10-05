#pragma once

#include <QRectF>
#include <optional>
#include <vector>

#include "domain/image_view.h"
#include "presentation/canvas/ComponentSnapshot.h"
#include "presentation/canvas/OverlayTypes.h"
#include "presentation/tools/ToolId.h"

namespace aoi::ui {

// Presenter 看到的畫布（docs/ux/ux-spec.md 第 12.2 節）。5 個方法；Presenter 不認識 QWidget。
// 縮放、平移、適配視窗不在這裡：它們是畫布自己的檢視狀態，不經過 Presenter（I1 的幀時間不能多一層間接）。
class ICanvasView {
public:
    virtual ~ICanvasView() = default;

    virtual void showImage(const aoi::ImageView& image) = 0;
    virtual void showComponents(SnapshotPtr snapshot) = 0;
    // nullopt ＝ 清除。退化矩形（寬或高為 0）是合法的：按下滑鼠的瞬間要有回饋。
    virtual void showRegionDraft(std::optional<QRectF> rect, DraftMode mode) = 0;
    virtual void setSelection(const std::vector<int>& componentIds) = 0;
    virtual void setInteraction(ToolId tool) = 0;
};

}  // namespace aoi::ui
