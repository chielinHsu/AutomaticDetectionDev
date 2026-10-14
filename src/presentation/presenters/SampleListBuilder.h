#pragma once

#include <cstdint>

#include "application/interaction/InspectionSession.h"
#include "domain/image_view.h"
#include "presentation/presenters/ViewInterfaces.h"

namespace aoi::ui {

// 板子上的樣本 → 樣本庫 dock 的清單（2026-10-14）：每個樣本從 image 裁一塊（主體框外加邊界）、縮到 thumbnailWidth、
// 疊上類型展開的框（主體與子框）。純函式：只做資料轉換，所以可以直接測。
// 裁圖用 ImageView 的 subView（零複製）再包成 QImage；12 bit 的影像在包的時候換到 8 bit（ImageItem::wrap），只算裁下來的那一小塊。
SampleListPtr buildSampleList(const InspectionSession& session, const ImageView& image, int thumbnailWidth = 96);

// Presenter 持有的快取：文件的修訂號、影像（像素指標與位元數）沒變就不重做。
struct SampleListCache {
    bool valid = false;
    std::uint64_t revision = 0;
    const void* imageData = nullptr;
    int significantBits = 0;
    SampleListPtr list;
};

// 命中就回快取的那一份（同一個指標，View 可以略過）；沒命中就重做並更新快取。
SampleListPtr sampleListFor(SampleListCache& cache, const InspectionSession& session, const ImageView& image,
                            int thumbnailWidth = 96);

}  // namespace aoi::ui
