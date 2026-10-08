# 組裝根的 halcon ＋ use case，同一組合成板子。

TARGET = tst_halcon_end_to_end
AOI_LINK_EXTRA = backend_halcon
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/tst_halcon_end_to_end.cpp
