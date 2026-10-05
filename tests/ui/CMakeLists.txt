# Qt Test，經 CTest 在 offscreen 平台執行（QT_QPA_PLATFORM、QT_QPA_FONTDIR 由 aoi_add_qt_test 設定）。
set(AOI_UI_TEST_LIBS aoi_presentation aoi_presentation_dev)

aoi_add_qt_test(tst_theme_i18n SOURCES tst_theme_i18n.cpp LIBS ${AOI_UI_TEST_LIBS})
aoi_add_qt_test(tst_items SOURCES tst_items.cpp LIBS ${AOI_UI_TEST_LIBS})
aoi_add_qt_test(tst_canvas SOURCES tst_canvas.cpp LIBS ${AOI_UI_TEST_LIBS})
aoi_add_qt_test(tst_tools SOURCES tst_tools.cpp LIBS ${AOI_UI_TEST_LIBS})
aoi_add_qt_test(tst_shell SOURCES tst_shell.cpp LIBS ${AOI_UI_TEST_LIBS})

# 純 QPainter 微基準（證據，不是閘門）。
aoi_add_qt_test(perf_paint_micro SOURCES perf_paint_micro.cpp LIBS ${AOI_UI_TEST_LIBS} LABELS perf)

# 效能基準線：只在 Release／RelWithDebInfo 有意義，預設的 test preset 排除 label=perf。
aoi_add_qt_test(perf_ui_baseline SOURCES perf_ui_baseline.cpp LIBS ${AOI_UI_TEST_LIBS} LABELS perf)
# Debug 的數字沒有意義（沒最佳化），所以只有 Release／RelWithDebInfo 才把預算當成失敗條件。
target_compile_definitions(perf_ui_baseline PRIVATE
    AOI_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    AOI_BUILD_TYPE="$<CONFIG>"
    $<$<NOT:$<CONFIG:Debug>>:AOI_PERF_ENFORCE=1>)
