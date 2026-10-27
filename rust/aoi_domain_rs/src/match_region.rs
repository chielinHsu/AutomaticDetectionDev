//! 比對區的換算（R05.3）：`src/domain/match_region.cpp` 的 Rust 實作（C++ 那一份在 `src/domain/legacy/match_region_legacy.cpp`）。
//!
//! 型別與規則見 `src/domain/match_region.h`（ADR-028）：存在類型上的設定（µm，相對主體中心）⇄ 影像像素的規格。
//! 運算順序、`std::lround` 與整數的行為照 MSVC（`cpp.rs`），差異測試才能要求結果一位不差。
//! `operator==` 留在 C++（型別本身的比較，`src/domain/match_region_equality.cpp`）。

// 刻意的寫法和 `lattice` 相同：`!(a > b)` 讓 NaN 走「否」的那一邊（和 C++ 對應）。
#![allow(clippy::neg_cmp_op_on_partial_ord)]

use crate::cpp::{msvc_i32, msvc_lround};
use crate::fallible::{Res, TryPush, vec_filled};
use crate::geometry::Point;
use crate::lattice::basis::Bounds;

/// 借用的遮罩（`PixelMask` 的檢視；FFI 的輸入，像素不複製）：左上角 (x, y)、寬高、逐列的 bits（1 ＝ 參與比對）。
#[derive(Clone, Copy, Debug)]
pub struct MaskRef<'a> {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
    pub bits: &'a [u8],
}

impl MaskRef<'_> {
    /// `PixelMask::empty`。
    pub fn is_empty(&self) -> bool {
        self.width <= 0 || self.height <= 0 || self.bits.len() != (self.width as usize) * (self.height as usize)
    }

    /// `PixelMask::count`。
    pub fn count(&self) -> usize {
        if self.is_empty() {
            return 0;
        }
        self.bits.iter().filter(|&&b| b != 0).count()
    }

    /// `PixelMask::at`：(px, py) 是影像像素的整數座標；在遮罩外回 false。
    pub fn at(&self, px: i32, py: i32) -> bool {
        if self.is_empty() || px < self.x || py < self.y || px >= self.x.wrapping_add(self.width) || py >= self.y.wrapping_add(self.height)
        {
            return false;
        }
        let k = (py.wrapping_sub(self.y) as usize)
            .wrapping_mul(self.width as usize)
            .wrapping_add(px.wrapping_sub(self.x) as usize);
        // 上面的檢查保證 k 在範圍內（座標溢位的輸入在 C++ 是未定義行為，這裡回 false，不 panic）。
        self.bits.get(k).is_some_and(|&b| b != 0)
    }

    /// `PixelMask::bounds`。
    pub fn bounds(&self) -> Bounds {
        Bounds {
            x: f64::from(self.x),
            y: f64::from(self.y),
            w: f64::from(self.width),
            h: f64::from(self.height),
        }
    }
}

/// `PixelMask`（擁有 bits；FFI 的輸出）。預設值 ＝ C++ 的 `PixelMask{}`（空的遮罩）。
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct PixelMask {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
    pub bits: Vec<u8>,
}

impl PixelMask {
    pub fn as_ref(&self) -> MaskRef<'_> {
        MaskRef {
            x: self.x,
            y: self.y,
            width: self.width,
            height: self.height,
            bits: &self.bits,
        }
    }
}

/// `MaskRun`：第 row 列的 [col_begin, col_end)。`#[repr(C)]`：和 C++ 的 `aoi::MaskRun`、C 的 `AoiRsMaskRun` 相同
/// （C++ 的包裝逐欄 `static_assert`，直接交出 `std::vector<MaskRun>` 的位址，不複製）。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct MaskRun {
    pub row: i32,
    pub col_begin: i32,
    pub col_end: i32,
}

/// 借用的 runs（`MatchMask` 的檢視）。
#[derive(Clone, Copy, Debug)]
pub struct MaskRunsRef<'a> {
    pub width: i32,
    pub height: i32,
    pub runs: &'a [MaskRun],
}

impl MaskRunsRef<'_> {
    /// `MatchMask::empty`。
    pub fn is_empty(&self) -> bool {
        self.width <= 0 || self.height <= 0 || self.runs.is_empty()
    }
}

/// `MatchMask`（擁有 runs；FFI 的輸出）。
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct MatchMask {
    pub width: i32,
    pub height: i32,
    pub runs: Vec<MaskRun>,
}

/// `MatchRegion` 的矩形（µm，比對區中心相對主體中心）。`#[repr(C)]`：和 C 的 `AoiRsRegionUm` 相同。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct RegionUm {
    pub center_x_um: f64,
    pub center_y_um: f64,
    pub width_um: f64,
    pub height_um: f64,
}

/// `MatchRegion`（輸入：遮罩的 runs 是借用的）。
#[derive(Clone, Copy, Debug)]
pub struct RegionRef<'a> {
    pub rect: RegionUm,
    pub mask: MaskRunsRef<'a>,
}

/// `matchSpecFor` 的結果（`MatchSpec` 的 rect 與 mask；mode 由 C++ 的包裝照抄）。
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SpecOut {
    pub rect: Option<Bounds>,
    pub mask: Option<PixelMask>,
}

/// `matchSettingsFromPixels` 的結果（`MatchSettings::region`；mode 由 C++ 的包裝照抄）。
#[derive(Clone, Debug, Default, PartialEq)]
pub struct RegionOut {
    pub rect: RegionUm,
    pub mask: Option<MatchMask>,
}

/// `rectFromCenter`（geometry.h）。
fn rect_from_center(center: Point, w: f64, h: f64) -> Bounds {
    Bounds {
        x: center.x - w * 0.5,
        y: center.y - h * 0.5,
        w,
        h,
    }
}

/// `Rect::center`。
fn center_of(r: &Bounds) -> Point {
    Point {
        x: r.x + r.w * 0.5,
        y: r.y + r.h * 0.5,
    }
}

/// `runsFromMask`：遮罩 → runs（格子的大小就是遮罩的寬高）。
pub fn runs_from_mask(mask: &MaskRef) -> Res<MatchMask> {
    let mut out = MatchMask::default();
    if mask.is_empty() {
        return Ok(out);
    }
    out.width = mask.width;
    out.height = mask.height;
    let w = mask.width as usize;
    for r in 0..mask.height {
        let row = &mask.bits[r as usize * w..][..w];
        let mut c = 0usize;
        while c < w {
            while c < w && row[c] == 0 {
                c += 1;
            }
            if c >= w {
                break;
            }
            let begin = c;
            while c < w && row[c] != 0 {
                c += 1;
            }
            out.runs.try_push(MaskRun {
                row: r,
                col_begin: begin as i32,
                col_end: c as i32,
            })?;
        }
    }
    Ok(out)
}

/// `maskFromRuns` 寫 bits 的那一段：`bits` 是 width × height 個（呼叫端配置）。不合法的 run（超出格子）忽略那一段。
/// 呼叫端保證 `runs.width`、`runs.height` > 0 而且 `bits.len()` ＝ width × height。
pub fn fill_mask_from_runs(runs: &MaskRunsRef, bits: &mut [u8]) {
    bits.fill(0);
    let w = runs.width as usize;
    for run in runs.runs {
        if run.row < 0 || run.row >= runs.height {
            continue;
        }
        let begin = 0.max(run.col_begin);
        let end = runs.width.min(run.col_end);
        for c in begin..end {
            bits[run.row as usize * w + c as usize] = 1;
        }
    }
}

/// `maskFromRuns`：runs → 遮罩（左上角 (x, y)）；格子的寬或高 ≤ 0 時是空的遮罩。
pub fn mask_from_runs(runs: &MaskRunsRef, x: i32, y: i32) -> Res<PixelMask> {
    if runs.width <= 0 || runs.height <= 0 {
        return Ok(PixelMask::default());
    }
    let mut bits = vec_filled((runs.width as usize) * (runs.height as usize), 0u8)?;
    fill_mask_from_runs(runs, &mut bits);
    Ok(PixelMask {
        x,
        y,
        width: runs.width,
        height: runs.height,
        bits,
    })
}

/// `matchSpecFor`：類型上的設定 → 影像像素的規格（主體中心在 `body_center`）。`region` 是 None ＝ 整個主體框。
/// 遮罩用最近鄰取樣到像素格（格子的外接矩形四捨五入到整數像素）。
pub fn match_spec_for(region: Option<&RegionRef>, body_center: Point, um_per_pixel: f64) -> Res<SpecOut> {
    let mut spec = SpecOut::default();
    let Some(region) = region else {
        return Ok(spec);
    };
    if !(um_per_pixel > 0.0) {
        return Ok(spec);
    }
    let px = 1.0 / um_per_pixel;
    let rect = rect_from_center(
        Point {
            x: body_center.x + region.rect.center_x_um * px,
            y: body_center.y + region.rect.center_y_um * px,
        },
        region.rect.width_um * px,
        region.rect.height_um * px,
    );
    if region.mask.is_empty() {
        spec.rect = Some(rect);
        return Ok(spec);
    }

    // 遮罩：外接矩形四捨五入到整數像素，每個像素取最近的格子（同一個校正值時就是原來的遮罩）。
    let x0 = msvc_lround(rect.x);
    let y0 = msvc_lround(rect.y);
    let w = 1.max(msvc_lround(rect.w));
    let h = 1.max(msvc_lround(rect.h));
    let grid = mask_from_runs(&region.mask, 0, 0)?;
    let grid = grid.as_ref();
    let mut bits = vec_filled((w as usize) * (h as usize), 0u8)?;
    for j in 0..h {
        let gy = (grid.height - 1).min(msvc_i32((f64::from(j) + 0.5) * f64::from(grid.height) / f64::from(h)));
        for i in 0..w {
            let gx = (grid.width - 1).min(msvc_i32((f64::from(i) + 0.5) * f64::from(grid.width) / f64::from(w)));
            bits[j as usize * w as usize + i as usize] = u8::from(grid.at(gx, gy));
        }
    }
    let mask = PixelMask {
        x: x0,
        y: y0,
        width: w,
        height: h,
        bits,
    };
    spec.rect = Some(mask.as_ref().bounds());
    spec.mask = Some(mask);
    Ok(spec)
}

/// `matchSettingsFromPixels`：影像像素的比對區 → 類型上的設定。`mask` 不是 None 而且不空時，比對區 ＝ 遮罩的外接矩形。
/// 回傳 None ＝ `MatchSettings::region` 是 nullopt（整個主體框）。
pub fn match_settings_from_pixels(
    region_px: Bounds,
    mask: Option<&MaskRef>,
    body_center: Point,
    um_per_pixel: f64,
) -> Res<Option<RegionOut>> {
    let mask = mask.filter(|m| !m.is_empty());
    let rect = match mask {
        Some(m) => m.bounds(),
        None => region_px,
    };
    if rect.is_empty() || !(um_per_pixel > 0.0) {
        return Ok(None);
    }
    let c = center_of(&rect);
    let region = RegionUm {
        center_x_um: (c.x - body_center.x) * um_per_pixel,
        center_y_um: (c.y - body_center.y) * um_per_pixel,
        width_um: rect.w * um_per_pixel,
        height_um: rect.h * um_per_pixel,
    };
    let runs = match mask {
        Some(m) => Some(runs_from_mask(m)?),
        None => None,
    };
    Ok(Some(RegionOut { rect: region, mask: runs }))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn mask(width: i32, height: i32, bits: &[u8]) -> PixelMask {
        PixelMask {
            x: 10,
            y: 20,
            width,
            height,
            bits: bits.to_vec(),
        }
    }

    #[test]
    fn runs_round_trip() {
        // 和 tst_match_region 的 runsRoundTrip 同一個遮罩。
        let m = mask(5, 3, &[0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0]);
        let runs = runs_from_mask(&m.as_ref()).unwrap();
        assert_eq!(runs.width, 5);
        assert_eq!(runs.runs.len(), 3);
        assert_eq!(
            runs.runs[0],
            MaskRun {
                row: 0,
                col_begin: 1,
                col_end: 3
            }
        );
        let back = mask_from_runs(
            &MaskRunsRef {
                width: runs.width,
                height: runs.height,
                runs: &runs.runs,
            },
            10,
            20,
        )
        .unwrap();
        assert_eq!(back, m);
        assert_eq!(back.as_ref().count(), 8);
    }

    #[test]
    fn invalid_runs_are_ignored_and_empty_masks_stay_empty() {
        let runs = [
            MaskRun {
                row: -1,
                col_begin: 0,
                col_end: 3,
            },
            MaskRun {
                row: 0,
                col_begin: -2,
                col_end: 2,
            },
            MaskRun {
                row: 1,
                col_begin: 2,
                col_end: 9,
            },
            MaskRun {
                row: 5,
                col_begin: 0,
                col_end: 1,
            },
        ];
        let m = mask_from_runs(
            &MaskRunsRef {
                width: 4,
                height: 2,
                runs: &runs,
            },
            0,
            0,
        )
        .unwrap();
        assert_eq!(m.bits, vec![1, 1, 0, 0, 0, 0, 1, 1]);
        assert_eq!(
            mask_from_runs(
                &MaskRunsRef {
                    width: 0,
                    height: 2,
                    runs: &runs
                },
                3,
                4
            )
            .unwrap(),
            PixelMask::default()
        );
        assert_eq!(mask(2, 2, &[1, 1, 1]).as_ref().count(), 0); // bits 的個數不對 ＝ 空的遮罩
    }

    #[test]
    fn spec_without_region_or_calibration_is_the_whole_body() {
        assert_eq!(match_spec_for(None, Point { x: 1.0, y: 2.0 }, 2.5).unwrap(), SpecOut::default());
        let region = RegionRef {
            rect: RegionUm {
                center_x_um: 10.0,
                center_y_um: 0.0,
                width_um: 20.0,
                height_um: 10.0,
            },
            mask: MaskRunsRef {
                width: 0,
                height: 0,
                runs: &[],
            },
        };
        assert_eq!(match_spec_for(Some(&region), Point::default(), 0.0).unwrap(), SpecOut::default());
        assert_eq!(
            match_spec_for(Some(&region), Point::default(), f64::NAN).unwrap(),
            SpecOut::default()
        );
        let spec = match_spec_for(Some(&region), Point { x: 100.0, y: 50.0 }, 2.0).unwrap();
        assert_eq!(
            spec.rect,
            Some(Bounds {
                x: 100.0,
                y: 47.5,
                w: 10.0,
                h: 5.0
            })
        );
        assert!(spec.mask.is_none());
    }
}
