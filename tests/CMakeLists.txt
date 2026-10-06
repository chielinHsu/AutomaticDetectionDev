add_subdirectory(support)  # S06：合成板子產生器（其他測試會用，所以先加）
add_subdirectory(core)
add_subdirectory(domain)
add_subdirectory(infra)
add_subdirectory(backend)  # S07：OpenCV back end（AOI_WITH_OPENCV=ON 才有）
add_subdirectory(ui)
