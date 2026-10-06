# domain 層的測試（S05：晶格估計、預測、正規方程累加器、NMS）。ADR-003：統一用 Qt Test。
# 被測的 aoi_domain 只用標準庫；Qt 只在測試程式裡（讀 JSON fixture、QTest 巨集）。
aoi_add_qt_test(tst_lattice SOURCES tst_lattice.cpp LIBS aoi_domain)
aoi_add_qt_test(tst_lattice_scenarios SOURCES tst_lattice_scenarios.cpp LIBS aoi_domain)
aoi_add_qt_test(tst_lattice_parts SOURCES tst_lattice_parts.cpp LIBS aoi_domain)
# 內部規則各自的測試（找 v、二維成立、證據、缺格、連通）：規則彼此備援，只看估計器的結果時，拿掉一條常常看不出來。
aoi_add_qt_test(tst_lattice_rules SOURCES tst_lattice_rules.cpp LIBS aoi_domain)
aoi_add_qt_test(tst_nms SOURCES tst_nms.cpp LIBS aoi_domain)

# S06：ImageView 的所有權規則、ImageBuffer、subView、複製計數。
aoi_add_qt_test(tst_image_view SOURCES tst_image_view.cpp LIBS aoi_domain)

# 說明頁（docs/steps/S05-lattice/explainer.html）的資料由 C++ 算：檢查頁面裡的資料仍是目前的程式算出來的。
# 更新頁面：設環境變數 AOI_EXPORT_EXPLAINER=1 再執行 tst_lattice_explainer。
aoi_add_qt_test(tst_lattice_explainer SOURCES tst_lattice_explainer.cpp LIBS aoi_domain)
target_compile_definitions(tst_lattice_explainer PRIVATE AOI_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# 與 S00 示範頁交叉驗證：fixture 由 tools/s00-demo-fixture.mjs 執行 S00 頁面原本的 JavaScript 產生。
aoi_add_qt_test(tst_lattice_crosscheck SOURCES tst_lattice_crosscheck.cpp LIBS aoi_domain)
target_compile_definitions(tst_lattice_crosscheck PRIVATE AOI_DOMAIN_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
# fixture 與 S00 頁面一致（頁面的演算法改了，fixture 就過期）。需要 Node.js；找不到時不註冊（tests/core 已經警告過）。
if(AOI_NODE)
  add_test(NAME s00_demo_fixture_is_current COMMAND ${AOI_NODE} ${PROJECT_SOURCE_DIR}/tools/s00-demo-fixture.mjs --check)
endif()

# 效能量測（label perf）：T3（1 000 點擬合 ≤ 2 ms）、NMS、漸進重新擬合。只有 Release／RelWithDebInfo 把 T3 當成失敗條件。
aoi_add_qt_test(perf_lattice SOURCES perf_lattice.cpp LIBS aoi_domain LABELS perf)
target_compile_definitions(perf_lattice PRIVATE
    AOI_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    AOI_BUILD_TYPE="$<CONFIG>"
    $<$<NOT:$<CONFIG:Debug>>:AOI_PERF_ENFORCE=1>)
