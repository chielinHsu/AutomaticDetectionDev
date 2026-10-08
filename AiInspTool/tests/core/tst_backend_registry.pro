# 組裝根登記哪些 back end：AOI_WITH_OPENCV／AOI_WITH_HALCON 打開時，組裝根連到對應的 back end，aoi_target.pri 定義同名的巨集。

TARGET = tst_backend_registry
include(core_tests.pri)
SOURCES += $$AOI_ROOT/tests/core/tst_backend_registry.cpp
