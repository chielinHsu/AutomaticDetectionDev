# 效能量測（-Perf）：T3（1 000 點擬合 ≤ 2 ms）、NMS、漸進重新擬合。只有 Release、Profile 把 T3 當成失敗條件。

TARGET = perf_lattice
include(domain_tests.pri)
SOURCES += $$AOI_ROOT/tests/domain/perf_lattice.cpp
