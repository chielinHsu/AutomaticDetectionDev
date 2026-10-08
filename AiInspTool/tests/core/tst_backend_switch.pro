# ADR-021：執行中切換 back end 的規則（BackendSwitch、啟動時用哪一個、SearchEngine 的擁有權）。沒有 Qt 的 application 層。

TARGET = tst_backend_switch
include(core_tests.pri)
SOURCES += $$AOI_ROOT/tests/core/tst_backend_switch.cpp
