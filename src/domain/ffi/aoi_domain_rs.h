/* domain 層的 Rust 實作（rust/aoi_domain_rs）的 C ABI。ADR-031：只在 AOI_DOMAIN_IMPL=rust 的建置連結。
 *
 * 只 include C 的標準標頭；結構的欄位順序和 Rust 的 #[repr(C)] 相同（src/domain/lattice/non_max_suppression_rs.cpp
 * 用 static_assert 檢查大小）。改這裡就要改 rust/aoi_domain_rs/src/ffi.rs，並把 AOI_RS_ABI_VERSION 加 1。
 *
 * 規則：
 *   - 回傳 AoiRsStatus 的函式：AOI_RS_OK 才有輸出。panic 被擋在 Rust 這一側（AOI_RS_PANIC，是 bug）；
 *     配置失敗是 AOI_RS_NO_MEMORY（C++ 的包裝丟 std::bad_alloc）；null 指標是 AOI_RS_INVALID_ARGUMENT。
 *   - 誰配置誰釋放：aoi_rs_overlap_index_new 給的 handle 用 aoi_rs_overlap_index_free 釋放；
 *     aoi_rs_nms_keep 的輸出陣列由呼叫端配置。
 *   - 量法：0 ＝ 交集 ÷ 聯集（IoU），其他 ＝ 交集 ÷ 較小的面積。
 *   - 同一個 handle 不可以同時在兩條執行緒上呼叫 add（和 C++ 的 OverlapIndex 一樣不是執行緒安全的）；其他函式可重入。
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#define AOI_RS_ABI_VERSION 1u

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t AoiRsStatus;
#define AOI_RS_OK 0
#define AOI_RS_PANIC 1
#define AOI_RS_NO_MEMORY 2
#define AOI_RS_INVALID_ARGUMENT 3

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

#ifdef __cplusplus
}
#endif
