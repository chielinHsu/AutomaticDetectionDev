# domain 層只用 C++ 標準庫，目前全是標頭（S05 加入晶格後才會有 .cpp，那時改成 STATIC）。
add_library(aoi_domain INTERFACE)
target_include_directories(aoi_domain INTERFACE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(aoi_domain INTERFACE cxx_std_20)
