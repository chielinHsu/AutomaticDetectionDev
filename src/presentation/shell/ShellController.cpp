#include "presentation/shell/ShellController.h"

#include <algorithm>

#include "presentation/i18n/Translations.h"

namespace aoi::ui {

ShellController::ShellController(ICanvasView& view, MessageBar& messages, const ThemeTokens& tokens)
    : view_(view), messages_(messages), tokens_(tokens) {}

void ShellController::onRegionDrawn(const QRectF& rect, DraftMode mode) {
    // 「太小」由這一層判斷（S04 起是 Presenter 的 ReleaseTooSmall）；工具不知道這條規則。
    const double minSide = tokens_.interaction.minSampleSidePx;
    if (std::min(rect.width(), rect.height()) < minSide) {
        view_.showRegionDraft(std::nullopt, mode);
        messages_.showToast(uiText("ui.sample.tooSmall", {{"min", QString::number(minSide)}}));
        return;
    }
    messages_.showToast(uiText("ui.shell.regionDrawn", {{"w", QString::number(qRound(rect.width()))},
                                                         {"h", QString::number(qRound(rect.height()))}}));
}

void ShellController::onRegionAborted() { view_.showRegionDraft(std::nullopt, DraftMode::Sample); }

void ShellController::onAddRequested(const QPointF& p) {
    messages_.showToast(uiText("ui.shell.addRequested",
                               {{"x", QString::number(qRound(p.x()))}, {"y", QString::number(qRound(p.y()))}}));
}

void ShellController::onComponentsMoved(const std::vector<int>&, const QPointF&) {}

void ShellController::onOpenTypeEditor(int componentId, const QPointF&) {
    messages_.showToast(uiText("ui.shell.openEditor", {{"id", QString::number(componentId)}}));
}

void ShellController::select(const std::vector<int>& componentIds) { view_.setSelection(componentIds); }

void ShellController::deleteSelected() { messages_.showToast(uiText("ui.shell.deleteRequested")); }

}  // namespace aoi::ui
