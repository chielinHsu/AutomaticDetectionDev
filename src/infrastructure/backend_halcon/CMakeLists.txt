# HALCON 的 adapter（S08）：shape model 的範本建立器、搜尋器、樣本庫比對器、授權檢查。
# 只在 AOI_WITH_HALCON=ON 時建置（src/infrastructure/CMakeLists.txt）。HALCON 只能出現在這個資料夾（check-deps）。
# 標頭與匯入程式庫來自 cmake/FindHalconLocal.cmake（Halcon::HalconCpp，halconcpp.dll 延遲載入）。
add_library(aoi_backend_halcon STATIC
  HalconRuntime.cpp
  HalconLicense.cpp
  HalconErrors.cpp
  HalconImages.cpp
  HalconCancelWatch.cpp
  HalconTemplate.cpp
  HalconTemplateBuilder.cpp
  HalconSearchSteps.cpp
  HalconPatternSearcher.cpp
  HalconLibraryMatcher.cpp
  HalconBackend.cpp)
# version：GetFileVersionInfo（HalconRuntime.cpp 檢查 DLL 的版本）。
target_link_libraries(aoi_backend_halcon PUBLIC aoi_application Halcon::HalconCpp PRIVATE version)
aoi_set_warnings(aoi_backend_halcon)
