# 真實影像：同 perf_opencv_real（AOI_REAL_IMAGE_DIR），同一張影像兩個 back end 並排。讀 TIFF 要 OpenCV 的影像來源，
# 所以 AOI_WITH_OPENCV 與 AOI_WITH_HALCON 都開才有（tests/tests.pro）。

TARGET = perf_halcon_real
AOI_LINK_EXTRA = backend_halcon backend_opencv
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/perf_halcon_real.cpp
