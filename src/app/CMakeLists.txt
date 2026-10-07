# app：組裝根。只做組裝，不放業務規則與繪圖。
# aoi_app_core 把 back end、執行緒、PerfProbe、影像來源與 use case 接在一起；獨立成 library 讓測試能驗證組裝的結果。
add_library(aoi_app_core STATIC CompositionRoot.cpp)
target_link_libraries(aoi_app_core PUBLIC aoi_application aoi_backend_fake aoi_image_qt aoi_perf aoi_threading aoi_threading_qt)
if(AOI_WITH_OPENCV)
  target_link_libraries(aoi_app_core PUBLIC aoi_backend_opencv)
  target_compile_definitions(aoi_app_core PRIVATE AOI_WITH_OPENCV=1)
endif()
if(AOI_WITH_HALCON)
  target_link_libraries(aoi_app_core PUBLIC aoi_backend_halcon)
  target_compile_definitions(aoi_app_core PRIVATE AOI_WITH_HALCON=1)
endif()
aoi_set_warnings(aoi_app_core)

qt_add_executable(aoi main.cpp)
target_link_libraries(aoi PRIVATE aoi_presentation aoi_app_core)
if(AOI_DEMO_SCENE)
  target_link_libraries(aoi PRIVATE aoi_presentation_dev)
  target_compile_definitions(aoi PRIVATE AOI_DEMO_SCENE=1)
endif()
aoi_set_warnings(aoi)

# OpenCV 的 DLL 放到 aoi.exe 旁邊（只複製 OpenCV 的：Qt 的 DLL 放過去，Qt 會到執行檔旁邊找外掛而找不到）。
if(AOI_WITH_OPENCV)
  add_custom_command(TARGET aoi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:opencv_world> $<TARGET_FILE_DIR:aoi>
    COMMENT "Copy the OpenCV DLL next to aoi.exe")
endif()
