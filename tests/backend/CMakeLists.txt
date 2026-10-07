# 視覺 back end 的測試（S07 OpenCV、S08 HALCON）：範本建立器與搜尋器的契約（S09 契約測試的草稿）、合成板子上的端到端、
# 樣本庫比對器（流程 B）與效能量測（label perf）。AOI_WITH_OPENCV、AOI_WITH_HALCON 各自打開才有對應的測試。
# 測試不是五層之一（check-deps 不掃 tests/），所以可以同時連到各層。
if(NOT AOI_WITH_OPENCV AND NOT AOI_WITH_HALCON)
  return()
endif()

set(AOI_BACKEND_TEST_LIBS aoi_application aoi_backend_fake aoi_test_support aoi_app_core)

function(aoi_add_backend_test name)
  cmake_parse_arguments(B "" "" "LIBS;LABELS" ${ARGN})
  if(B_LABELS)
    aoi_add_qt_test(${name} SOURCES ${name}.cpp LIBS ${AOI_BACKEND_TEST_LIBS} ${B_LIBS} LABELS ${B_LABELS})
  else()
    aoi_add_qt_test(${name} SOURCES ${name}.cpp LIBS ${AOI_BACKEND_TEST_LIBS} ${B_LIBS})
  endif()
  target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/tests/core)  # QtTestPrinters.h
endfunction()

if(AOI_WITH_OPENCV)
  aoi_add_backend_test(tst_opencv_backend LIBS aoi_backend_opencv)
  aoi_add_backend_test(tst_opencv_end_to_end LIBS aoi_backend_opencv)
  aoi_add_backend_test(tst_opencv_library_matcher LIBS aoi_backend_opencv)
  # 使用者的真實影像（不入版控）：AOI_REAL_IMAGE_DIR 指到影像的資料夾才執行，否則跳過。只在效能量測時跑。
  aoi_add_backend_test(perf_opencv_real LIBS aoi_backend_opencv LABELS perf)
  # 效能量測（label perf）：T1、T2a、T4、T6a／T6b、T7、角度範圍、影像大小、樣本大小、執行緒數、峰值記憶體（docs/steps/S07-opencv/perf.md）。
  aoi_add_backend_test(perf_opencv LIBS aoi_backend_opencv LABELS perf)
  target_compile_definitions(perf_opencv PRIVATE AOI_SOURCE_DIR="${PROJECT_SOURCE_DIR}" AOI_BUILD_TYPE="$<CONFIG>")
  target_link_libraries(perf_opencv PRIVATE psapi)
endif()

# S08：HALCON。這台機器沒有授權（或沒有 HALCON 的 DLL）時，測試在 initTestCase 跳過並說原因（QSKIP），不是失敗也不是通過。
if(AOI_WITH_HALCON)
  aoi_add_backend_test(tst_halcon_backend LIBS aoi_backend_halcon)
  aoi_add_backend_test(tst_halcon_end_to_end LIBS aoi_backend_halcon)
  aoi_add_backend_test(tst_halcon_library_matcher LIBS aoi_backend_halcon)
  # 效能量測（label perf）：T1、T2b、T4、T6a／T6b、T7、授權檢查、取消、角度範圍與影像大小的曲線；OpenCV 也編進來時同一組情境並排量。
  aoi_add_backend_test(perf_halcon LIBS aoi_backend_halcon LABELS perf)
  target_compile_definitions(perf_halcon PRIVATE AOI_SOURCE_DIR="${PROJECT_SOURCE_DIR}" AOI_BUILD_TYPE="$<CONFIG>")
  target_link_libraries(perf_halcon PRIVATE psapi)
  if(AOI_WITH_OPENCV)
    target_link_libraries(perf_halcon PRIVATE aoi_backend_opencv)
    target_compile_definitions(perf_halcon PRIVATE AOI_WITH_OPENCV=1)
    # 真實影像：同 perf_opencv_real（AOI_REAL_IMAGE_DIR），同一張影像兩個 back end 並排。讀 TIFF 要 OpenCV 的影像來源。
    aoi_add_backend_test(perf_halcon_real LIBS aoi_backend_halcon aoi_backend_opencv LABELS perf)
  endif()
endif()
