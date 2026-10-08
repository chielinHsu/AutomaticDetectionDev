# 效能量測（-Perf）：T1、T2a、T4、T6a／T6b、T7、角度範圍、影像大小、樣本大小、執行緒數、峰值記憶體（docs/steps/S07-opencv/perf.md）。
# psapi：GetProcessMemoryInfo（峰值記憶體）。

TARGET = perf_opencv
AOI_LINK_EXTRA = backend_opencv
include(backend_tests.pri)
SOURCES += $$AOI_ROOT/tests/backend/perf_opencv.cpp
LIBS += psapi.lib
