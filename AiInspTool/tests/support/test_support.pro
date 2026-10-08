# 測試用的共用程式（S06）：合成板子產生器（標準庫）與把它存成 PNG＋JSON 的工具（Qt）。不是產品的一部分。
# 每個測試另外直接編進 tests/support/test_environment.cpp（qmake/aoi_test.pri），不放在這個程式庫裡。

AOI_MODULE = test_support
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/tests/support
SOURCES += \
    $$DIR/synthetic_board.cpp \
    $$DIR/synthetic_board_io.cpp

HEADERS += \
    $$DIR/synthetic_board.h \
    $$DIR/synthetic_board_io.h
