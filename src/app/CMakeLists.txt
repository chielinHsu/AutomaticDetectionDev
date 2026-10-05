# app：組裝根。只做組裝，不放業務規則與繪圖。
# aoi_app_core 把 back end、執行緒、PerfProbe 與 use case 接在一起；獨立成 library 讓測試能驗證組裝的結果。
add_library(aoi_app_core STATIC CompositionRoot.cpp)
target_link_libraries(aoi_app_core PUBLIC aoi_application aoi_backend_fake aoi_perf aoi_threading aoi_threading_qt)
aoi_set_warnings(aoi_app_core)

qt_add_executable(aoi main.cpp)
target_link_libraries(aoi PRIVATE aoi_presentation aoi_app_core)
if(AOI_DEMO_SCENE)
  target_link_libraries(aoi PRIVATE aoi_presentation_dev)
  target_compile_definitions(aoi PRIVATE AOI_DEMO_SCENE=1)
endif()
aoi_set_warnings(aoi)
