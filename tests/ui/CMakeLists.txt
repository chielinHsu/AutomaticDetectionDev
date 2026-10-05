# Qt Test，經 CTest 在 offscreen 平台執行（QT_QPA_PLATFORM、QT_QPA_FONTDIR 由 aoi_add_qt_test 設定）。
# S04：Presenter 的測試走產品的接線（core → MainWindow → AppPresenters），所以也連組裝根與假 back end。
set(AOI_UI_TEST_LIBS aoi_presentation aoi_presentation_dev aoi_app_core)

aoi_add_qt_test(tst_theme_i18n SOURCES tst_theme_i18n.cpp LIBS ${AOI_UI_TEST_LIBS})
aoi_add_qt_test(tst_items SOURCES tst_items.cpp LIBS ${AOI_UI_TEST_LIBS})
aoi_add_qt_test(tst_canvas SOURCES tst_canvas.cpp LIBS ${AOI_UI_TEST_LIBS})
aoi_add_qt_test(tst_tools SOURCES tst_tools.cpp LIBS ${AOI_UI_TEST_LIBS})
aoi_add_qt_test(tst_shell SOURCES tst_shell.cpp LIBS ${AOI_UI_TEST_LIBS})
aoi_add_qt_test(tst_objects_model SOURCES tst_objects_model.cpp LIBS ${AOI_UI_TEST_LIBS})
aoi_add_qt_test(tst_params SOURCES tst_params.cpp LIBS ${AOI_UI_TEST_LIBS})
aoi_add_qt_test(tst_messages SOURCES tst_messages.cpp LIBS ${AOI_UI_TEST_LIBS})
aoi_add_qt_test(tst_scene SOURCES tst_scene.cpp LIBS ${AOI_UI_TEST_LIBS})
target_include_directories(tst_scene PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core)  # ProposalTestSupport.h
aoi_add_qt_test(tst_presenter SOURCES tst_presenter.cpp LIBS ${AOI_UI_TEST_LIBS})
# QtTestPrinters.h（狀態的 QCOMPARE）放在 tests/core，UI 測試共用。
target_include_directories(tst_presenter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core)

# 純 QPainter 微基準（證據，不是閘門）。
aoi_add_qt_test(perf_paint_micro SOURCES perf_paint_micro.cpp LIBS ${AOI_UI_TEST_LIBS} LABELS perf)

# S04 的效能量測：U2、U3、I3、I4、I6。
aoi_add_qt_test(perf_ui_s04 SOURCES perf_ui_s04.cpp LIBS ${AOI_UI_TEST_LIBS} LABELS perf)
target_include_directories(perf_ui_s04 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core)
target_compile_definitions(perf_ui_s04 PRIVATE
    AOI_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    AOI_BUILD_TYPE="$<CONFIG>"
    $<$<NOT:$<CONFIG:Debug>>:AOI_PERF_ENFORCE=1>)

# 效能基準線：只在 Release／RelWithDebInfo 有意義，預設的 test preset 排除 label=perf。
aoi_add_qt_test(perf_ui_baseline SOURCES perf_ui_baseline.cpp LIBS ${AOI_UI_TEST_LIBS} LABELS perf)
# Debug 的數字沒有意義（沒最佳化），所以只有 Release／RelWithDebInfo 才把預算當成失敗條件。
target_compile_definitions(perf_ui_baseline PRIVATE
    AOI_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    AOI_BUILD_TYPE="$<CONFIG>"
    $<$<NOT:$<CONFIG:Debug>>:AOI_PERF_ENFORCE=1>)
