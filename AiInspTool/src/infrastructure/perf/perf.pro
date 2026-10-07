# PerfProbe：IPerfProbe 的實作（環狀緩衝、JSON 輸出）。只用標準庫。

AOI_MODULE = perf
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/infrastructure/perf
SOURCES += \
    $$DIR/PerfProbe.cpp

HEADERS += \
    $$DIR/PerfProbe.h
