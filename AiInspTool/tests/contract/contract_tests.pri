# tests/contract：契約測試（S09）。同一組測試以工廠參數化，對組裝根註冊的每一個 back end（fake、opencv、halcon）各跑一列：
# 沒編進程式（AOI_WITH_OPENCV=OFF、AOI_WITH_HALCON=OFF）或沒有授權的那一列 SKIPPED 並說原因。所以這些測試不管哪一種建置都有
# （tests/tests.pro 的 AOI_TESTS），連結 app_core：它依建置的選項帶進 back end 的程式庫與 AOI_WITH_* 巨集。
# 容差只寫在 tests/contract/ContractTolerances.h。
AOI_LINK = application backend_fake test_support app_core
include($$AOI_QMAKE/aoi_test.pri)
INCLUDEPATH += $$AOI_ROOT/tests/contract $$AOI_ROOT/tests/backend
HEADERS += \
    $$AOI_ROOT/tests/contract/ContractSupport.h \
    $$AOI_ROOT/tests/contract/ContractTolerances.h \
    $$AOI_ROOT/tests/backend/BackendTestSupport.h
