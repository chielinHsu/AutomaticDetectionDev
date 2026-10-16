# 所有 Qt Test（ADR-003：一個測試類別一個執行檔，在 <建置>/bin）。AiInspTool.pro 在 AOI_BUILD_TESTS=ON 時才納入，
# 而且排在 src/ 之後（tst_cli 要 AiInspTool.exe）。
#
# 這裡是「建置哪些測試」的唯一清單（CMake 時代在 tests/*/CMakeLists.txt）：
#   AOI_TESTS        單元與整合測試：scripts/dev/test.ps1 預設跑這些（再加 4 個 node 的檢查）
#   AOI_PERF_TESTS   效能量測：test.ps1 -Perf 只跑這些；只在 Release 有意義
#   AOI_TOOLS        不是測試的工具（和測試一樣的接線）：一起建置，不進任何清單，手動執行（2026-10-15：record_guidance）
# qmake 執行時把兩份清單寫到 <建置>/aoi-tests.txt、aoi-perf-tests.txt，test.ps1 照清單執行（不看 bin/ 裡有什麼檔案，
# 所以不會跑到過期的執行檔）。Qt Creator 的測試面板自己從原始碼找測試。
# S09 的契約測試（contract/）在每一種建置都有：沒編進來或沒有授權的 back end，那一列 SKIPPED 並說原因。

TEMPLATE = subdirs
include($$AOI_QMAKE/aoi_options.pri)

SUBDIRS += test_support
test_support.file = support/test_support.pro

AOI_TESTS = \
    core/tst_domain \
    core/tst_core_types \
    core/tst_job_runner \
    core/tst_generate_objects \
    core/tst_fake_backend \
    core/tst_backend_registry \
    core/tst_backend_switch \
    core/tst_backend_comparison \
    core/tst_perf_probe \
    core/tst_undo_stack \
    core/tst_state_machine \
    core/tst_box_expander \
    core/tst_proposal_session \
    core/tst_document \
    core/tst_session \
    core/tst_step_guide \
    core/tst_qt_executor \
    core/tst_image_chain \
    core/tst_recipe_saver \
    core/tst_cli \
    domain/tst_lattice \
    domain/tst_lattice_scenarios \
    domain/tst_lattice_parts \
    domain/tst_lattice_rules \
    domain/tst_nms \
    domain/tst_match_region \
    domain/tst_image_view \
    domain/tst_lattice_explainer \
    domain/tst_lattice_crosscheck \
    infra/tst_image_qt \
    infra/tst_recipe_store \
    infra/tst_library_store \
    infra/tst_library_layers \
    infra/tst_synthetic_board \
    infra/tst_image_source_composition \
    ui/tst_theme_i18n \
    ui/tst_items \
    ui/tst_canvas \
    ui/tst_tools \
    ui/tst_shell \
    ui/tst_objects_model \
    ui/tst_params \
    ui/tst_messages \
    ui/tst_scene \
    ui/tst_presenter \
    ui/tst_compare \
    contract/tst_contract_searcher \
    contract/tst_contract_end_to_end \
    contract/tst_contract_library_matcher

AOI_PERF_TESTS = \
    core/perf_core \
    domain/perf_lattice \
    infra/perf_storage \
    ui/perf_paint_micro \
    ui/perf_ui_s04 \
    ui/perf_ui_baseline \
    ui/perf_compare_ui \
    contract/perf_ab \
    perf/perf_matrix \
    perf/perf_e2e

AOI_TOOLS = \
    ui/record_guidance

# S06、S07：OpenCV 的影像 adapter 與 back end。
equals(AOI_WITH_OPENCV, ON) {
    AOI_TESTS += \
        infra/tst_opencv_image \
        infra/tst_image_sources_agree \
        backend/tst_opencv_backend
    AOI_PERF_TESTS += backend/perf_opencv_real backend/perf_opencv backend/perf_match_region_real
}
# S08：HALCON（沒有授權或沒有 DLL 時，測試跳過並說原因）。真實影像的並排量測讀 TIFF 要 OpenCV。
equals(AOI_WITH_HALCON, ON) {
    AOI_TESTS += \
        backend/tst_halcon_backend \
        backend/tst_halcon_library_matcher
    AOI_PERF_TESTS += backend/perf_halcon
    equals(AOI_WITH_OPENCV, ON): AOI_PERF_TESTS += backend/perf_halcon_real
}

for(test, $$list($$AOI_TESTS $$AOI_PERF_TESTS $$AOI_TOOLS)) {
    name = $$basename(test)
    SUBDIRS += $$name
    $${name}.file = $${test}.pro
    $${name}.depends = test_support
}

aoi_test_names =
for(test, AOI_TESTS): aoi_test_names += $$basename(test)
aoi_perf_names =
for(test, AOI_PERF_TESTS): aoi_perf_names += $$basename(test)
write_file($$AOI_BUILD_ROOT/aoi-tests.txt, aoi_test_names)|error("Cannot write $$AOI_BUILD_ROOT/aoi-tests.txt")
write_file($$AOI_BUILD_ROOT/aoi-perf-tests.txt, aoi_perf_names)|error("Cannot write $$AOI_BUILD_ROOT/aoi-perf-tests.txt")
