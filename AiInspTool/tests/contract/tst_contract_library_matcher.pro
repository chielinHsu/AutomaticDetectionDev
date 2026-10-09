# 契約：ILibraryMatcher（流程 B）的辨識正確率、排序、位姿誤差、樣本庫大小 N、兩個 back end 一致。

TARGET = tst_contract_library_matcher
include(contract_tests.pri)
SOURCES += $$AOI_ROOT/tests/contract/tst_contract_library_matcher.cpp
