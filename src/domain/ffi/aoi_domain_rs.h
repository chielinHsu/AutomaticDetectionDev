/* domain 層的 Rust 實作（rust/aoi_domain_rs）的 C ABI。ADR-031：只在 AOI_DOMAIN_IMPL=rust 的建置連結。
 *
 * 只 include C 的標準標頭；結構的欄位順序和 Rust 的 #[repr(C)] 相同（C++ 的包裝 non_max_suppression_rs.cpp、
 * lattice/lattice_rs_bridge.cpp、match_region_rs.cpp、repeatability_rs.cpp 用 static_assert 檢查大小，Rust 那一側也有）。改這裡就要改 rust/aoi_domain_rs/src/ffi.rs
 * （NMS）、ffi_lattice.rs（晶格）、ffi_match.rs（比對區）或 ffi_repeatability.rs（重複性），並把 AOI_RS_ABI_VERSION 加 1。版本 1：R05.1（NMS）；版本 2：R05.2（晶格）；
 * 版本 3：R05.3（比對區與自動擷取）；版本 4：R05.4（重複性的樣本統計）；版本 5：R07.2（aoi_rs_lattice_holes_are_backed 要讀 cells：
 * 索引區域不再是外接矩形）。
 *
 * 規則：
 *   - 回傳 AoiRsStatus 的函式：AOI_RS_OK 才有輸出。panic 被擋在 Rust 這一側（AOI_RS_PANIC，是 bug）；
 *     配置失敗是 AOI_RS_NO_MEMORY（C++ 的包裝丟 std::bad_alloc）；null 指標是 AOI_RS_INVALID_ARGUMENT。
 *   - 誰配置誰釋放：aoi_rs_overlap_index_new 給的 handle 用 aoi_rs_overlap_index_free 釋放；
 *     aoi_rs_nms_keep 的輸出陣列由呼叫端配置。晶格（R05.2）：長度有上限的輸出由呼叫端配置；長度不固定的輸出是 handle
 *     （*_get／*_model 讀到的指標指向 handle 裡的記憶體，*_free 之後不可以再用）。比對區（R05.3）同樣規則：遮罩與 runs 是 handle，
 *     maskFromRuns 的 bits 由呼叫端配置；輸入的遮罩、runs 與影像的像素直接借用（不複製）。
 *     重複性（R05.4）：值直接借用 std::span 的資料，輸出 AoiRsStatistics 由呼叫端配置（沒有 handle）。
 *   - 自訂的分群策略（AOI_RS_CLUSTER_CALLBACK）：回呼回報失敗、或輸出違反 IVectorClustering 的契約（群數比向量多、
 *     標號超出群數）時，整個呼叫回傳 AOI_RS_CALLBACK_FAILED（C++ 的包裝重新丟出回呼記下的例外，沒有記下就丟 std::logic_error）。
 *   - 量法：0 ＝ 交集 ÷ 聯集（IoU），其他 ＝ 交集 ÷ 較小的面積。
 *   - 同一個 handle 不可以同時在兩條執行緒上呼叫 add（和 C++ 的 OverlapIndex 一樣不是執行緒安全的）；其他函式可重入。
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define AOI_RS_ABI_VERSION 5u

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t AoiRsStatus;
#define AOI_RS_OK 0
#define AOI_RS_PANIC 1
#define AOI_RS_NO_MEMORY 2
#define AOI_RS_INVALID_ARGUMENT 3
#define AOI_RS_CALLBACK_FAILED 4

#define AOI_RS_MEASURE_IOU 0
#define AOI_RS_MEASURE_SMALLER 1

/* 旋轉矩形：中心、角度（度）、未旋轉時的寬高（和 aoi::RectObj 的欄位順序相同）。 */
typedef struct AoiRsRect {
    double x;
    double y;
    double angle_deg;
    double width;
    double height;
} AoiRsRect;

typedef struct AoiRsScoredBox {
    AoiRsRect rect;
    double score;
} AoiRsScoredBox;

typedef struct AoiRsOverlapIndex AoiRsOverlapIndex; /* 不透明的 handle */

uint32_t aoi_rs_abi_version(void);
/* 測試用：故意在 Rust 裡 panic，回傳 AOI_RS_PANIC（證明 panic 不跨過 FFI）。 */
AoiRsStatus aoi_rs_test_panic(void);

AoiRsStatus aoi_rs_overlap_area(const AoiRsRect* a, const AoiRsRect* b, double* out);
AoiRsStatus aoi_rs_overlap_ratio(const AoiRsRect* a, const AoiRsRect* b, int32_t measure, double* out);

AoiRsStatus aoi_rs_overlap_index_new(double cell_size, double max_overlap, int32_t measure, AoiRsOverlapIndex** out);
void aoi_rs_overlap_index_free(AoiRsOverlapIndex* index);
AoiRsStatus aoi_rs_overlap_index_overlaps_any(const AoiRsOverlapIndex* index, const AoiRsRect* rect, int32_t* out);
AoiRsStatus aoi_rs_overlap_index_add(AoiRsOverlapIndex* index, const AoiRsRect* rect);
AoiRsStatus aoi_rs_overlap_index_size(const AoiRsOverlapIndex* index, size_t* out);

/* 留下的框的索引寫進 out_indices（至少 count 個），個數寫進 *out_count；排序與壓制的規則見 non_max_suppression.h。 */
AoiRsStatus aoi_rs_nms_keep(const AoiRsScoredBox* boxes, size_t count, double max_overlap, int32_t measure, size_t* out_indices,
                            size_t* out_count);

/* ---- 晶格（R05.2）：lattice/ 的公開標頭的 Rust 實作 ---- */

#define AOI_RS_CLUSTER_GRID_VOTE 0 /* parameter ＝ binFraction */
#define AOI_RS_CLUSTER_GREEDY 1    /* parameter ＝ tolerance */
#define AOI_RS_CLUSTER_CALLBACK 2  /* callback＋context（C++ 的 IVectorClustering） */

#define AOI_RS_PREDICT_IN_RANGE 0
#define AOI_RS_PREDICT_HOLES 1
#define AOI_RS_PREDICT_ALL_IN_BOUNDS 2

typedef struct AoiRsPoint {
    double x;
    double y;
} AoiRsPoint;

typedef struct AoiRsCell {
    int32_t i;
    int32_t j;
} AoiRsCell;

typedef struct AoiRsBasis {
    AoiRsPoint origin;
    AoiRsPoint u;
    AoiRsPoint v;
    int32_t has_v;
} AoiRsBasis;

/* Rect：影像像素，右、下邊不含。 */
typedef struct AoiRsBounds {
    double x;
    double y;
    double w;
    double h;
} AoiRsBounds;

/* LatticeAccumulator 的狀態（總和相對於參考點與參考格子）。 */
typedef struct AoiRsLatticeSums {
    AoiRsPoint reference_point;
    AoiRsCell reference_cell;
    int64_t count;
    double n, si, sj, sii, sij, sjj;
    double sx, six, sjx, sxx;
    double sy, siy, sjy, syy;
} AoiRsLatticeSums;

/* LatticeOptions（欄位的意義見 lattice_estimator.h）。 */
typedef struct AoiRsLatticeOptions {
    double inlier_fraction;
    size_t neighbors;
    size_t max_samples;
    double support_fraction;
    int32_t ransac_trials;
    int32_t ransac_index_radius;
    int32_t max_gap_cells;
    size_t min_second_row_inliers;
    double min_row_consistency;
    size_t min_inliers;
    double min_confidence;
    double check_below;
    size_t max_lattices;
    size_t min_extra_lattice_inliers;
    uint64_t seed;
} AoiRsLatticeOptions;

typedef struct AoiRsVectorCluster {
    AoiRsPoint center;
    size_t members;
} AoiRsVectorCluster;

/* 自訂分群的回呼：count 個向量分群，群寫進 clusters（容量 count）、群數寫進 *cluster_count、每個向量的標號寫進 labels。
 * 成功回傳 AOI_RS_OK；其他值讓整個呼叫回傳 AOI_RS_CALLBACK_FAILED。不可以讓 C++ 的例外穿過回呼。 */
typedef AoiRsStatus (*AoiRsClusterFn)(void* context, const AoiRsPoint* vectors, size_t count, double scale,
                                      AoiRsVectorCluster* clusters, size_t* cluster_count, int32_t* labels);

typedef struct AoiRsClustering {
    int32_t kind; /* AOI_RS_CLUSTER_* */
    double parameter;
    AoiRsClusterFn callback;
    void* context;
} AoiRsClustering;

typedef struct AoiRsNeighbor {
    size_t index;
    double distance;
} AoiRsNeighbor;

typedef struct AoiRsPredictedCell {
    AoiRsCell cell;
    AoiRsPoint position;
} AoiRsPredictedCell;

/* LatticeModel：輸入時陣列指標指向呼叫端的陣列；aoi_rs_lattice_fit_model 讀出時指向 handle 裡的陣列。 */
typedef struct AoiRsLatticeModel {
    AoiRsBasis basis;
    double rms_px;
    double tolerance_px;
    double coverage;
    double consistency;
    double confidence;
    int32_t suggest_check;
    int32_t min_i, max_i, min_j, max_j;
    AoiRsLatticeSums equations;
    const size_t* inliers;
    size_t inlier_count;
    const AoiRsCell* cells;
    size_t cell_count;
} AoiRsLatticeModel;

/* detail::LatticeCandidate。 */
typedef struct AoiRsLatticeCandidate {
    AoiRsBasis basis;
    double tolerance;
    const size_t* members;
    size_t member_count;
    const AoiRsCell* cells;
    size_t cell_count;
} AoiRsLatticeCandidate;

typedef struct AoiRsSpatialGrid AoiRsSpatialGrid;     /* 不透明的 handle（自己保存一份點） */
typedef struct AoiRsPointList AoiRsPointList;         /* 不透明的 handle */
typedef struct AoiRsPredictedList AoiRsPredictedList; /* 不透明的 handle */
typedef struct AoiRsLatticeFit AoiRsLatticeFit;       /* 不透明的 handle */

/* LatticeAccumulator：weight > 0 加一個點、≤ 0 拿掉一個點。不配置記憶體。 */
AoiRsStatus aoi_rs_lattice_sums_accumulate(AoiRsLatticeSums* sums, AoiRsCell cell, AoiRsPoint p, double weight);
AoiRsStatus aoi_rs_lattice_sums_two_dimensional(const AoiRsLatticeSums* sums, int32_t* out);
/* 解不出時 *solved 是 0（*out 不變）。 */
AoiRsStatus aoi_rs_lattice_sums_solve(const AoiRsLatticeSums* sums, AoiRsBasis* out, int32_t* solved);
AoiRsStatus aoi_rs_lattice_sums_residual_sum_of_squares(const AoiRsLatticeSums* sums, const AoiRsBasis* basis, double* out);

/* SpatialGrid。nearest 的 out 由呼叫端配置 capacity 個（至少 min(k, 點數)）。 */
AoiRsStatus aoi_rs_spatial_grid_new(const AoiRsPoint* points, size_t count, double cell_size, AoiRsSpatialGrid** out);
void aoi_rs_spatial_grid_free(AoiRsSpatialGrid* grid);
AoiRsStatus aoi_rs_spatial_grid_cell_size(const AoiRsSpatialGrid* grid, double* out);
AoiRsStatus aoi_rs_spatial_grid_nearest(const AoiRsSpatialGrid* grid, size_t self_index, size_t k, double min_distance,
                                        AoiRsNeighbor* out, size_t capacity, size_t* out_count);

/* IVectorClustering::cluster：clusters、labels 由呼叫端各配置 count 個。 */
AoiRsStatus aoi_rs_cluster_vectors(const AoiRsClustering* clustering, const AoiRsPoint* vectors, size_t count, double scale,
                                   AoiRsVectorCluster* clusters, size_t* cluster_count, int32_t* labels);

/* detail::BasisFinder。 */
AoiRsStatus aoi_rs_basis_directions(const AoiRsLatticeOptions* options, const AoiRsClustering* clustering, const AoiRsPoint* points,
                                    size_t count, double min_distance, AoiRsPointList** out);
AoiRsStatus aoi_rs_basis_second_direction(const AoiRsLatticeOptions* options, const AoiRsClustering* clustering,
                                          const AoiRsPoint* points, size_t count, double min_distance, AoiRsPoint u,
                                          const AoiRsPoint* directions, size_t direction_count, AoiRsPoint* out, int32_t* found);
AoiRsStatus aoi_rs_point_list_get(const AoiRsPointList* list, const AoiRsPoint** data, size_t* count);
void aoi_rs_point_list_free(AoiRsPointList* list);

/* detail::largestComponent（out_indices 由呼叫端配置 count 個）與接受規則（which：0 ＝ twoDimensionalIsValid、1 ＝ enoughEvidence）。
 * holes_are_backed 讀 model 的 inlier_count 與 cells（R07.2 起索引區域要看內點的格子）；inliers 不讀。 */
AoiRsStatus aoi_rs_lattice_largest_component(const AoiRsCell* cells, size_t count, int32_t link, int32_t two_dimensional,
                                             size_t* out_indices, size_t* out_count);
AoiRsStatus aoi_rs_lattice_candidate_rule(int32_t which, const AoiRsLatticeCandidate* candidate, const AoiRsPoint* points,
                                          size_t count, const AoiRsLatticeOptions* options, int32_t* out);
AoiRsStatus aoi_rs_lattice_holes_are_backed(const AoiRsLatticeModel* model, int32_t* out);
AoiRsStatus aoi_rs_lattice_consistency(const AoiRsLatticeModel* model, const AoiRsPoint* points, size_t count, double* out);

/* LatticeEstimator：最多 max_models 組晶格。reason 是 NoLatticeReason 的數值（None ＝ 0 … LowConfidence ＝ 5）。 */
AoiRsStatus aoi_rs_lattice_fit(const AoiRsLatticeOptions* options, const AoiRsClustering* clustering, const AoiRsPoint* points,
                               size_t count, size_t max_models, AoiRsLatticeFit** out);
AoiRsStatus aoi_rs_lattice_fit_summary(const AoiRsLatticeFit* fit, int32_t* reason, double* rejected_confidence, size_t* model_count);
AoiRsStatus aoi_rs_lattice_fit_model(const AoiRsLatticeFit* fit, size_t index, AoiRsLatticeModel* out);
void aoi_rs_lattice_fit_free(AoiRsLatticeFit* fit);

/* LatticePredictor：mode 是 AOI_RS_PREDICT_*（margin 只用在 IN_RANGE）。 */
AoiRsStatus aoi_rs_lattice_predict(const AoiRsLatticeModel* model, AoiRsBounds bounds, int32_t mode, int32_t margin, size_t max_count,
                                   AoiRsPredictedList** out);
AoiRsStatus aoi_rs_predicted_list_get(const AoiRsPredictedList* list, const AoiRsPredictedCell** data, size_t* count);
void aoi_rs_predicted_list_free(AoiRsPredictedList* list);
AoiRsStatus aoi_rs_lattice_order_near_to_far(AoiRsPredictedCell* cells, size_t count, AoiRsPoint anchor);

/* ---- 比對區與自動擷取（R05.3）：match_region.h、feature_extract.h 的 Rust 實作 ---- */

/* PixelFormat 的數值（feature_extract_rs.cpp 用 static_assert 檢查）。 */
#define AOI_RS_PIXEL_GRAY8 0
#define AOI_RS_PIXEL_RGB24 1
#define AOI_RS_PIXEL_BGRX32 2
#define AOI_RS_PIXEL_BGR24 3
#define AOI_RS_PIXEL_GRAY16 4

/* MaskRun：第 row 列的 [col_begin, col_end)（和 aoi::MaskRun 逐欄相同，C++ 直接交出 std::vector 的位址）。 */
typedef struct AoiRsMaskRun {
    int32_t row;
    int32_t col_begin;
    int32_t col_end;
} AoiRsMaskRun;

/* PixelMask 的檢視：輸入時 bits 指向呼叫端的陣列；aoi_rs_pixel_mask_get 讀出時指向 handle 裡的陣列。 */
typedef struct AoiRsPixelMaskView {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    const uint8_t* bits;
    size_t bit_count;
} AoiRsPixelMaskView;

/* MatchRegion 的矩形（µm，比對區中心相對主體中心）。 */
typedef struct AoiRsRegionUm {
    double center_x_um;
    double center_y_um;
    double width_um;
    double height_um;
} AoiRsRegionUm;

/* MatchRegion：遮罩的 runs 指向呼叫端的陣列。 */
typedef struct AoiRsMatchRegion {
    AoiRsRegionUm rect;
    int32_t mask_width;
    int32_t mask_height;
    const AoiRsMaskRun* runs;
    size_t run_count;
} AoiRsMatchRegion;

/* ImageView 的像素（借用）；effective_bits 是 C++ 的 effectiveBits(image)（image_depth 留在 C++）。 */
typedef struct AoiRsImage {
    const uint8_t* data;
    int32_t width;
    int32_t height;
    int32_t stride_bytes;
    int32_t format; /* AOI_RS_PIXEL_* */
    int32_t effective_bits;
} AoiRsImage;

typedef struct AoiRsPixelMask AoiRsPixelMask; /* 不透明的 handle（遮罩） */
typedef struct AoiRsMaskRuns AoiRsMaskRuns;   /* 不透明的 handle（runs） */

/* 輸出的 handle 是 null ＝ 空的遮罩（PixelMask{}）或沒有遮罩。 */
AoiRsStatus aoi_rs_pixel_mask_get(const AoiRsPixelMask* mask, AoiRsPixelMaskView* out);
void aoi_rs_pixel_mask_free(AoiRsPixelMask* mask);
AoiRsStatus aoi_rs_mask_runs_get(const AoiRsMaskRuns* runs, int32_t* width, int32_t* height, const AoiRsMaskRun** data, size_t* count);
void aoi_rs_mask_runs_free(AoiRsMaskRuns* runs);

/* PixelMask::count、runsFromMask（*out 一定是 handle）。 */
AoiRsStatus aoi_rs_mask_count(const AoiRsPixelMaskView* mask, size_t* out);
AoiRsStatus aoi_rs_runs_from_mask(const AoiRsPixelMaskView* mask, AoiRsMaskRuns** out);
/* maskFromRuns 的 bits：width、height > 0，out_bits 由呼叫端配置 width × height 個（out_len），整個寫過。 */
AoiRsStatus aoi_rs_mask_from_runs(const AoiRsMaskRun* runs, size_t count, int32_t width, int32_t height, uint8_t* out_bits,
                                  size_t out_len);
/* matchSpecFor：region 是 null ＝ 沒有比對區；*has_rect 是 0 時 *rect 不變；*mask 是 null ＝ 沒有遮罩。mode 由呼叫端照抄。 */
AoiRsStatus aoi_rs_match_spec_for(const AoiRsMatchRegion* region, AoiRsPoint body_center, double um_per_pixel, int32_t* has_rect,
                                  AoiRsBounds* rect, AoiRsPixelMask** mask);
/* matchSettingsFromPixels：mask 是 null ＝ 沒有遮罩；*has_region 是 0 時 *region 不變；*mask_runs 是 null ＝ 比對區沒有遮罩。 */
AoiRsStatus aoi_rs_match_settings_from_pixels(AoiRsBounds region_px, const AoiRsPixelMaskView* mask, AoiRsPoint body_center,
                                              double um_per_pixel, int32_t* has_region, AoiRsRegionUm* region, AoiRsMaskRuns** mask_runs);
/* pickBrightBlob：找到時 *found 是 1、*blob 是連通塊；找不到（含無效的影像）時 *found 是 0、*blob 是 null。 */
AoiRsStatus aoi_rs_pick_bright_blob(const AoiRsImage* image, AoiRsBounds within, AoiRsPoint click, int32_t* found, int32_t* threshold8,
                                    AoiRsPixelMask** blob);
/* growMask：*out 是 null ＝ 空的遮罩。 */
AoiRsStatus aoi_rs_grow_mask(const AoiRsPixelMaskView* blob, int32_t radius, AoiRsBounds clip, AoiRsPixelMask** out);

/* ---- 重複性的樣本統計（R05.4，ffi_repeatability.rs） ---- */

/* aoi::repeatability::Statistics：std::optional<double> 拆成 has_*（0 ＝ nullopt，那時值是 0）與值。由呼叫端配置。 */
typedef struct AoiRsStatistics {
    size_t count;
    size_t expected;
    int32_t has_minimum;
    int32_t has_maximum;
    int32_t has_average;
    int32_t has_range;
    int32_t has_deviation;
    int32_t has_pt;
    double minimum;
    double maximum;
    double average;
    double range;
    double deviation;
    double pt;
} AoiRsStatistics;

/* summarize：values 指向 count 個 double（count 是 0 時可以是 null；直接借用 std::span 的資料）；has_tolerance 是 0 ＝ 沒有 SPEC。 */
AoiRsStatus aoi_rs_repeatability_summarize(const double* values, size_t count, size_t expected, int32_t has_tolerance, double tolerance,
                                           AoiRsStatistics* out);

#ifdef __cplusplus
}
#endif
