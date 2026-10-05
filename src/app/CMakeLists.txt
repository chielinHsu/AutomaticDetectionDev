# app：組裝根。只做組裝，不放業務規則與繪圖。
qt_add_executable(aoi main.cpp)
target_link_libraries(aoi PRIVATE aoi_presentation)
if(AOI_DEMO_SCENE)
  target_link_libraries(aoi PRIVATE aoi_presentation_dev)
  target_compile_definitions(aoi PRIVATE AOI_DEMO_SCENE=1)
endif()
aoi_set_warnings(aoi)
