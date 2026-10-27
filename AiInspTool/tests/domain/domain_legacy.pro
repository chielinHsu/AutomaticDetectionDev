# 差異測試的對照組（ADR-031）：AOI_DOMAIN_IMPL=rust 時，domain 的 C++ 實作（命名空間 aoi::legacy）編成這個程式庫，
# 只有 tst_domain_rs_diff 連結它（同一個執行檔裡同時有 Rust 與 C++ 的實作，逐筆比對）。
# AOI_DOMAIN_IMPL=cpp 時這些檔案編在 aoi_domain 裡，這個程式庫不建置（tests/tests.pro）。

AOI_MODULE = domain_legacy
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/domain
SOURCES += \
    $$DIR/legacy/feature_extract_legacy.cpp \
    $$DIR/legacy/match_region_legacy.cpp \
    $$DIR/legacy/repeatability_legacy.cpp \
    $$DIR/lattice/legacy/basis_finder_legacy.cpp \
    $$DIR/lattice/legacy/cell_components_legacy.cpp \
    $$DIR/lattice/legacy/lattice_acceptance_legacy.cpp \
    $$DIR/lattice/legacy/lattice_accumulator_legacy.cpp \
    $$DIR/lattice/legacy/lattice_estimator_legacy.cpp \
    $$DIR/lattice/legacy/lattice_predictor_legacy.cpp \
    $$DIR/lattice/legacy/non_max_suppression_legacy.cpp \
    $$DIR/lattice/legacy/spatial_grid_legacy.cpp \
    $$DIR/lattice/legacy/vector_clustering_legacy.cpp
HEADERS += \
    $$DIR/legacy/feature_extract_legacy.h \
    $$DIR/legacy/match_region_legacy.h \
    $$DIR/legacy/repeatability_legacy.h \
    $$DIR/lattice/legacy/basis_finder_legacy.h \
    $$DIR/lattice/legacy/cell_components_legacy.h \
    $$DIR/lattice/legacy/lattice_acceptance_legacy.h \
    $$DIR/lattice/legacy/lattice_accumulator_legacy.h \
    $$DIR/lattice/legacy/lattice_estimator_legacy.h \
    $$DIR/lattice/legacy/lattice_predictor_legacy.h \
    $$DIR/lattice/legacy/non_max_suppression_legacy.h \
    $$DIR/lattice/legacy/spatial_grid_legacy.h \
    $$DIR/lattice/legacy/vector_clustering_legacy.h
