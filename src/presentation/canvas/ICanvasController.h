#pragma once

#include <QPointF>
#include <QRectF>
#include <Qt>
#include <vector>

#include "presentation/canvas/OverlayTypes.h"

namespace aoi::ui {

// 畫布上的指標操作（docs/ux/ux-spec.md 第 12.3 節）；由 Presenter 實作（S04）。
// 畫布與工具只透過這兩個介面對外說話，不知道 use case 的存在（SOLID 的 D）。
class ICanvasController {
public:
    virtual ~ICanvasController() = default;

    // 放開滑鼠：不論多小都回報，由 Presenter 判斷「太小」（ReleaseTooSmall）。座標是影像像素。
    virtual void onRegionDrawn(const QRectF& rect, DraftMode mode) = 0;
    virtual void onRegionAborted() = 0;  // 拖曳中按 Esc
    virtual void onAddRequested(const QPointF& imagePoint) = 0;
    virtual void onComponentsMoved(const std::vector<int>& componentIds, const QPointF& delta) = 0;
    virtual void onOpenTypeEditor(int componentId, const QPointF& imagePoint) = 0;
};

// 選取與刪除（ux-spec 的 ISelectionController）。setFilter 等 S04 有了篩選型別再加。
class ISelectionController {
public:
    virtual ~ISelectionController() = default;

    virtual void select(const std::vector<int>& componentIds) = 0;
    virtual void deleteSelected() = 0;
};

// 工具沒有用到的按鍵（ux-spec 第 8 節）：Enter 接受、Esc 退回一層、Del 刪除所選、方向鍵微調、E 開類型編輯器。
// 回傳 true ＝ 用掉了（畫布不再處理）。Presenter 實作；畫布只管轉交，不知道每個鍵的意義。
class IKeyboardController {
public:
    virtual ~IKeyboardController() = default;

    virtual bool onKeyPressed(int key, Qt::KeyboardModifiers modifiers) = 0;
};

}  // namespace aoi::ui
