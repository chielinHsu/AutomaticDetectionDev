# 效能量測（-Perf）：T1、T2b、T4、T6a／T6b、T7、授權檢查、取消、角度範圍與影像大小的曲線；
# OpenCV 也編進來時同一組情境並排量（CV-*）。psapi：GetProcessMemoryInfo。

TARGET = perf_halcon
AOI_LINK_EXTRA = backend_halcon
AOI_LINK_IF_OPENCV = backend_opencv
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/perf_halcon.cpp
LIBS += psapi.lib
