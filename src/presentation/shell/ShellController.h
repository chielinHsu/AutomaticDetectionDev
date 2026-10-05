#pragma once

#include "presentation/canvas/ICanvasController.h"
#include "presentation/canvas/ICanvasView.h"
#include "presentation/shell/MessageBar.h"
#include "presentation/theme/ThemeTokens.h"

namespace aoi::ui {

// S02 的暫代 controller：畫布的每個操作都會走到這裡，讓介面可以實際操作，但不接任何 use case。
// 它只做兩件事：把選取回送給畫布，以及在訊息列說「這個動作收到了」。
// S04 的 Presenter 取代它（實作同樣的介面），畫布與工具不必改。
class ShellController final : public ICanvasController, public ISelectionController {
public:
    ShellController(ICanvasView& view, MessageBar& messages, const ThemeTokens& tokens);

    void onRegionDrawn(const QRectF& rect, DraftMode mode) override;
    void onRegionAborted() override;
    void onAddRequested(const QPointF& imagePoint) override;
    void onComponentsMoved(const std::vector<int>& componentIds, const QPointF& delta) override;
    void onOpenTypeEditor(int componentId, const QPointF& imagePoint) override;

    void select(const std::vector<int>& componentIds) override;
    void deleteSelected() override;

private:
    ICanvasView& view_;
    MessageBar& messages_;
    const ThemeTokens& tokens_;
};

}  // namespace aoi::ui
