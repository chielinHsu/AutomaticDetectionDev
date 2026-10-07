# OpenCV 的樣本庫比對器（流程 B）。

TARGET = tst_opencv_library_matcher
AOI_LINK_EXTRA = backend_opencv
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/tst_opencv_library_matcher.cpp
