# 效能量測（-Perf，S10）：搜尋矩陣。影像大小 × 樣本大小 × 角度範圍 × 元件數 × 難度 × back end，use case 端到端。
# AOI_PERF_PROFILE=full 跑整個外積（要 test.ps1 -TimeoutSec 3600）。

TARGET = perf_matrix
include(perf_tests.pri)
SOURCES += $$AOI_ROOT/tests/perf/perf_matrix.cpp
