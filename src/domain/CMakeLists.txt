# domain 層只用 C++ 標準庫。S05 起有晶格估計的 .cpp（lattice/），所以是 STATIC；其餘仍是只有標頭的型別。
add_library(aoi_domain STATIC
  lattice/basis_finder.cpp
  lattice/cell_components.cpp
  lattice/lattice_acceptance.cpp
  lattice/lattice_accumulator.cpp
  lattice/lattice_estimator.cpp
  lattice/lattice_predictor.cpp
  lattice/non_max_suppression.cpp
  lattice/spatial_grid.cpp
  lattice/vector_clustering.cpp)
target_include_directories(aoi_domain PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_features(aoi_domain PUBLIC cxx_std_20)
aoi_set_warnings(aoi_domain)
