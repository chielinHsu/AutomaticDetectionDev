# 核心（domain、application、infrastructure 的非視覺部分）的測試。ADR-003：統一用 Qt Test。
# 測試本身不是五層之一（check-deps 不掃 tests/），所以可以同時連到各層。
set(AOI_CORE_TEST_LIBS aoi_application aoi_backend_fake aoi_perf aoi_threading aoi_threading_qt aoi_app_core)

aoi_add_qt_test(tst_domain SOURCES tst_domain.cpp LIBS ${AOI_CORE_TEST_LIBS})
aoi_add_qt_test(tst_core_types SOURCES tst_core_types.cpp LIBS ${AOI_CORE_TEST_LIBS})
# tst_grid（S03 暫代 SimpleGrid 的規格）在 S05 搬到 tests/domain/tst_lattice.cpp。
aoi_add_qt_test(tst_job_runner SOURCES tst_job_runner.cpp LIBS ${AOI_CORE_TEST_LIBS})
aoi_add_qt_test(tst_generate_objects SOURCES tst_generate_objects.cpp LIBS ${AOI_CORE_TEST_LIBS})
aoi_add_qt_test(tst_fake_backend SOURCES tst_fake_backend.cpp LIBS ${AOI_CORE_TEST_LIBS})
aoi_add_qt_test(tst_backend_registry SOURCES tst_backend_registry.cpp LIBS ${AOI_CORE_TEST_LIBS})
aoi_add_qt_test(tst_perf_probe SOURCES tst_perf_probe.cpp LIBS ${AOI_CORE_TEST_LIBS})

# S04：互動狀態機、復原、文件與工作階段（application 層，沒有 Qt 的畫面）。
aoi_add_qt_test(tst_undo_stack SOURCES tst_undo_stack.cpp LIBS ${AOI_CORE_TEST_LIBS})
aoi_add_qt_test(tst_state_machine SOURCES tst_state_machine.cpp LIBS ${AOI_CORE_TEST_LIBS})
aoi_add_qt_test(tst_box_expander SOURCES tst_box_expander.cpp LIBS ${AOI_CORE_TEST_LIBS})
aoi_add_qt_test(tst_proposal_session SOURCES tst_proposal_session.cpp LIBS ${AOI_CORE_TEST_LIBS})
aoi_add_qt_test(tst_document SOURCES tst_document.cpp LIBS ${AOI_CORE_TEST_LIBS})
aoi_add_qt_test(tst_session SOURCES tst_session.cpp LIBS ${AOI_CORE_TEST_LIBS})
aoi_add_qt_test(tst_qt_executor SOURCES tst_qt_executor.cpp LIBS ${AOI_CORE_TEST_LIBS})

# S06：影像來源的組合（ImageSourceChain）與背景存檔（RecipeSaver），都是 application 層、用假的 port。
aoi_add_qt_test(tst_image_chain SOURCES tst_image_chain.cpp LIBS ${AOI_CORE_TEST_LIBS})
aoi_add_qt_test(tst_recipe_saver SOURCES tst_recipe_saver.cpp LIBS ${AOI_CORE_TEST_LIBS})

# 效能量測（label perf）：PerfProbe 單次成本、JobRunner 延遲與取消延遲、use case 本身的成本。
# 只有 Release／RelWithDebInfo 才把「PerfProbe 關閉時 < 1 µs」當成失敗條件；預設的 test preset 排除 label=perf。
aoi_add_qt_test(perf_core SOURCES perf_core.cpp LIBS ${AOI_CORE_TEST_LIBS} LABELS perf)
target_compile_definitions(perf_core PRIVATE
    AOI_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    AOI_BUILD_TYPE="$<CONFIG>"
    $<$<NOT:$<CONFIG:Debug>>:AOI_PERF_ENFORCE=1>)

# 依賴方向檢查（tools/check-deps.mjs）。需要 Node.js；找不到時不註冊，並在 configure 時警告（不是靜默通過）。
#   check_deps                       掃這個 repo 的 src/，違規即失敗
#   check_deps_self_test             13 個 fixture：該報的違規一個不漏、不該報的一個不多
#   check_deps_catches_a_violation   故意違規的 fixture（domain 引用 <QString>），工具必須以非 0 結束（WILL_FAIL）
find_program(AOI_NODE node)
if(AOI_NODE)
  set(AOI_CHECK_DEPS ${PROJECT_SOURCE_DIR}/tools/check-deps.mjs)
  add_test(NAME check_deps COMMAND ${AOI_NODE} ${AOI_CHECK_DEPS} --root ${PROJECT_SOURCE_DIR})
  add_test(NAME check_deps_self_test COMMAND ${AOI_NODE} ${AOI_CHECK_DEPS} --self-test)
  add_test(NAME check_deps_catches_a_violation
           COMMAND ${AOI_NODE} ${AOI_CHECK_DEPS} --root ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/deps/domain-qt)
  set_tests_properties(check_deps_catches_a_violation PROPERTIES WILL_FAIL TRUE)
else()
  message(WARNING "node was not found: the check_deps tests are NOT registered. Install Node.js to run the dependency check.")
endif()

# 命令列：從外面用真的 aoi 行程驗證 --backend（沒編進程式的 back end 要明確失敗、結束碼 2）。
aoi_add_qt_test(tst_cli SOURCES tst_cli.cpp LIBS ${AOI_CORE_TEST_LIBS})
add_dependencies(tst_cli aoi)
target_compile_definitions(tst_cli PRIVATE
    AOI_EXE_PATH="$<TARGET_FILE:aoi>"
    AOI_FONT_DIR="${AOI_FONT_DIR}")
# S07：OpenCV 編進程式時，--backend=opencv 要能啟動、組裝根要登記成可用。
if(AOI_WITH_OPENCV)
  target_compile_definitions(tst_cli PRIVATE AOI_WITH_OPENCV=1)
  target_compile_definitions(tst_backend_registry PRIVATE AOI_WITH_OPENCV=1)
endif()
# S08：HALCON 編進程式時，--backend=halcon 要能啟動（有授權時）；沒有授權、沒有 DLL 時要明確失敗（結束碼 2、原因）。
if(AOI_WITH_HALCON)
  target_compile_definitions(tst_cli PRIVATE AOI_WITH_HALCON=1)
  target_compile_definitions(tst_backend_registry PRIVATE AOI_WITH_HALCON=1)
endif()
