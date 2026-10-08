# 合成板子與框，供測試與效能量測用，也給 AiInspTool 的 --demo 選項用（AOI_DEMO_SCENE=ON）。不是產品功能。

AOI_MODULE = presentation_dev
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/presentation/dev
SOURCES += \
    $$DIR/FakeScene.cpp

HEADERS += \
    $$DIR/FakeScene.h
