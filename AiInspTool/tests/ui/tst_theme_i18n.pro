# 主題（tokens.json）與文案（copy.json）：也證明 aoi_ux.qrc 的資源有內嵌進程式（aoi_target.pri）。

TARGET = tst_theme_i18n
include(ui_tests.pri)
SOURCES += $$AOI_ROOT/tests/ui/tst_theme_i18n.cpp
