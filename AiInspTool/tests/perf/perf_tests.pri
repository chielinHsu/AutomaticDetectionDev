# tests/perf：S10 的效能 harness（搜尋矩陣 perf_matrix、端到端 T5 perf_e2e）。和契約測試一樣連結組裝根：
# 依建置的選項帶進 back end 的程式庫，沒編進來或沒有授權的 back end 那一列 SKIPPED 並說原因。
# psapi：GetProcessMemoryInfo（峰值記憶體）；powrprof：CallNtPowerInformation、PowerGetActiveScheme（量測當下的時脈與電源計畫）。
AOI_LINK = application backend_fake test_support app_core
include($$AOI_QMAKE/aoi_test.pri)
INCLUDEPATH += $$AOI_ROOT/tests/perf $$AOI_ROOT/tests/contract $$AOI_ROOT/tests/backend $$AOI_ROOT/tests/ui
LIBS += psapi.lib powrprof.lib
HEADERS += \
    $$AOI_ROOT/tests/perf/HarnessSupport.h \
    $$AOI_ROOT/tests/contract/ContractSupport.h \
    $$AOI_ROOT/tests/contract/ContractTolerances.h \
    $$AOI_ROOT/tests/backend/BackendTestSupport.h \
    $$AOI_ROOT/tests/ui/MeasurementHygiene.h
