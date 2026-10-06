# infrastructure 的測試（S06）：Qt 的影像 adapter、配方與樣本庫的 JSON 儲存、合成板子產生器、
# OpenCV 的影像 adapter（AOI_WITH_OPENCV=ON 時）。ADR-003：統一用 Qt Test。
# 測試不是五層之一（check-deps 不掃 tests/），所以可以同時連到各層。
set(AOI_INFRA_TEST_LIBS aoi_application aoi_image_qt aoi_storage aoi_threading aoi_test_support)
set(AOI_INFRA_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

function(aoi_add_infra_test name)
  cmake_parse_arguments(I "" "" "LIBS;LABELS" ${ARGN})
  if(I_LABELS)
    aoi_add_qt_test(${name} SOURCES ${name}.cpp LIBS ${AOI_INFRA_TEST_LIBS} ${I_LIBS} LABELS ${I_LABELS})
  else()
    aoi_add_qt_test(${name} SOURCES ${name}.cpp LIBS ${AOI_INFRA_TEST_LIBS} ${I_LIBS})
  endif()
  target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/tests/core)  # QtTestPrinters.h
  target_compile_definitions(${name} PRIVATE AOI_INFRA_FIXTURES="${AOI_INFRA_FIXTURES}")
endfunction()

# QImage ⇄ ImageView、QtImageSource。連 presentation 是為了比對畫布的 ImageItem::wrap 與 adapter 的格式對照表。
aoi_add_infra_test(tst_image_qt LIBS aoi_presentation)
# 配方（v1 範例檔在 fixtures/recipe-v1.json：手寫、不要改，之後的版本也要讀得進來）。
aoi_add_infra_test(tst_recipe_store)
# 樣本庫（一個檔案 ＝ 一層；fixtures/library-v1.json）。
aoi_add_infra_test(tst_library_store)
# 分層（TRI：專案 → 使用者 → 系統；使用者 2026-10-06 決定）。
aoi_add_infra_test(tst_library_layers)

# 合成板子。AOI_SYNTHETIC_REFERENCE_HASH 是「不傾斜的參考板子」的像素雜湊：改了產生器，這個值就會變，
# 確定是刻意的修改之後再更新它（測試的 log 會印出新的值）。它只證明「同樣的輸入給同樣的輸出」，不證明輸出是對的
# （對不對由同一個測試裡的像素與標準答案檢查負責）。
aoi_add_infra_test(tst_synthetic_board)
target_compile_definitions(tst_synthetic_board PRIVATE AOI_SYNTHETIC_REFERENCE_HASH="447862f70216e47b")

# OpenCV 的影像 adapter 與「兩個來源給出相同的影像」（IImageSource 的 Liskov 契約）。AOI_WITH_OPENCV=ON 才有。
if(AOI_WITH_OPENCV)
  aoi_add_infra_test(tst_opencv_image LIBS aoi_backend_opencv)
  aoi_add_infra_test(tst_image_sources_agree LIBS aoi_backend_opencv)
endif()

# 效能量測（label perf）：影像載入（5 MP／25 MP）、轉換的複製次數、配方與樣本庫的存讀。沒有預算，只記錄（perf.md）。
aoi_add_infra_test(perf_storage LIBS aoi_presentation_dev LABELS perf)
target_compile_definitions(perf_storage PRIVATE AOI_SOURCE_DIR="${PROJECT_SOURCE_DIR}" AOI_BUILD_TYPE="$<CONFIG>")
if(AOI_WITH_OPENCV)
  target_link_libraries(perf_storage PRIVATE aoi_backend_opencv)
  target_compile_definitions(perf_storage PRIVATE AOI_WITH_OPENCV=1)
endif()

# 組裝根的 makeImageSource()：OpenCV 開與關兩種建置都要對（TIFF 只有 OpenCV 讀得了）。
aoi_add_infra_test(tst_image_source_composition LIBS aoi_app_core)
if(AOI_WITH_OPENCV)
  target_compile_definitions(tst_image_source_composition PRIVATE AOI_WITH_OPENCV=1)
endif()
