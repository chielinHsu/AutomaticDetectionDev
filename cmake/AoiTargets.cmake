# 自家程式碼的共用編譯設定。第三方標頭（Qt、OpenCV、HALCON）不受 /W4 /WX 影響。

function(aoi_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE
      /W4 /WX
      /permissive-
      /utf-8
      /Zc:__cplusplus
      /external:W0 /external:anglebrackets)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endfunction()

# offscreen 平台不會自己找到字型（Qt 不附字型），要指到系統的字型資料夾，否則文字量測與截圖都是空的。
if(WIN32 AND DEFINED ENV{WINDIR})
  file(TO_CMAKE_PATH "$ENV{WINDIR}/Fonts" AOI_FONT_DIR)
else()
  set(AOI_FONT_DIR "")
endif()

# Qt Test 在 Windows 不把結果寫到 stdout（實測：重新導向後是空的），所以用 -o 指定 log 檔，
# 放在 <build>/test-logs/<name>.txt。CTest 只看結束碼；失敗時看 log（scripts/dev/test.ps1 會印出來）。
set(AOI_TEST_LOG_DIR "${CMAKE_BINARY_DIR}/test-logs")
file(MAKE_DIRECTORY "${AOI_TEST_LOG_DIR}")

# Qt Test 可執行檔：一個類別一個執行檔，經 CTest 在 offscreen 平台執行。
#   aoi_add_qt_test(<name> SOURCES a.cpp b.cpp [LIBS ...] [LABELS perf])
function(aoi_add_qt_test name)
  cmake_parse_arguments(T "" "" "SOURCES;LIBS;LABELS" ${ARGN})
  qt_add_executable(${name} ${T_SOURCES})
  target_link_libraries(${name} PRIVATE Qt6::Test ${T_LIBS})
  aoi_set_warnings(${name})
  add_test(NAME ${name} COMMAND ${name} -o "${AOI_TEST_LOG_DIR}/${name}.txt,txt")
  # Qt 的 DLL 一定要在 PATH 上；AOI_WITH_OPENCV=ON 時 OpenCV 的 DLL 也是（S06）。
  set(_aoi_path_mods "PATH=path_list_prepend:${AOI_QT_BIN_DIR}")
  if(AOI_OPENCV_BIN_DIR)
    list(APPEND _aoi_path_mods "PATH=path_list_prepend:${AOI_OPENCV_BIN_DIR}")
  endif()
  # AOI_WITH_HALCON=ON（S08）：HALCON 的 DLL（安裝程式通常已放進 PATH；這裡再加一次，不依賴系統設定）。
  if(HALCON_RUNTIME_DIR)
    list(APPEND _aoi_path_mods "PATH=path_list_append:${HALCON_RUNTIME_DIR}")
  endif()
  set_tests_properties(${name} PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_FONTDIR=${AOI_FONT_DIR}"
    ENVIRONMENT_MODIFICATION "${_aoi_path_mods}"
    TIMEOUT 300)
  if(T_LABELS)
    set_tests_properties(${name} PROPERTIES LABELS "${T_LABELS}")
  endif()
endfunction()

# Qt 的 DLL 資料夾（執行測試時要在 PATH 上）。
get_target_property(_aoi_qt_core_loc Qt6::Core IMPORTED_LOCATION_RELEASE)
if(NOT _aoi_qt_core_loc)
  get_target_property(_aoi_qt_core_loc Qt6::Core IMPORTED_LOCATION_DEBUG)
endif()
if(_aoi_qt_core_loc)
  get_filename_component(AOI_QT_BIN_DIR "${_aoi_qt_core_loc}" DIRECTORY)
else()
  set(AOI_QT_BIN_DIR "")
endif()

# OpenCV 的 DLL 資料夾（AOI_WITH_OPENCV=ON 時，執行測試要在 PATH 上）：opencv_world 的 Release 或 Debug 版所在的資料夾。
set(AOI_OPENCV_BIN_DIR "")
if(AOI_WITH_OPENCV AND TARGET opencv_world)
  get_target_property(_aoi_cv_loc opencv_world IMPORTED_LOCATION_RELEASE)
  if(NOT _aoi_cv_loc)
    get_target_property(_aoi_cv_loc opencv_world IMPORTED_LOCATION_DEBUG)
  endif()
  if(_aoi_cv_loc)
    get_filename_component(AOI_OPENCV_BIN_DIR "${_aoi_cv_loc}" DIRECTORY)
  endif()
endif()
