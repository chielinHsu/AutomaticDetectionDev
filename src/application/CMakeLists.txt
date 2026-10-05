# application 層：use case 與 port。只依賴 domain 與標準庫（不連 Qt、OpenCV、HALCON）。
add_library(aoi_application STATIC
  backend/BackendRegistry.cpp
  jobs/JobRunner.cpp
  usecases/GenerateObjectsFromSample.cpp
  usecases/ProposalView.cpp
  usecases/SimpleGrid.cpp)
target_link_libraries(aoi_application PUBLIC aoi_domain)
aoi_set_warnings(aoi_application)
