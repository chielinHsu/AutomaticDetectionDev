//! 重複性的樣本統計（R05.4）的 C ABI。標頭是 `src/domain/ffi/aoi_domain_rs.h`：改這裡就要改那裡（`AOI_RS_ABI_VERSION` 也要加 1）。
//!
//! 規則和 `ffi` 相同（`catch_unwind`），另外：
//! - 輸入不複製：值直接借用 `std::span<const double>` 的資料（`count` 是 0 時指標可以是 null）。
//! - 輸出是固定大小的結構，由呼叫端配置（`AoiRsStatistics`；C++ 逐欄 `static_assert`，這裡用 `offset_of!` 檢查同樣的位置）。
//!   `has_*` 是 0 ＝ C++ 的 `std::nullopt`，那時對應的值寫 0。這裡不配置記憶體，所以沒有 `AOI_RS_NO_MEMORY`。

use std::mem::{offset_of, size_of};

use crate::ffi::{AOI_RS_INVALID_ARGUMENT, AOI_RS_OK, guard};
use crate::repeatability::{self, Statistics};

/// `AoiRsStatistics`：`aoi::repeatability::Statistics` 的 C 版本（`std::optional<double>` 拆成旗標與值）。
#[repr(C)]
#[derive(Debug, Default, Clone, Copy)]
pub struct FfiStatistics {
    pub count: usize,
    pub expected: usize,
    pub has_minimum: i32,
    pub has_maximum: i32,
    pub has_average: i32,
    pub has_range: i32,
    pub has_deviation: i32,
    pub has_pt: i32,
    pub minimum: f64,
    pub maximum: f64,
    pub average: f64,
    pub range: f64,
    pub deviation: f64,
    pub pt: f64,
}

// 和 C++ 那一側（src/domain/repeatability_rs.cpp）的 static_assert 同樣的數字。
const _: () = assert!(size_of::<FfiStatistics>() == 88);
const _: () = assert!(offset_of!(FfiStatistics, count) == 0 && offset_of!(FfiStatistics, expected) == 8);
const _: () = assert!(offset_of!(FfiStatistics, has_minimum) == 16 && offset_of!(FfiStatistics, has_pt) == 36);
const _: () = assert!(offset_of!(FfiStatistics, minimum) == 40 && offset_of!(FfiStatistics, maximum) == 48);
const _: () = assert!(offset_of!(FfiStatistics, average) == 56 && offset_of!(FfiStatistics, range) == 64);
const _: () = assert!(offset_of!(FfiStatistics, deviation) == 72 && offset_of!(FfiStatistics, pt) == 80);

fn split(v: Option<f64>) -> (i32, f64) {
    v.map_or((0, 0.0), |x| (1, x))
}

fn to_ffi(s: &Statistics) -> FfiStatistics {
    let (has_minimum, minimum) = split(s.minimum);
    let (has_maximum, maximum) = split(s.maximum);
    let (has_average, average) = split(s.average);
    let (has_range, range) = split(s.range);
    let (has_deviation, deviation) = split(s.deviation);
    let (has_pt, pt) = split(s.pt);
    FfiStatistics {
        count: s.count,
        expected: s.expected,
        has_minimum,
        has_maximum,
        has_average,
        has_range,
        has_deviation,
        has_pt,
        minimum,
        maximum,
        average,
        range,
        deviation,
        pt,
    }
}

/// `summarize(values, expected, tolerance)`；`has_tolerance` 是 0 ＝ 沒有 SPEC（`tolerance` 不看）。
///
/// # Safety
/// `values` 指向 `count` 個 `double`（或 count 是 0）；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_repeatability_summarize(
    values: *const f64,
    count: usize,
    expected: usize,
    has_tolerance: i32,
    tolerance: f64,
    out: *mut FfiStatistics,
) -> i32 {
    guard(|| {
        if out.is_null() || (values.is_null() && count != 0) {
            return AOI_RS_INVALID_ARGUMENT;
        }
        let values: &[f64] = if count == 0 {
            &[]
        } else {
            // SAFETY: 呼叫端保證 values 指向 count 個 double（上面已排除 null）。
            unsafe { std::slice::from_raw_parts(values, count) }
        };
        let tolerance = (has_tolerance != 0).then_some(tolerance);
        let s = repeatability::summarize(values, expected, tolerance);
        // SAFETY: out 可寫（上面已排除 null）。
        unsafe { *out = to_ffi(&s) };
        AOI_RS_OK
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn null_pointers_are_rejected_and_an_empty_span_may_be_null() {
        let mut out = FfiStatistics::default();
        // SAFETY: 測試 null 的處理；不會解參考 null。
        unsafe {
            assert_eq!(
                aoi_rs_repeatability_summarize(std::ptr::null(), 0, 3, 0, 0.0, std::ptr::null_mut()),
                AOI_RS_INVALID_ARGUMENT
            );
            assert_eq!(
                aoi_rs_repeatability_summarize(std::ptr::null(), 2, 3, 0, 0.0, &mut out),
                AOI_RS_INVALID_ARGUMENT
            );
            assert_eq!(
                aoi_rs_repeatability_summarize(std::ptr::null(), 0, 3, 1, 200.0, &mut out),
                AOI_RS_OK
            );
        }
        assert_eq!((out.count, out.expected, out.has_average, out.has_pt), (0, 3, 0, 0));
    }

    #[test]
    fn optionals_become_flags() {
        let values = [-48.0, -41.0, -43.0, -44.0, -46.0];
        let mut out = FfiStatistics::default();
        // SAFETY: values 指向 5 個 double；out 可寫。
        assert_eq!(
            unsafe { aoi_rs_repeatability_summarize(values.as_ptr(), 5, 5, 1, 350.0, &mut out) },
            AOI_RS_OK
        );
        assert_eq!(
            (out.count, out.has_minimum, out.minimum, out.has_range, out.range),
            (5, 1, -48.0, 1, 7.0)
        );
        assert_eq!((out.has_deviation, out.has_pt), (1, 1));
        // SAFETY: 同上；沒有 SPEC。
        assert_eq!(
            unsafe { aoi_rs_repeatability_summarize(values.as_ptr(), 1, 5, 0, 350.0, &mut out) },
            AOI_RS_OK
        );
        assert_eq!(
            (out.count, out.has_average, out.has_deviation, out.deviation, out.has_pt),
            (1, 1, 0, 0.0, 0)
        );
    }
}
