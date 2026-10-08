# QImage ⇄ ImageView、QtImageSource。連 presentation 是為了比對畫布的 ImageItem::wrap 與 adapter 的格式對照表。

TARGET = tst_image_qt
AOI_LINK_EXTRA = presentation
include(infra_tests.pri)
SOURCES += $$AOI_ROOT/tests/infra/tst_image_qt.cpp
