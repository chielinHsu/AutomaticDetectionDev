//! 比對區與自動擷取（R05.3）的 C ABI。標頭是 `src/domain/ffi/aoi_domain_rs.h`：改這裡就要改那裡（`AOI_RS_ABI_VERSION` 也要加 1）。
//!
//! 規則和 `ffi`、`ffi_lattice` 相同（`catch_unwind`、`try_reserve`、誰配置誰釋放），另外：
//! - 輸入不複製：遮罩的 bits、影像的像素、`std::vector<MaskRun>`（欄位和 `MaskRun` 相同，C++ 逐欄 `static_assert`）都直接借用。
//! - 長度不固定的輸出（遮罩、runs）是 handle：`aoi_rs_pixel_mask_get`／`aoi_rs_mask_runs_get` 讀、`*_free` 釋放；
//!   handle 是 null ＝ 空的遮罩（C++ 的 `PixelMask{}`）或沒有遮罩。
//! - 長度有上限的輸出（`aoi_rs_mask_from_runs` 的 bits：寬 × 高）由呼叫端配置。

use crate::fallible::Fail;
use crate::feature_extract::{self, Image};
use crate::ffi::{AOI_RS_INVALID_ARGUMENT, AOI_RS_NO_MEMORY, AOI_RS_OK, guard};
use crate::geometry::Point;
use crate::lattice::basis::Bounds;
use crate::match_region::{self, MaskRef, MaskRun, MaskRunsRef, MatchMask, PixelMask, RegionRef, RegionUm};

/// `AoiRsPixelMaskView`：遮罩的檢視（輸入時 bits 指向呼叫端的陣列；`aoi_rs_pixel_mask_get` 讀出時指向 handle 裡的陣列）。
#[repr(C)]
pub struct FfiMaskView {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
    pub bits: *const u8,
    pub bit_count: usize,
}

/// `AoiRsMatchRegion`：類型上的比對區（runs 指向呼叫端的陣列）。
#[repr(C)]
pub struct FfiMatchRegion {
    pub rect: RegionUm,
    pub mask_width: i32,
    pub mask_height: i32,
    pub runs: *const MaskRun,
    pub run_count: usize,
}

/// `AoiRsImage`：`ImageView` 的像素（借用）與 C++ 算好的有效位元數。
#[repr(C)]
pub struct FfiImage {
    pub data: *const u8,
    pub width: i32,
    pub height: i32,
    pub stride_bytes: i32,
    pub format: i32,
    pub effective_bits: i32,
}

const _: () = assert!(std::mem::size_of::<MaskRun>() == 12);
const _: () = assert!(std::mem::size_of::<FfiMaskView>() == 32);
const _: () = assert!(std::mem::size_of::<RegionUm>() == 32);
const _: () = assert!(std::mem::size_of::<FfiMatchRegion>() == 56);
const _: () = assert!(std::mem::size_of::<FfiImage>() == 32);

/// 遮罩（`aoi_rs_pick_bright_blob`、`aoi_rs_grow_mask`、`aoi_rs_match_spec_for` 的輸出）。
pub struct MaskHandle(PixelMask);
/// runs（`aoi_rs_runs_from_mask`、`aoi_rs_match_settings_from_pixels` 的輸出）。
pub struct RunsHandle(MatchMask);

fn status_of(fail: Fail) -> i32 {
    match fail {
        Fail::NoMemory => AOI_RS_NO_MEMORY,
        Fail::Callback(_) => AOI_RS_INVALID_ARGUMENT, // 這裡沒有回呼
    }
}

/// 呼叫端給的陣列換成切片（`count` 是 0 時指標可以是 null）。
///
/// # Safety
/// `ptr` 指向 `count` 個有效的元素（或 count 是 0）。
unsafe fn slice<'a, T>(ptr: *const T, count: usize) -> Option<&'a [T]> {
    if count == 0 {
        return Some(&[]);
    }
    if ptr.is_null() {
        return None;
    }
    // SAFETY: 呼叫端保證 ptr 指向 count 個元素。
    Some(unsafe { std::slice::from_raw_parts(ptr, count) })
}

/// `AoiRsPixelMaskView` → 借用的遮罩。
///
/// # Safety
/// `view.bits` 指向 `view.bit_count` 個 byte（或 bit_count 是 0）。
unsafe fn mask_of<'a>(view: &FfiMaskView) -> Option<MaskRef<'a>> {
    // SAFETY: 呼叫端保證。
    let bits = unsafe { slice(view.bits, view.bit_count) }?;
    Some(MaskRef {
        x: view.x,
        y: view.y,
        width: view.width,
        height: view.height,
        bits,
    })
}

fn into_mask_handle(mask: Option<PixelMask>) -> *mut MaskHandle {
    mask.map_or(std::ptr::null_mut(), |m| Box::into_raw(Box::new(MaskHandle(m))))
}

fn into_runs_handle(runs: Option<MatchMask>) -> *mut RunsHandle {
    runs.map_or(std::ptr::null_mut(), |r| Box::into_raw(Box::new(RunsHandle(r))))
}

/// `PixelMask::count`。
///
/// # Safety
/// `mask` 是有效的檢視；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_mask_count(mask: *const FfiMaskView, out: *mut usize) -> i32 {
    guard(|| {
        if mask.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: mask、out 有效（上面已排除 null）；bits 由呼叫端保證。
        unsafe {
            let Some(m) = mask_of(&*mask) else {
                return AOI_RS_INVALID_ARGUMENT;
            };
            *out = m.count();
        }
        AOI_RS_OK
    })
}

/// `runsFromMask`；成功時 `*out` 是新的 handle（遮罩是空的時 handle 裡的寬高與 runs 都是空的）。
///
/// # Safety
/// `mask` 是有效的檢視；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_runs_from_mask(mask: *const FfiMaskView, out: *mut *mut RunsHandle) -> i32 {
    guard(|| {
        if mask.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: mask、out 有效（上面已排除 null）；bits 由呼叫端保證。
        unsafe {
            let Some(m) = mask_of(&*mask) else {
                return AOI_RS_INVALID_ARGUMENT;
            };
            match match_region::runs_from_mask(&m) {
                Ok(runs) => {
                    *out = into_runs_handle(Some(runs));
                    AOI_RS_OK
                }
                Err(e) => status_of(e),
            }
        }
    })
}

/// `maskFromRuns` 的 bits：`out_bits` 由呼叫端配置 `width × height` 個（`out_len`），整個寫過（不合法的 run 忽略）。
/// `width`、`height` 要 > 0（≤ 0 時 C++ 的包裝直接回空的遮罩，不呼叫這裡）。
///
/// # Safety
/// `runs` 指向 `count` 個 run；`out_bits` 可寫 `out_len` 個 byte。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_mask_from_runs(
    runs: *const MaskRun,
    count: usize,
    width: i32,
    height: i32,
    out_bits: *mut u8,
    out_len: usize,
) -> i32 {
    guard(|| {
        if width <= 0 || height <= 0 || out_bits.is_null() || out_len != (width as usize) * (height as usize) {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 呼叫端保證 runs 指向 count 個元素。
        let Some(r) = (unsafe { slice(runs, count) }) else {
            return AOI_RS_INVALID_ARGUMENT;
        };
        // SAFETY: out_bits 可寫 out_len 個 byte（呼叫端保證，上面已排除 null）。
        let bits = unsafe { std::slice::from_raw_parts_mut(out_bits, out_len) };
        match_region::fill_mask_from_runs(&MaskRunsRef { width, height, runs: r }, bits);
        AOI_RS_OK
    })
}

/// `matchSpecFor`（rect 與 mask；mode 由呼叫端照抄）。`region` 是 null ＝ 沒有比對區（整個主體框）。
/// `*has_rect` 是 0 時 `*rect` 不變；`*mask` 是 null ＝ 沒有遮罩。
///
/// # Safety
/// `region` 是 null 或有效（runs 指向 run_count 個）；`has_rect`、`rect`、`mask` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_match_spec_for(
    region: *const FfiMatchRegion,
    body_center: Point,
    um_per_pixel: f64,
    has_rect: *mut i32,
    rect: *mut Bounds,
    mask: *mut *mut MaskHandle,
) -> i32 {
    guard(|| {
        if has_rect.is_null() || rect.is_null() || mask.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: region 是 null 或有效（呼叫端保證）；runs 由呼叫端保證。
        let region = if region.is_null() {
            None
        } else {
            let r = unsafe { &*region };
            let Some(runs) = (unsafe { slice(r.runs, r.run_count) }) else {
                return AOI_RS_INVALID_ARGUMENT;
            };
            Some(RegionRef {
                rect: r.rect,
                mask: MaskRunsRef {
                    width: r.mask_width,
                    height: r.mask_height,
                    runs,
                },
            })
        };
        match match_region::match_spec_for(region.as_ref(), body_center, um_per_pixel) {
            Ok(spec) => {
                // SAFETY: 三個輸出指標有效（上面已排除 null）。
                unsafe {
                    *has_rect = i32::from(spec.rect.is_some());
                    if let Some(r) = spec.rect {
                        *rect = r;
                    }
                    *mask = into_mask_handle(spec.mask);
                }
                AOI_RS_OK
            }
            Err(e) => status_of(e),
        }
    })
}

/// `matchSettingsFromPixels`（region；mode 由呼叫端照抄）。`mask` 是 null ＝ 沒有遮罩。
/// `*has_region` 是 0 時 `*region` 不變；`*mask_runs` 是 null ＝ 比對區沒有遮罩。
///
/// # Safety
/// `mask` 是 null 或有效的檢視；`has_region`、`region`、`mask_runs` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_match_settings_from_pixels(
    region_px: Bounds,
    mask: *const FfiMaskView,
    body_center: Point,
    um_per_pixel: f64,
    has_region: *mut i32,
    region: *mut RegionUm,
    mask_runs: *mut *mut RunsHandle,
) -> i32 {
    guard(|| {
        if has_region.is_null() || region.is_null() || mask_runs.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        let m = if mask.is_null() {
            None
        } else {
            // SAFETY: mask 有效（呼叫端保證）。
            match unsafe { mask_of(&*mask) } {
                Some(m) => Some(m),
                None => return AOI_RS_INVALID_ARGUMENT,
            }
        };
        match match_region::match_settings_from_pixels(region_px, m.as_ref(), body_center, um_per_pixel) {
            Ok(out) => {
                // SAFETY: 三個輸出指標有效（上面已排除 null）。
                unsafe {
                    *has_region = i32::from(out.is_some());
                    match out {
                        Some(r) => {
                            *region = r.rect;
                            *mask_runs = into_runs_handle(r.mask);
                        }
                        None => *mask_runs = std::ptr::null_mut(),
                    }
                }
                AOI_RS_OK
            }
            Err(e) => status_of(e),
        }
    })
}

/// `pickBrightBlob`：找到時 `*found` 是 1、`*threshold8` 是 Otsu 的門檻、`*blob` 是連通塊的 handle；找不到時 `*found` 是 0、
/// `*blob` 是 null。影像無效（data 是 null、寬高 ≤ 0、stride 不夠）時找不到（和 C++ 一樣不是錯誤）。
///
/// # Safety
/// `image` 有效，像素至少有 stride × (height − 1) ＋ 一列的 byte；`found`、`threshold8`、`blob` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_pick_bright_blob(
    image: *const FfiImage,
    within: Bounds,
    click: Point,
    found: *mut i32,
    threshold8: *mut i32,
    blob: *mut *mut MaskHandle,
) -> i32 {
    guard(|| {
        if image.is_null() || found.is_null() || threshold8.is_null() || blob.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: image 有效（上面已排除 null）。
        let raw = unsafe { &*image };
        let valid = !raw.data.is_null() && feature_extract::image_is_valid(raw.width, raw.height, raw.stride_bytes, raw.format);
        let view = if valid {
            let len = feature_extract::image_byte_len(raw.width, raw.height, raw.stride_bytes, raw.format);
            Some(Image {
                // SAFETY: 有效的 ImageView 至少有這麼多 byte（呼叫端保證）。
                data: unsafe { std::slice::from_raw_parts(raw.data, len) },
                width: raw.width,
                height: raw.height,
                stride_bytes: raw.stride_bytes,
                format: raw.format,
                effective_bits: raw.effective_bits,
            })
        } else {
            None
        };
        match feature_extract::pick_bright_blob(view.as_ref(), within, click) {
            Ok(pick) => {
                // SAFETY: 三個輸出指標有效（上面已排除 null）。
                unsafe {
                    *found = i32::from(pick.is_some());
                    *threshold8 = pick.as_ref().map_or(0, |p| p.1);
                    *blob = into_mask_handle(pick.map(|p| p.0));
                }
                AOI_RS_OK
            }
            Err(e) => status_of(e),
        }
    })
}

/// `growMask`：`*out` 是外擴後的遮罩，null ＝ 空的遮罩。
///
/// # Safety
/// `blob` 是有效的檢視；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_grow_mask(blob: *const FfiMaskView, radius: i32, clip: Bounds, out: *mut *mut MaskHandle) -> i32 {
    guard(|| {
        if blob.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: blob、out 有效（上面已排除 null）；bits 由呼叫端保證。
        unsafe {
            let Some(m) = mask_of(&*blob) else {
                return AOI_RS_INVALID_ARGUMENT;
            };
            match feature_extract::grow_mask(&m, radius, clip) {
                Ok(grown) => {
                    *out = into_mask_handle(grown);
                    AOI_RS_OK
                }
                Err(e) => status_of(e),
            }
        }
    })
}

/// 讀遮罩的 handle（bits 指向 handle 裡的陣列）。
///
/// # Safety
/// `mask` 是有效的 handle；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_pixel_mask_get(mask: *const MaskHandle, out: *mut FfiMaskView) -> i32 {
    guard(|| {
        if mask.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 兩個指標有效（上面已排除 null）。
        unsafe {
            let m = &(*mask).0;
            *out = FfiMaskView {
                x: m.x,
                y: m.y,
                width: m.width,
                height: m.height,
                bits: m.bits.as_ptr(),
                bit_count: m.bits.len(),
            };
        }
        AOI_RS_OK
    })
}

/// 釋放遮罩的 handle。null 不做事。
///
/// # Safety
/// `mask` 是還沒釋放的 handle（或 null）。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_pixel_mask_free(mask: *mut MaskHandle) {
    let _ = guard(|| {
        if !mask.is_null() {
            // SAFETY: mask 由 Box::into_raw 產生、只釋放一次（呼叫端保證）。
            drop(unsafe { Box::from_raw(mask) });
        }
        AOI_RS_OK
    });
}

/// 讀 runs 的 handle（runs 指向 handle 裡的陣列）。
///
/// # Safety
/// `handle` 是有效的 handle；其他指標可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_mask_runs_get(
    handle: *const RunsHandle,
    width: *mut i32,
    height: *mut i32,
    runs: *mut *const MaskRun,
    count: *mut usize,
) -> i32 {
    guard(|| {
        if handle.is_null() || width.is_null() || height.is_null() || runs.is_null() || count.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 指標都有效（上面已排除 null）。
        unsafe {
            let r = &(*handle).0;
            *width = r.width;
            *height = r.height;
            *runs = r.runs.as_ptr();
            *count = r.runs.len();
        }
        AOI_RS_OK
    })
}

/// 釋放 runs 的 handle。null 不做事。
///
/// # Safety
/// `handle` 是還沒釋放的 handle（或 null）。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_mask_runs_free(handle: *mut RunsHandle) {
    let _ = guard(|| {
        if !handle.is_null() {
            // SAFETY: handle 由 Box::into_raw 產生、只釋放一次（呼叫端保證）。
            drop(unsafe { Box::from_raw(handle) });
        }
        AOI_RS_OK
    });
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn null_pointers_are_rejected() {
        let mut n = 0usize;
        // SAFETY: 測試 null 的處理；不會解參考。
        unsafe {
            assert_eq!(aoi_rs_mask_count(std::ptr::null(), &mut n), AOI_RS_INVALID_ARGUMENT);
            let view = FfiMaskView {
                x: 0,
                y: 0,
                width: 2,
                height: 2,
                bits: std::ptr::null(),
                bit_count: 4,
            };
            assert_eq!(aoi_rs_mask_count(&view, &mut n), AOI_RS_INVALID_ARGUMENT);
            let mut bits = [9u8; 4];
            assert_eq!(
                aoi_rs_mask_from_runs(std::ptr::null(), 0, 2, 2, bits.as_mut_ptr(), 3),
                AOI_RS_INVALID_ARGUMENT
            );
            assert_eq!(aoi_rs_mask_from_runs(std::ptr::null(), 0, 2, 2, bits.as_mut_ptr(), 4), AOI_RS_OK);
            assert_eq!(bits, [0; 4]);
        }
    }

    #[test]
    fn an_invalid_image_finds_nothing() {
        let image = FfiImage {
            data: std::ptr::null(),
            width: 10,
            height: 10,
            stride_bytes: 10,
            format: 0,
            effective_bits: 8,
        };
        let (mut found, mut threshold) = (7, 7);
        let mut blob: *mut MaskHandle = std::ptr::dangling_mut();
        // SAFETY: 輸出指標有效；data 是 null，不會讀像素。
        unsafe {
            assert_eq!(
                aoi_rs_pick_bright_blob(
                    &image,
                    Bounds {
                        x: 0.0,
                        y: 0.0,
                        w: 5.0,
                        h: 5.0
                    },
                    Point::default(),
                    &mut found,
                    &mut threshold,
                    &mut blob
                ),
                AOI_RS_OK
            );
        }
        assert_eq!((found, threshold), (0, 0));
        assert!(blob.is_null());
    }

    #[test]
    fn mask_handles_round_trip() {
        let bits = [0u8, 1, 1, 0, 1, 1];
        let view = FfiMaskView {
            x: 3,
            y: 4,
            width: 3,
            height: 2,
            bits: bits.as_ptr(),
            bit_count: bits.len(),
        };
        let mut grown: *mut MaskHandle = std::ptr::null_mut();
        let mut runs: *mut RunsHandle = std::ptr::null_mut();
        // SAFETY: handle 由 Rust 建立、最後各釋放一次。
        unsafe {
            assert_eq!(
                aoi_rs_grow_mask(
                    &view,
                    0,
                    Bounds {
                        x: 0.0,
                        y: 0.0,
                        w: 50.0,
                        h: 50.0
                    },
                    &mut grown
                ),
                AOI_RS_OK
            );
            let mut out = FfiMaskView {
                x: 0,
                y: 0,
                width: 0,
                height: 0,
                bits: std::ptr::null(),
                bit_count: 0,
            };
            assert_eq!(aoi_rs_pixel_mask_get(grown, &mut out), AOI_RS_OK);
            assert_eq!((out.x, out.y, out.width, out.height, out.bit_count), (4, 4, 2, 2, 4));
            aoi_rs_pixel_mask_free(grown);

            assert_eq!(aoi_rs_runs_from_mask(&view, &mut runs), AOI_RS_OK);
            let (mut w, mut h, mut p, mut n) = (0, 0, std::ptr::null(), 0usize);
            assert_eq!(aoi_rs_mask_runs_get(runs, &mut w, &mut h, &mut p, &mut n), AOI_RS_OK);
            assert_eq!((w, h, n), (3, 2, 2));
            assert_eq!(
                *p.add(1),
                MaskRun {
                    row: 1,
                    col_begin: 1,
                    col_end: 3
                }
            );
            aoi_rs_mask_runs_free(runs);
        }
    }
}
