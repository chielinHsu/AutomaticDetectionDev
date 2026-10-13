# 探測（fix/halcon-accuracy）：不列進 tests.pro，單獨 qmake 建置。
TARGET = probe_opencv_angle
AOI_LINK_EXTRA = backend_opencv
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/probe_opencv_angle.cpp
