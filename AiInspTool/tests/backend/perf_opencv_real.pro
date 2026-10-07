# 使用者的真實影像（不入版控）：AOI_REAL_IMAGE_DIR 指到影像的資料夾才執行，否則跳過。只在效能量測（-Perf）時跑。

TARGET = perf_opencv_real
AOI_LINK_EXTRA = backend_opencv
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/perf_opencv_real.cpp
