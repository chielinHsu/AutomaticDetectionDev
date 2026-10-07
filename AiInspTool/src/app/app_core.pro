# app：組裝根。只做組裝，不放業務規則與繪圖。
# aoi_app_core 把 back end、執行緒、PerfProbe、影像來源與 use case 接在一起；獨立成程式庫讓測試能驗證組裝的結果。
# AOI_WITH_OPENCV／AOI_WITH_HALCON 打開時，依賴表（aoi_deps.pri）把對應的 back end 加進來並定義同名的巨集。

AOI_MODULE = app_core
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/app
SOURCES += \
    $$DIR/CompositionRoot.cpp

HEADERS += \
    $$DIR/CompositionRoot.h
