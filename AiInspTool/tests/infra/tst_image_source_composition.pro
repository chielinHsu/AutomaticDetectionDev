# 組裝根的 makeImageSource()：OpenCV 開與關兩種建置都要對（TIFF 只有 OpenCV 讀得了）。

TARGET = tst_image_source_composition
AOI_LINK_EXTRA = app_core
include(infra_tests.pri)
SOURCES += $$AOI_ROOT/tests/infra/tst_image_source_composition.cpp
