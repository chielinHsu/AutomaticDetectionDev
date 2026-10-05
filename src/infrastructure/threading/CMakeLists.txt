add_library(aoi_threading STATIC ThreadPoolExecutor.cpp)
target_link_libraries(aoi_threading PUBLIC aoi_application)
aoi_set_warnings(aoi_threading)

# IExecutor 的 QThreadPool 版（S04）。只用 Qt Core；另外一個 target，所以只要 std::thread 版的使用者不必連 Qt。
add_library(aoi_threading_qt STATIC QtThreadPoolExecutor.cpp QtThreadPoolExecutor.h)
target_link_libraries(aoi_threading_qt PUBLIC aoi_application Qt6::Core)
aoi_set_warnings(aoi_threading_qt)
