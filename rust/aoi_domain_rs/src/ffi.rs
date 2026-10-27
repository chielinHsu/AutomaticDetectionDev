//! C ABI（ADR-031 第 2–4 點）。標頭是 `src/domain/ffi/aoi_domain_rs.h`：改這裡就要改那裡（`AOI_RS_ABI_VERSION` 也要加 1）。
//!
//! 規則：
//! - 每個函式用 `catch_unwind` 把 panic 擋在 Rust 這一側，回傳 `AOI_RS_PANIC`（panic 是 bug；`aoi_rs_test_panic` 證明擋得住）。
//! - 配置失敗回傳 `AOI_RS_NO_MEMORY`（用 `try_reserve`，不結束行程），C++ 的包裝照原本的契約丟 `std::bad_alloc`。
//! - 誰配置誰釋放：`OverlapIndex` 由 `aoi_rs_overlap_index_new` 配置、`aoi_rs_overlap_index_free` 釋放；
//!   輸出大小有上限的陣列（`aoi_rs_nms_keep` 的索引）由呼叫端配置，Rust 只寫進去。
//! - 量法的整數：0 ＝ IoU，其他 ＝ 交集 ÷ 較小的面積（和 C++ 的 `measure == IntersectionOverUnion ? … : …` 相同）。

use std::panic::{AssertUnwindSafe, catch_unwind};

use crate::geometry::RectObj;
use crate::nms::{self, OverlapIndex, OverlapMeasure, ScoredBox};

pub const AOI_RS_OK: i32 = 0;
pub const AOI_RS_PANIC: i32 = 1;
pub const AOI_RS_NO_MEMORY: i32 = 2;
pub const AOI_RS_INVALID_ARGUMENT: i32 = 3;

/// C 標頭的 `AOI_RS_ABI_VERSION`：C++ 包裝在第一次呼叫前比對，連到舊的程式庫時明確失敗。
pub const AOI_RS_ABI_VERSION: u32 = 1;

fn guard(f: impl FnOnce() -> i32) -> i32 {
    catch_unwind(AssertUnwindSafe(f)).unwrap_or(AOI_RS_PANIC)
}

fn measure_of(measure: i32) -> OverlapMeasure {
    if measure == 0 {
        OverlapMeasure::IntersectionOverUnion
    } else {
        OverlapMeasure::IntersectionOverSmaller
    }
}

/// 這個程式庫的 ABI 版本。
#[unsafe(no_mangle)]
pub extern "C" fn aoi_rs_abi_version() -> u32 {
    AOI_RS_ABI_VERSION
}

/// 測試用：故意 panic，證明 panic 不會跨過 FFI（回傳 `AOI_RS_PANIC`）。
#[unsafe(no_mangle)]
pub extern "C" fn aoi_rs_test_panic() -> i32 {
    guard(|| panic!("aoi_rs_test_panic: intentional panic (the C++ side must see AOI_RS_PANIC)"))
}

/// 兩個旋轉矩形的重疊面積。
///
/// # Safety
/// `a`、`b` 指向有效的 `RectObj`；`out` 指向可寫的 `double`。任一個是 null 時回傳 `AOI_RS_INVALID_ARGUMENT`。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_overlap_area(a: *const RectObj, b: *const RectObj, out: *mut f64) -> i32 {
    guard(|| {
        if a.is_null() || b.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 呼叫端保證三個指標有效（上面已排除 null）。
        unsafe { *out = nms::overlap_area(&*a, &*b) };
        AOI_RS_OK
    })
}

/// 重疊比例（0..1）。
///
/// # Safety
/// 同 `aoi_rs_overlap_area`。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_overlap_ratio(a: *const RectObj, b: *const RectObj, measure: i32, out: *mut f64) -> i32 {
    guard(|| {
        if a.is_null() || b.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 呼叫端保證三個指標有效（上面已排除 null）。
        unsafe { *out = nms::overlap_ratio(&*a, &*b, measure_of(measure)) };
        AOI_RS_OK
    })
}

/// 建立空間索引；成功時 `*out` 是新的 handle（用 `aoi_rs_overlap_index_free` 釋放）。
///
/// # Safety
/// `out` 指向可寫的指標。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_overlap_index_new(cell_size: f64, max_overlap: f64, measure: i32, out: *mut *mut OverlapIndex) -> i32 {
    guard(|| {
        if out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        match OverlapIndex::new(cell_size, max_overlap, measure_of(measure)) {
            Ok(index) => {
                // SAFETY: out 有效（上面已排除 null）。
                unsafe { *out = Box::into_raw(Box::new(index)) };
                AOI_RS_OK
            }
            Err(_) => AOI_RS_NO_MEMORY,
        }
    })
}

/// 釋放 `aoi_rs_overlap_index_new` 建立的 handle。null 不做事。
///
/// # Safety
/// `index` 是 `aoi_rs_overlap_index_new` 給的、還沒釋放的 handle（或 null）。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_overlap_index_free(index: *mut OverlapIndex) {
    let _ = guard(|| {
        if !index.is_null() {
            // SAFETY: index 由 Box::into_raw 產生、只釋放一次（呼叫端保證）。
            drop(unsafe { Box::from_raw(index) });
        }
        AOI_RS_OK
    });
}

/// 和已加入的任何一個框的重疊比例 > max_overlap？`*out` 是 1 或 0。
///
/// # Safety
/// `index` 是有效的 handle；`rect` 指向有效的 `RectObj`；`out` 指向可寫的 `int32_t`。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_overlap_index_overlaps_any(index: *const OverlapIndex, rect: *const RectObj, out: *mut i32) -> i32 {
    guard(|| {
        if index.is_null() || rect.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 三個指標有效（上面已排除 null）。
        unsafe { *out = i32::from((*index).overlaps_any(&*rect)) };
        AOI_RS_OK
    })
}

/// 加入一個框（不是有限值的框不加）。配置失敗時回傳 `AOI_RS_NO_MEMORY`，索引不變。
///
/// # Safety
/// `index` 是有效的 handle；`rect` 指向有效的 `RectObj`。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_overlap_index_add(index: *mut OverlapIndex, rect: *const RectObj) -> i32 {
    guard(|| {
        if index.is_null() || rect.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 兩個指標有效（上面已排除 null），index 沒有別人同時在用（呼叫端保證）。
        match unsafe { (*index).add(&*rect) } {
            Ok(()) => AOI_RS_OK,
            Err(_) => AOI_RS_NO_MEMORY,
        }
    })
}

/// 已加入的框數。
///
/// # Safety
/// `index` 是有效的 handle；`out` 指向可寫的 `size_t`。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_overlap_index_size(index: *const OverlapIndex, out: *mut usize) -> i32 {
    guard(|| {
        if index.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 兩個指標有效（上面已排除 null）。
        unsafe { *out = (*index).len() };
        AOI_RS_OK
    })
}

/// 非極大值抑制：留下的框的索引寫進 `out_indices`（呼叫端配置，至少 `count` 個），個數寫進 `*out_count`。
///
/// # Safety
/// `boxes` 指向 `count` 個 `ScoredBox`（`count` 是 0 時可以是 null）；`out_indices` 可寫 `count` 個 `size_t`
/// （`count` 是 0 時可以是 null）；`out_count` 指向可寫的 `size_t`。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_nms_keep(
    boxes: *const ScoredBox,
    count: usize,
    max_overlap: f64,
    measure: i32,
    out_indices: *mut usize,
    out_count: *mut usize,
) -> i32 {
    guard(|| {
        if out_count.is_null() || (count > 0 && (boxes.is_null() || out_indices.is_null())) {
            return AOI_RS_INVALID_ARGUMENT;
        }
        let input: &[ScoredBox] = if count == 0 {
            &[]
        } else {
            // SAFETY: boxes 指向 count 個元素（呼叫端保證，上面已排除 null）。
            unsafe { std::slice::from_raw_parts(boxes, count) }
        };
        match nms::keep(input, max_overlap, measure_of(measure)) {
            Ok(kept) => {
                if !kept.is_empty() {
                    // SAFETY: out_indices 可寫 count 個元素，kept.len() ≤ count（每個輸入最多留一次）。
                    unsafe { std::ptr::copy_nonoverlapping(kept.as_ptr(), out_indices, kept.len()) };
                }
                // SAFETY: out_count 有效（上面已排除 null）。
                unsafe { *out_count = kept.len() };
                AOI_RS_OK
            }
            Err(_) => AOI_RS_NO_MEMORY,
        }
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn panic_does_not_cross_the_ffi() {
        assert_eq!(aoi_rs_test_panic(), AOI_RS_PANIC);
    }

    #[test]
    fn null_pointers_are_rejected() {
        let mut out = 0.0;
        // SAFETY: 測試 null 的處理；不會解參考。
        unsafe {
            assert_eq!(
                aoi_rs_overlap_area(std::ptr::null(), std::ptr::null(), &mut out),
                AOI_RS_INVALID_ARGUMENT
            );
            assert_eq!(
                aoi_rs_nms_keep(std::ptr::null(), 3, 0.5, 0, std::ptr::null_mut(), std::ptr::null_mut()),
                AOI_RS_INVALID_ARGUMENT
            );
            let mut count = 99usize;
            assert_eq!(
                aoi_rs_nms_keep(std::ptr::null(), 0, 0.5, 0, std::ptr::null_mut(), &mut count),
                AOI_RS_OK
            );
            assert_eq!(count, 0);
        }
    }

    #[test]
    fn overlap_index_handle_round_trip() {
        let mut handle: *mut OverlapIndex = std::ptr::null_mut();
        let r = RectObj {
            x: 0.0,
            y: 0.0,
            angle_deg: 0.0,
            width: 10.0,
            height: 10.0,
        };
        // SAFETY: handle 由 new 建立、最後 free 一次。
        unsafe {
            assert_eq!(aoi_rs_overlap_index_new(14.0, 0.5, 0, &mut handle), AOI_RS_OK);
            let mut hit = -1;
            assert_eq!(aoi_rs_overlap_index_overlaps_any(handle, &r, &mut hit), AOI_RS_OK);
            assert_eq!(hit, 0);
            assert_eq!(aoi_rs_overlap_index_add(handle, &r), AOI_RS_OK);
            assert_eq!(aoi_rs_overlap_index_overlaps_any(handle, &r, &mut hit), AOI_RS_OK);
            assert_eq!(hit, 1);
            let mut size = 0usize;
            assert_eq!(aoi_rs_overlap_index_size(handle, &mut size), AOI_RS_OK);
            assert_eq!(size, 1);
            aoi_rs_overlap_index_free(handle);
        }
    }
}
