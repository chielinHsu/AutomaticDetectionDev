# OpenCV back end 的測試（S07）：範本建立器與搜尋器的契約（S09 契約測試的草稿）、合成板子上的端到端、
# 樣本庫比對器（流程 B）與效能量測（label perf：T1、T2a、T4、T6、T7、角度範圍的曲線）。AOI_WITH_OPENCV=ON 才有。
# 測試不是五層之一（check-deps 不掃 tests/），所以可以同時連到各層。
if(NOT AOI_WITH_OPENCV)
  return()
endif()

set(AOI_BACKEND_TEST_LIBS aoi_application aoi_backend_fake aoi_backend_opencv aoi_test_support aoi_app_core)

function(aoi_add_backend_test name)
  cmake_parse_arguments(B "" "" "LIBS;LABELS" ${ARGN})
  if(B_LABELS)
    aoi_add_qt_test(${name} SOURCES ${name}.cpp LIBS ${AOI_BACKEND_TEST_LIBS} ${B_LIBS} LABELS ${B_LABELS})
  else()
    aoi_add_qt_test(${name} SOURCES ${name}.cpp LIBS ${AOI_BACKEND_TEST_LIBS} ${B_LIBS})
  endif()
  target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/tests/core)  # QtTestPrinters.h
endfunction()

aoi_add_backend_test(tst_opencv_backend)
aoi_add_backend_test(tst_opencv_end_to_end)
aoi_add_backend_test(tst_opencv_library_matcher)
# 使用者的真實影像（不入版控）：AOI_REAL_IMAGE_DIR 指到影像的資料夾才執行，否則跳過。只在效能量測時跑。
aoi_add_backend_test(perf_opencv_real LABELS perf)
# 效能量測（label perf）：T1、T2a、T4、T6a／T6b、T7、角度範圍、影像大小、樣本大小、執行緒數、峰值記憶體（docs/steps/S07-opencv/perf.md）。
aoi_add_backend_test(perf_opencv LABELS perf)
target_compile_definitions(perf_opencv PRIVATE AOI_SOURCE_DIR="${PROJECT_SOURCE_DIR}" AOI_BUILD_TYPE="$<CONFIG>")
target_link_libraries(perf_opencv PRIVATE psapi)
