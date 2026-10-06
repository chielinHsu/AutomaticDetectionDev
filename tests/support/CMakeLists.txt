# 測試用的共用程式（S06）：合成板子產生器（標準庫）與把它存成 PNG＋JSON 的工具（Qt）。不是產品的一部分。
add_library(aoi_test_support STATIC synthetic_board.cpp synthetic_board.h synthetic_board_io.cpp synthetic_board_io.h)
target_include_directories(aoi_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aoi_test_support PUBLIC aoi_domain aoi_image_qt Qt6::Core Qt6::Gui)
aoi_set_warnings(aoi_test_support)
