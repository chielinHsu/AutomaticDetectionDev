# 與 S00 示範頁交叉驗證：fixture 由 tools/s00-demo-fixture.mjs 執行 S00 頁面原本的 JavaScript 產生
# （fixture 是否過期由 scripts/dev/test.ps1 的 s00_demo_fixture_is_current 檢查）。

TARGET = tst_lattice_crosscheck
include(domain_tests.pri)
SOURCES += $$AOI_ROOT/tests/domain/tst_lattice_crosscheck.cpp
