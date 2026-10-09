# 契約：ITemplateBuilder 與 IPatternSearcher（錯誤碼、分數與排序、區域、空結果、取消、可重入、座標與角度的慣例、準確度）。

TARGET = tst_contract_searcher
include(contract_tests.pri)
SOURCES += $$AOI_ROOT/tests/contract/tst_contract_searcher.cpp
