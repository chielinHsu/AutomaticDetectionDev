# 契約：use case ＋ 每個會讀像素的 back end，在 8 種合成板子（加上 ADR-004 §5 的三種，那幾列是預期失敗）上和標準答案比
# （T6a、T6b 的單元版本）。

TARGET = tst_contract_end_to_end
include(contract_tests.pri)
SOURCES += $$AOI_ROOT/tests/contract/tst_contract_end_to_end.cpp
