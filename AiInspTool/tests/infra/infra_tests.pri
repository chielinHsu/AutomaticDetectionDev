# tests/infra：infrastructure 的測試（S06）：Qt 的影像 adapter、配方與樣本庫的 JSON 儲存、合成板子產生器、
# OpenCV 的影像 adapter（AOI_WITH_OPENCV=ON 時）。
AOI_LINK = application image_qt storage threading test_support
include($$AOI_QMAKE/aoi_test.pri)
DEFINES += AOI_INFRA_FIXTURES=\\\"$$AOI_ROOT/tests/infra/fixtures\\\"
HEADERS += \
    $$AOI_ROOT/tests/infra/InfraTestSupport.h \
    $$AOI_ROOT/tests/infra/RecipeTestSupport.h
