# Qt 的影像 adapter（S06）：QImage ⇄ ImageView 與 IImageSource 的 QImageReader 版。
# 用 Qt Core ＋ Qt Gui，不用 Qt Widgets、不用視覺函式庫（check-deps 的 infrastructure/image_qt 規則）。

AOI_MODULE = image_qt
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/infrastructure/image_qt
SOURCES += \
    $$DIR/QtImageSource.cpp \
    $$DIR/QtImages.cpp

HEADERS += \
    $$DIR/QtImageSource.h \
    $$DIR/QtImages.h
