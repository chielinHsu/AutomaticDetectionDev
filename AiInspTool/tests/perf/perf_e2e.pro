# 效能量測（-Perf，S10）：T5 端到端（放開滑鼠 → 提案就緒），走產品的接線（視窗、Presenter、真的 back end）。

TARGET = perf_e2e
AOI_LINK_EXTRA = presentation
include(perf_tests.pri)
HEADERS += $$AOI_ROOT/tests/ui/TestSupport.h
SOURCES += $$AOI_ROOT/tests/perf/perf_e2e.cpp
