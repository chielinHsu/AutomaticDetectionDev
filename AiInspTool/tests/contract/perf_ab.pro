# 效能量測（-Perf）：A/B 並排。每個會讀像素的 back end 在同一組 5 MP 合成板子上：時間、準確度、分數的分布、峰值記憶體，
# 兩個都能用時再比一次（compareProposals）。psapi：GetProcessMemoryInfo。

TARGET = perf_ab
include(contract_tests.pri)
SOURCES += $$AOI_ROOT/tests/contract/perf_ab.cpp
LIBS += psapi.lib
