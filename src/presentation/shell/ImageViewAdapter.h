#pragma once

#include <QImage>

#include "domain/image_view.h"

namespace aoi::ui {

// QImage → ImageView。S02 的畫布與載入器是 QImage（S06 的 IImageSource 取代），而 use case 看的是 ImageView，
// 所以 Presenter 在這裡轉。
//
// 零複製：ImageView 的 owner 持有一份 QImage（隱式共享，只加參照計數），像素活到最後一個 view 消失。
// 格式：Grayscale8 → Gray8、RGB32／ARGB32 → Bgrx32（記憶體順序 B,G,R,x）、RGB888 → Rgb24；其他格式先轉成 RGB32（複製一次）。
// 空影像回傳無效的 ImageView（valid() ＝ false）。
aoi::ImageView toImageView(const QImage& image, double umPerPixel = 0.0);

}  // namespace aoi::ui
