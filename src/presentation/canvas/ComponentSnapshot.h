#pragma once

#include <QRectF>
#include <QString>
#include <cstddef>
#include <memory>
#include <vector>

#include "presentation/canvas/OverlayTypes.h"

namespace aoi::ui {

// Presenter 交給畫布的「要畫什麼」。座標是影像像素（場景座標：原點在影像左上角，x 向右、y 向下）。
// 板上的選取單位是元件（含它的全部框），所以這裡以元件為單位；框是從元件類型推導出來的結果。

struct BoxView {
    QRectF rect;
    BoxKind kind = BoxKind::Body;
};

struct ComponentView {
    int id = 0;
    OverlayState state = OverlayState::Found;
    QRectF outline;  // 全部框的外接矩形；選取外框、點擊判定、標籤位置都用它
    QString label;   // 例如「8371 0.97」；空字串＝不顯示
    std::vector<BoxView> boxes;
};

struct ComponentsSnapshot {
    std::vector<ComponentView> components;

    std::size_t boxCount() const {
        std::size_t n = 0;
        for (const ComponentView& c : components) n += c.boxes.size();
        return n;
    }
};

using SnapshotPtr = std::shared_ptr<const ComponentsSnapshot>;

}  // namespace aoi::ui
