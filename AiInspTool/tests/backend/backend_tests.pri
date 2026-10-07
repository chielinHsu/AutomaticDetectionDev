# tests/backend：視覺 back end 的測試（S07 OpenCV、S08 HALCON）：範本建立器與搜尋器的契約（S09 契約測試的草稿）、
# 合成板子上的端到端、樣本庫比對器（流程 B）與效能量測。AOI_WITH_OPENCV、AOI_WITH_HALCON 各自打開才有對應的測試（tests/tests.pro）。
AOI_LINK = application backend_fake test_support app_core
include($$AOI_QMAKE/aoi_test.pri)
HEADERS += $$AOI_ROOT/tests/backend/BackendTestSupport.h
