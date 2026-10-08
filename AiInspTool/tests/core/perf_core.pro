# 效能量測（-Perf）：PerfProbe 單次成本、JobRunner 延遲與取消延遲、use case 本身的成本。
# 只有 Release、Profile 才把「PerfProbe 關閉時 < 1 µs」當成失敗條件（AOI_PERF_ENFORCE，aoi_test.pri）。

TARGET = perf_core
include(core_tests.pri)
SOURCES += $$AOI_ROOT/tests/core/perf_core.cpp
