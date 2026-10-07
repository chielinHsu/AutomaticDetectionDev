# IExecutor 的 QThreadPool 版（S04）。只用 Qt Core；另外一個程式庫，所以只要 std::thread 版的使用者不必連 Qt。

AOI_MODULE = threading_qt
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/infrastructure/threading
SOURCES += \
    $$DIR/QtThreadPoolExecutor.cpp

HEADERS += \
    $$DIR/QtThreadPoolExecutor.h
