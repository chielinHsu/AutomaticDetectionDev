cmake_minimum_required(VERSION 3.21)
project(AutoCadCreated VERSION 0.1.0 LANGUAGES CXX)

# 分層與依賴規則見 docs/architecture.md；目標之間的 target_link_libraries 就是依賴方向，
# 違規的 #include 由 tools/check-deps.mjs（S03）擋下。

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AOI_WITH_OPENCV "Build the OpenCV back end (S07)" OFF)
option(AOI_WITH_HALCON "Build the HALCON back end (S08)" OFF)
option(AOI_BUILD_TESTS "Build the Qt Test targets" ON)
option(AOI_DEMO_SCENE "Link the synthetic scene generator into the app (--demo)" ON)

# Core（storage、threading）、Gui（image_qt 的 QImage）、Widgets（presentation）。
set(AOI_QT_COMPONENTS Core Gui Widgets)
if(AOI_BUILD_TESTS)
  list(APPEND AOI_QT_COMPONENTS Test)
endif()
find_package(Qt6 6.5 REQUIRED COMPONENTS ${AOI_QT_COMPONENTS})
qt_standard_project_setup()

# 視覺函式庫只在對應的 infrastructure/backend_* 使用（S07、S08 才有程式碼）。
# 這裡先確認「選項打開時找得到」，之後的步驟不必再回頭處理探測。
if(AOI_WITH_OPENCV)
  find_package(OpenCV REQUIRED)
  message(STATUS "OpenCV ${OpenCV_VERSION} (${OpenCV_DIR})")
endif()
if(AOI_WITH_HALCON)
  if(NOT DEFINED ENV{HALCONROOT})
    message(FATAL_ERROR "AOI_WITH_HALCON=ON needs the HALCONROOT environment variable")
  endif()
  if(NOT EXISTS "$ENV{HALCONROOT}/include/halconcpp/HalconCpp.h")
    message(FATAL_ERROR
      "AOI_WITH_HALCON=ON, but $ENV{HALCONROOT}/include/halconcpp/HalconCpp.h does not exist. "
      "The HALCON C++ interface (headers and import libraries) is not installed.")
  endif()
  message(STATUS "HALCON $ENV{HALCONROOT}")
endif()

include(cmake/AoiTargets.cmake)

add_subdirectory(src)

if(AOI_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
