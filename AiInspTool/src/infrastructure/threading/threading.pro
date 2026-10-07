# IExecutor 的標準庫版（std::thread）。QThreadPool 版在同一個資料夾的 threading_qt.pro。

AOI_MODULE = threading
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/infrastructure/threading
SOURCES += \
    $$DIR/ThreadPoolExecutor.cpp

HEADERS += \
    $$DIR/ThreadPoolExecutor.h
