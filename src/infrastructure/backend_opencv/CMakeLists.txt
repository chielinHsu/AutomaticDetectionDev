# OpenCV 的 adapter。S06：影像轉換（cv::Mat ⇄ ImageView）與 IImageSource；S07 加範本建立器與搜尋器。
# 只在 AOI_WITH_OPENCV=ON 時建置（src/infrastructure/CMakeLists.txt）。OpenCV 只能出現在這個資料夾（check-deps）。
# 這台機器的 OpenCV 4.9.0 Windows 套件只有 opencv_world（一個 DLL），vc16（VS 2019）建置、由 VS 2022 的工具鏈連結。
add_library(aoi_backend_opencv STATIC OpenCvImages.cpp OpenCvImageSource.cpp)
target_link_libraries(aoi_backend_opencv PUBLIC aoi_application ${OpenCV_LIBS})
target_include_directories(aoi_backend_opencv SYSTEM PUBLIC ${OpenCV_INCLUDE_DIRS})
aoi_set_warnings(aoi_backend_opencv)
