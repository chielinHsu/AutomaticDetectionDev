# 不是測試：產生操作步驟面板的示範畫面（docs/ux/guidance/*.png；ADR-027）。手動執行，見 tests/ui/record_guidance.cpp。

TARGET = record_guidance
include(ui_tests.pri)
SOURCES += $$AOI_ROOT/tests/ui/record_guidance.cpp
