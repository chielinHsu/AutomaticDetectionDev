# 組裝根的 OpenCV back end ＋ use case，在合成板子上和標準答案比。

TARGET = tst_opencv_end_to_end
AOI_LINK_EXTRA = backend_opencv
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/tst_opencv_end_to_end.cpp
