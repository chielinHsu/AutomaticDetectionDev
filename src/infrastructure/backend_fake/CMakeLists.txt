# 假 back end：只用標準庫，不看影像內容。見 FakeGrid.h。
add_library(aoi_backend_fake STATIC FakeBackend.cpp FakeGrid.cpp)
target_link_libraries(aoi_backend_fake PUBLIC aoi_application)
aoi_set_warnings(aoi_backend_fake)
