# infrastructure 層：實作 application 的 port。每個 adapter 一個 target，彼此不依賴。
add_subdirectory(backend_fake)
add_subdirectory(image_qt)
add_subdirectory(perf)
add_subdirectory(storage)
add_subdirectory(threading)
# OpenCV（S06 起：影像轉換與影像來源；S07 加範本建立器與搜尋器）。AOI_WITH_OPENCV=OFF 時整個 adapter 不建置。
if(AOI_WITH_OPENCV)
  add_subdirectory(backend_opencv)
endif()
# HALCON（S08：shape model 的範本建立器、搜尋器、樣本庫比對器、授權檢查）。AOI_WITH_HALCON=OFF 時整個 adapter 不建置。
if(AOI_WITH_HALCON)
  add_subdirectory(backend_halcon)
endif()
