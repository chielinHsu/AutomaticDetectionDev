#pragma once

#include <optional>

#include "domain/geometry.h"
#include "domain/image_view.h"
#include "domain/match_region.h"

// domain 層：只用標準庫。比對區的自動擷取（ADR-028）：使用者點一下特徵 → Otsu 二值化 → 離點擊最近的亮連通塊 → 外擴。
//
// 放在 domain（不是 adapter 後面的 port）：遮罩是存下來的教學資料，不能因為切換 back end 而不同；範圍只有主體框，O(n)。
namespace aoi {

struct BlobPick {
    PixelMask blob;      // 連通塊（影像像素，外接矩形）
    int threshold8 = 0;  // Otsu 的門檻（亮度 0..255；亮度 > 門檻算前景）
};

// within 與影像的交集裡做 Otsu（256 階，亮度依有效位元數換到 0..255）；點擊落在前景就取它所在的 8 連通塊，
// 否則取離點擊最近的前景像素所在的塊。沒有前景（整塊一樣亮）或 within 在影像外時 nullopt。不讀 within 以外的像素。
std::optional<BlobPick> pickBrightBlob(const ImageView& image, const Rect& within, Point2D click);

// 把遮罩往外擴 radiusPx 個像素（圓盤），裁到 clip（影像像素；外接整數像素）內。radiusPx ≤ 0 ＝ 只裁切。
PixelMask growMask(const PixelMask& blob, int radiusPx, const Rect& clip);

}  // namespace aoi
