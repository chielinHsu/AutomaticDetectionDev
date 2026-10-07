# 假 back end：只用標準庫，不看影像內容。見 FakeGrid.h。

AOI_MODULE = backend_fake
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/infrastructure/backend_fake
SOURCES += \
    $$DIR/FakeBackend.cpp \
    $$DIR/FakeGrid.cpp

HEADERS += \
    $$DIR/FakeBackend.h \
    $$DIR/FakeGrid.h
