# 效能量測（-Perf，R05.3）：比對區與自動擷取（match_region、feature_extract）的微基準，ADR-031 的 before／after。資訊性，沒有預算。

TARGET = perf_match_region
include(domain_tests.pri)
SOURCES += $$AOI_ROOT/tests/domain/perf_match_region.cpp
