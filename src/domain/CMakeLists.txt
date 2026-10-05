# domain 層只用 C++ 標準庫。S02 只有 ImageView 一個標頭；S03 起加入其餘型別。
add_library(aoi_domain INTERFACE)
target_include_directories(aoi_domain INTERFACE ${PROJECT_SOURCE_DIR}/src)
target_compile_features(aoi_domain INTERFACE cxx_std_20)
