# 真實影像的比對區驗收（ADR-026）：AOI_REAL_MATCH_IMAGE 指到影像檔才執行，否則跳過。讀 TIFF 要 OpenCV；HALCON 有編進來而且有授權時一起量。

TARGET = perf_match_region_real
AOI_LINK_EXTRA = backend_opencv
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/perf_match_region_real.cpp
