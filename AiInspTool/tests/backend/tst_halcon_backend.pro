# S08：HALCON。這台機器沒有授權（或沒有 HALCON 的 DLL）時，測試在 initTestCase 跳過並說原因（QSKIP），不是失敗也不是通過。
# AOI_WITH_HALCON=ON 才有（tests/tests.pro）。

TARGET = tst_halcon_backend
AOI_LINK_EXTRA = backend_halcon
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/tst_halcon_backend.cpp
