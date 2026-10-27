# 效能量測（-Perf，R05.4）：重複性的樣本統計（repeatability 的 summarize）的微基準，ADR-031 的 before／after。資訊性，沒有預算。

TARGET = perf_repeatability_stats
include(domain_tests.pri)
SOURCES += $$AOI_ROOT/tests/domain/perf_repeatability_stats.cpp
