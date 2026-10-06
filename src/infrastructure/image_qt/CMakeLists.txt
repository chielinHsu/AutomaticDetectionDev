# Qt 的影像 adapter（S06）：QImage ⇄ ImageView 與 IImageSource 的 QImageReader 版。
# 用 Qt Core ＋ Qt Gui，不用 Qt Widgets、不用視覺函式庫（check-deps 的 infrastructure/image_qt 規則）。
add_library(aoi_image_qt STATIC QtImages.cpp QtImageSource.cpp)
target_link_libraries(aoi_image_qt PUBLIC aoi_application Qt6::Gui)
aoi_set_warnings(aoi_image_qt)
