# 效能量測（-Perf）：影像載入（5 MP／25 MP）、轉換的複製次數、配方與樣本庫的存讀。沒有預算，只記錄（perf.md）。
# OpenCV 開的時候也量 OpenCV 的影像來源與 TIFF。

TARGET = perf_storage
AOI_LINK_EXTRA = presentation_dev
AOI_LINK_IF_OPENCV = backend_opencv
include(infra_tests.pri)
SOURCES += $$AOI_ROOT/tests/infra/perf_storage.cpp
