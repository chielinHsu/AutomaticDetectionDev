# tests/domain：domain 層的測試（S05：晶格估計、預測、正規方程累加器、NMS；S06：ImageView）。
# 被測的 aoi_domain 只用標準庫；Qt 只在測試程式裡（讀 JSON fixture、QTest 巨集）。
AOI_LINK = domain
include($$AOI_QMAKE/aoi_test.pri)
DEFINES += AOI_DOMAIN_FIXTURES=\\\"$$AOI_ROOT/tests/domain/fixtures\\\"
HEADERS += $$AOI_ROOT/tests/domain/LatticeTestSupport.h
