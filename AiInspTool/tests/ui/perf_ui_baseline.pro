# 效能基準線（-Perf）：只在 Release 有意義；Debug 的數字沒有意義（沒最佳化），所以只有 Release、Profile 才把預算當成失敗條件。

TARGET = perf_ui_baseline
include(ui_tests.pri)
SOURCES += $$AOI_ROOT/tests/ui/perf_ui_baseline.cpp
