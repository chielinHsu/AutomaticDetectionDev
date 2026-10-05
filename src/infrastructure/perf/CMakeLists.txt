add_library(aoi_perf STATIC PerfProbe.cpp)
target_link_libraries(aoi_perf PUBLIC aoi_application)
aoi_set_warnings(aoi_perf)
