# OpenCV 的範本建立器與搜尋器的契約（S09 契約測試的草稿）。AOI_WITH_OPENCV=ON 才有（tests/tests.pro）。

TARGET = tst_opencv_backend
AOI_LINK_EXTRA = backend_opencv
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/tst_opencv_backend.cpp
