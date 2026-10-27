# domain 層只用 C++ 標準庫（CONFIG -= qt：誤 include Qt 會編不過）。S05 起有晶格估計的 .cpp（lattice/）；其餘多半是只有標頭的型別。
# ADR-028：比對區（match_region）與自動擷取（feature_extract）。
# S06：影像緩衝（image_buffer）、複製計數（image_copy）、位元深度（image_depth，12 bit 的影像）。
# ADR-031（R05）：搬進 Rust 的演算法有兩份實作，依 AOI_DOMAIN_IMPL 選一份；標頭兩種建置都一樣。
#   cpp：lattice/legacy/*_legacy.cpp（aoi::legacy）＋ 轉呼叫它的 .cpp；rust：轉呼叫 Rust（ffi/aoi_domain_rs.h）的 *_rs.cpp
#   （晶格共用的轉換在 lattice_rs_bridge.cpp）。R05.1：NMS；R05.2：晶格；R05.3：比對區與自動擷取（C++ 在 legacy/，
#   operator== 在 match_region_equality.cpp，兩種建置都編）；R05.4：重複性的 summarize（C++ 在 legacy/，
#   valueOf、defaultMetrics 在 repeatability_types.cpp，兩種建置都編）。

AOI_MODULE = domain
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/domain
SOURCES += \
    $$DIR/repeatability_types.cpp \
    $$DIR/image_buffer.cpp \
    $$DIR/image_copy.cpp \
    $$DIR/image_depth.cpp \
    $$DIR/match_region_equality.cpp

equals(AOI_DOMAIN_IMPL, rust) {
    SOURCES += \
        $$DIR/feature_extract_rs.cpp \
        $$DIR/match_region_rs.cpp \
        $$DIR/repeatability_rs.cpp \
        $$DIR/lattice/basis_finder_rs.cpp \
        $$DIR/lattice/cell_components_rs.cpp \
        $$DIR/lattice/lattice_acceptance_rs.cpp \
        $$DIR/lattice/lattice_accumulator_rs.cpp \
        $$DIR/lattice/lattice_estimator_rs.cpp \
        $$DIR/lattice/lattice_predictor_rs.cpp \
        $$DIR/lattice/non_max_suppression_rs.cpp \
        $$DIR/lattice/spatial_grid_rs.cpp \
        $$DIR/lattice/vector_clustering_rs.cpp \
        $$DIR/lattice/lattice_rs_bridge.cpp
} else {
    SOURCES += \
        $$DIR/legacy/feature_extract_legacy.cpp \
        $$DIR/legacy/match_region_legacy.cpp \
        $$DIR/legacy/repeatability_legacy.cpp \
        $$DIR/feature_extract.cpp \
        $$DIR/match_region.cpp \
        $$DIR/repeatability.cpp \
        $$DIR/lattice/legacy/basis_finder_legacy.cpp \
        $$DIR/lattice/legacy/cell_components_legacy.cpp \
        $$DIR/lattice/legacy/lattice_acceptance_legacy.cpp \
        $$DIR/lattice/legacy/lattice_accumulator_legacy.cpp \
        $$DIR/lattice/legacy/lattice_estimator_legacy.cpp \
        $$DIR/lattice/legacy/lattice_predictor_legacy.cpp \
        $$DIR/lattice/legacy/non_max_suppression_legacy.cpp \
        $$DIR/lattice/legacy/spatial_grid_legacy.cpp \
        $$DIR/lattice/legacy/vector_clustering_legacy.cpp \
        $$DIR/lattice/basis_finder.cpp \
        $$DIR/lattice/cell_components.cpp \
        $$DIR/lattice/lattice_acceptance.cpp \
        $$DIR/lattice/lattice_accumulator.cpp \
        $$DIR/lattice/lattice_estimator.cpp \
        $$DIR/lattice/lattice_predictor.cpp \
        $$DIR/lattice/non_max_suppression.cpp \
        $$DIR/lattice/spatial_grid.cpp \
        $$DIR/lattice/vector_clustering.cpp
}

HEADERS += \
    $$DIR/ffi/aoi_domain_rs.h \
    $$DIR/legacy/feature_extract_legacy.h \
    $$DIR/legacy/match_region_legacy.h \
    $$DIR/legacy/repeatability_legacy.h \
    $$DIR/match_region_rs_bridge.h \
    $$DIR/lattice/legacy/basis_finder_legacy.h \
    $$DIR/lattice/legacy/cell_components_legacy.h \
    $$DIR/lattice/legacy/lattice_acceptance_legacy.h \
    $$DIR/lattice/legacy/lattice_accumulator_legacy.h \
    $$DIR/lattice/legacy/lattice_estimator_legacy.h \
    $$DIR/lattice/legacy/lattice_predictor_legacy.h \
    $$DIR/lattice/legacy/non_max_suppression_legacy.h \
    $$DIR/lattice/legacy/spatial_grid_legacy.h \
    $$DIR/lattice/legacy/vector_clustering_legacy.h \
    $$DIR/lattice/lattice_accumulator_access.h \
    $$DIR/lattice/lattice_rs_bridge.h \
    $$DIR/repeatability.h \
    $$DIR/box_expander.h \
    $$DIR/component.h \
    $$DIR/feature_extract.h \
    $$DIR/geometry.h \
    $$DIR/image_buffer.h \
    $$DIR/image_copy.h \
    $$DIR/image_depth.h \
    $$DIR/image_view.h \
    $$DIR/lattice/basis_finder.h \
    $$DIR/lattice/cell_components.h \
    $$DIR/lattice/flat_map.h \
    $$DIR/lattice/lattice_acceptance.h \
    $$DIR/lattice/lattice_accumulator.h \
    $$DIR/lattice/lattice_basis.h \
    $$DIR/lattice/lattice_confidence.h \
    $$DIR/lattice/lattice_detail.h \
    $$DIR/lattice/lattice_estimator.h \
    $$DIR/lattice/lattice_model.h \
    $$DIR/lattice/lattice_predictor.h \
    $$DIR/lattice/non_max_suppression.h \
    $$DIR/lattice/spatial_grid.h \
    $$DIR/lattice/vector_clustering.h \
    $$DIR/match.h \
    $$DIR/match_region.h \
    $$DIR/object_state.h \
    $$DIR/recipe.h \
    $$DIR/search_params.h
