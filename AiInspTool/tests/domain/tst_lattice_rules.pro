# 內部規則各自的測試（找 v、二維成立、證據、缺格、連通）：規則彼此備援，只看估計器的結果時，拿掉一條常常看不出來。

TARGET = tst_lattice_rules
include(domain_tests.pri)
SOURCES += $$AOI_ROOT/tests/domain/tst_lattice_rules.cpp
