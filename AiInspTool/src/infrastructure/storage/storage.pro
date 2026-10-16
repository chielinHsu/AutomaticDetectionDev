# 儲存（S06）：配方與樣本庫的 JSON 檔。只用 Qt Core（QJsonDocument、QSaveFile），不用 Qt Gui／Widgets、不用視覺函式庫。

AOI_MODULE = storage
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/infrastructure/storage
SOURCES += \
    $$DIR/InspectionCsv.cpp \
    $$DIR/RepeatabilityStorage.cpp \
    $$DIR/JsonCodec.cpp \
    $$DIR/JsonFile.cpp \
    $$DIR/JsonLibraryStore.cpp \
    $$DIR/JsonRecipeStore.cpp \
    $$DIR/LayeredLibraryStore.cpp

HEADERS += \
    $$DIR/CsvRepeatabilitySource.h \
    $$DIR/JsonCodec.h \
    $$DIR/JsonFile.h \
    $$DIR/JsonLibraryStore.h \
    $$DIR/JsonRecipeStore.h \
    $$DIR/LayeredLibraryStore.h
