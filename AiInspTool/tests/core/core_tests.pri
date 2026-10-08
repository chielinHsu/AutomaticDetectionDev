# tests/core：核心（domain、application、infrastructure 的非視覺部分）與組裝根的測試。ADR-003：統一用 Qt Test。
# 測試不是五層之一（check-deps 不掃 tests/），所以可以同時連到各層。
AOI_LINK = application backend_fake perf threading threading_qt app_core
include($$AOI_QMAKE/aoi_test.pri)
HEADERS += \
    $$AOI_ROOT/tests/core/CoreTestSupport.h \
    $$AOI_ROOT/tests/core/ProposalTestSupport.h \
    $$AOI_ROOT/tests/core/QtTestPrinters.h
