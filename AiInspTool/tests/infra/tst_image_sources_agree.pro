# 「兩個來源給出相同的影像」（IImageSource 的 Liskov 契約）。AOI_WITH_OPENCV=ON 才有。

TARGET = tst_image_sources_agree
AOI_LINK_EXTRA = backend_opencv
include(infra_tests.pri)
SOURCES += $$AOI_ROOT/tests/infra/tst_image_sources_agree.cpp
