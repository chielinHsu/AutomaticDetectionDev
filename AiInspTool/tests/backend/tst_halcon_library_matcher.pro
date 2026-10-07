# HALCON 的樣本庫比對器（流程 B）：一次 find_shape_models，加上兩種做法一致。

TARGET = tst_halcon_library_matcher
AOI_LINK_EXTRA = backend_halcon
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/tst_halcon_library_matcher.cpp
