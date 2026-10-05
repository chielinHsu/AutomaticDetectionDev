#pragma once

#include <QImage>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "domain/image_view.h"
#include "presentation/canvas/ComponentSnapshot.h"
#include "presentation/canvas/OverlayStyle.h"
#include "presentation/canvas/RoiGraphicsItem.h"

class QGraphicsScene;

// 合成的板子影像與框，供測試與效能量測用（S02）。不是產品功能：視覺函式庫、真實影像都不在這裡。
// 同樣的參數永遠產生同樣的結果（自己的亂數產生器，不依賴 <random> 的實作）。
namespace aoi::dev {

struct FakeBoard {
    std::shared_ptr<std::vector<std::uint8_t>> pixels;  // Bgrx32，由 view 的 owner 共享
    int width = 0;
    int height = 0;

    aoi::ImageView view() const;
    // 零複製：像素仍在 pixels 裡，QImage 只是包著它。
    QImage toQImage() const;
};

// 類似 PCB 的底圖：綠底、雜訊、細線路。width × height 預設是 5 MP（2592 × 1944）。
FakeBoard makeFakeBoard(int width = 2592, int height = 1944, unsigned seed = 1);

enum class FakeLayout {
    Chips,  // 每個元件只有一個主體框（電阻、電容）
    Ics,    // 每個元件 41 個框：1 個主體、2 × 10 支 Pin、每支 Pin 一個窗（和 S01 原型的 8371 一樣）
};

struct FakeSceneOptions {
    int width = 2592;
    int height = 1944;
    std::size_t boxes = 5000;  // 框的總數（上下 1 個元件以內）
    FakeLayout layout = FakeLayout::Ics;
    unsigned seed = 1;
};

// 在板子的格子上排出元件；狀態大約 90% Found、4% Rejected、3% Inferred，第一個是 Sample。
aoi::ui::SnapshotPtr makeFakeComponents(const FakeSceneOptions& options);

// 效能比較用的「逐一繪製」：每個元件一個 RoiGraphicsItem 直接放進場景（不經過 RoiLayerItem）。
// 回傳放進去的 Item，呼叫端用 removeItem 與 delete 清掉。
std::vector<aoi::ui::RoiGraphicsItem*> addPerComponentItems(QGraphicsScene& scene, aoi::ui::OverlayStyle& style,
                                                            const aoi::ui::ComponentsSnapshot& snapshot);

}  // namespace aoi::dev
