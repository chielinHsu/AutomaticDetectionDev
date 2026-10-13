# 探測（fix/halcon-accuracy）：不列進 tests.pro，單獨 qmake 建置。
TARGET = probe_halcon_accuracy
AOI_LINK_EXTRA = backend_halcon
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/probe_halcon_accuracy.cpp
