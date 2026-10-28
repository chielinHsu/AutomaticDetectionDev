# R07：真實影像上 1／2／3 個樣本的搜尋與合併（AOI_REAL_IMAGE_DIR），每個可用的 back end 一列。讀 TIFF 要 OpenCV 的影像來源，
# 所以 AOI_WITH_OPENCV=ON 才有（tests/tests.pro）；HALCON 編進來時那一列也量，不能用時跳過。

TARGET = perf_multi_sample_real
AOI_LINK_EXTRA = backend_opencv
equals(AOI_WITH_HALCON, ON): AOI_LINK_EXTRA += backend_halcon
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/perf_multi_sample_real.cpp
