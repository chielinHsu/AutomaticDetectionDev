# HALCON 的 adapter（S08）：shape model 的範本建立器、搜尋器、樣本庫比對器、授權檢查。
# 只在 AOI_WITH_HALCON=ON 時建置（src/src.pro）。HALCON 只能出現在這個資料夾（check-deps）。
# 標頭與匯入程式庫來自 qmake/aoi_halcon.pri（halconcpp.dll 延遲載入，連結設定在 aoi_target.pri）。

AOI_MODULE = backend_halcon
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/infrastructure/backend_halcon
SOURCES += \
    $$DIR/HalconBackend.cpp \
    $$DIR/HalconCancelWatch.cpp \
    $$DIR/HalconErrors.cpp \
    $$DIR/HalconImages.cpp \
    $$DIR/HalconLibraryMatcher.cpp \
    $$DIR/HalconLicense.cpp \
    $$DIR/HalconPatternSearcher.cpp \
    $$DIR/HalconRuntime.cpp \
    $$DIR/HalconSearchSteps.cpp \
    $$DIR/HalconTemplate.cpp \
    $$DIR/HalconTemplateBuilder.cpp

HEADERS += \
    $$DIR/HalconBackend.h \
    $$DIR/HalconCancelWatch.h \
    $$DIR/HalconErrors.h \
    $$DIR/HalconImages.h \
    $$DIR/HalconLibraryMatcher.h \
    $$DIR/HalconLicense.h \
    $$DIR/HalconPatternSearcher.h \
    $$DIR/HalconPose.h \
    $$DIR/HalconRuntime.h \
    $$DIR/HalconSearchOptions.h \
    $$DIR/HalconSearchSteps.h \
    $$DIR/HalconTemplate.h \
    $$DIR/HalconTemplateBuilder.h
