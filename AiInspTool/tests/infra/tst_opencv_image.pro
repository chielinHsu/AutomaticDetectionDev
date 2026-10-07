# OpenCV 的影像 adapter。AOI_WITH_OPENCV=ON 才有（tests/tests.pro）。

TARGET = tst_opencv_image
AOI_LINK_EXTRA = backend_opencv
include(infra_tests.pri)
SOURCES += $$AOI_ROOT/tests/infra/tst_opencv_image.cpp
