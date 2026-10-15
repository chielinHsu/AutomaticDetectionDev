# domain 層只用 C++ 標準庫（CONFIG -= qt：誤 include Qt 會編不過）。S05 起有晶格估計的 .cpp（lattice/）；其餘多半是只有標頭的型別。
# ADR-026：比對區（match_region）與自動擷取（feature_extract）。
# S06：影像緩衝（image_buffer）、複製計數（image_copy）、位元深度（image_depth，12 bit 的影像）。

AOI_MODULE = domain
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/domain
SOURCES += \
    $$DIR/feature_extract.cpp \
    $$DIR/image_buffer.cpp \
    $$DIR/image_copy.cpp \
    $$DIR/image_depth.cpp \
    $$DIR/lattice/basis_finder.cpp \
    $$DIR/lattice/cell_components.cpp \
    $$DIR/lattice/lattice_acceptance.cpp \
    $$DIR/lattice/lattice_accumulator.cpp \
    $$DIR/lattice/lattice_estimator.cpp \
    $$DIR/lattice/lattice_predictor.cpp \
    $$DIR/lattice/non_max_suppression.cpp \
    $$DIR/lattice/spatial_grid.cpp \
    $$DIR/lattice/vector_clustering.cpp \
    $$DIR/match_region.cpp

HEADERS += \
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
