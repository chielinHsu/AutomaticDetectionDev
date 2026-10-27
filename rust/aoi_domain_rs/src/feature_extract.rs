//! 比對區的自動擷取（R05.3）：`src/domain/feature_extract.cpp` 的 Rust 實作（C++ 那一份在
//! `src/domain/legacy/feature_extract_legacy.cpp`）。規則見 `src/domain/feature_extract.h`（ADR-028）：
//! Otsu 二值化 → 離點擊最近的亮連通塊（8 連通）→ 外擴（圓盤）。
//!
//! 運算順序照 C++：亮度的公式、Otsu 的累加順序、最近前景像素的比較（同距離取先掃到的）、`std::lround`、
//! `static_cast<int>` 與 int 的溢位（MSVC 會繞回；Rust 用 `wrapping_*`，Debug 才不會 panic）。

// 刻意的寫法和 `lattice` 相同：用索引走訪（照 C++ 的迴圈，逐行對照比較容易）。
#![allow(clippy::needless_range_loop)]

use crate::cpp::{msvc_i32, msvc_lround};
use crate::fallible::{Res, TryPush, vec_filled};
use crate::geometry::Point;
use crate::lattice::basis::Bounds;
use crate::match_region::{MaskRef, PixelMask};

/// `PixelFormat` 的數值（C++ 的包裝 `static_assert` 兩邊相同）。
pub const PIXEL_GRAY8: i32 = 0;
pub const PIXEL_RGB24: i32 = 1;
pub const PIXEL_BGRX32: i32 = 2;
pub const PIXEL_BGR24: i32 = 3;
pub const PIXEL_GRAY16: i32 = 4;

/// `bytesPerPixel`；不認得的格式是 0（和 C++ 的 switch 相同）。
pub fn bytes_per_pixel(format: i32) -> i64 {
    match format {
        PIXEL_GRAY8 => 1,
        PIXEL_RGB24 | PIXEL_BGR24 => 3,
        PIXEL_BGRX32 => 4,
        PIXEL_GRAY16 => 2,
        _ => 0,
    }
}

/// `normalizedMax`（image_depth.h）：這個位元數的最大值（2^bits − 1）。
pub fn normalized_max(bits: i32) -> i32 {
    if bits <= 0 || bits > 16 { 65535 } else { (1 << bits) - 1 }
}

/// 有效的 `ImageView` 的像素（借用，不複製）。`data` 從第一列的起點到最後一列實際用到的 byte。
/// `effective_bits` 是 C++ 的 `effectiveBits(image)`（image_depth 留在 C++，ADR-031）。
#[derive(Clone, Copy, Debug)]
pub struct Image<'a> {
    pub data: &'a [u8],
    pub width: i32,
    pub height: i32,
    pub stride_bytes: i32,
    pub format: i32,
    pub effective_bits: i32,
}

/// `ImageView::valid`（data 不是 null 另外檢查）。
pub fn image_is_valid(width: i32, height: i32, stride_bytes: i32, format: i32) -> bool {
    width > 0 && height > 0 && i64::from(stride_bytes) >= i64::from(width) * bytes_per_pixel(format)
}

/// 有效的影像實際可以讀的 byte 數：stride × (height − 1) ＋ 一列用到的 byte。
pub fn image_byte_len(width: i32, height: i32, stride_bytes: i32, format: i32) -> usize {
    (stride_bytes as usize) * (height as usize - 1) + (i64::from(width) * bytes_per_pixel(format)) as usize
}

/// 一個像素的亮度（0..255）。公式和 GenerateObjectsFromSample 的對比檢查相同；Gray16 依有效位元數換算。
fn luminance8(format: i32, row: &[u8], x: usize, gray16_scale: f64) -> i32 {
    let v = match format {
        PIXEL_GRAY8 => f64::from(row[x]),
        PIXEL_RGB24 => {
            let p = &row[3 * x..];
            (77.0 * f64::from(p[0]) + 150.0 * f64::from(p[1]) + 29.0 * f64::from(p[2])) / 256.0
        }
        PIXEL_BGRX32 => {
            let p = &row[4 * x..];
            (77.0 * f64::from(p[2]) + 150.0 * f64::from(p[1]) + 29.0 * f64::from(p[0])) / 256.0
        }
        PIXEL_BGR24 => {
            let p = &row[3 * x..];
            (77.0 * f64::from(p[2]) + 150.0 * f64::from(p[1]) + 29.0 * f64::from(p[0])) / 256.0
        }
        PIXEL_GRAY16 => {
            // 本機的位元組順序（C++ 用 memcpy：列的起點不保證 2 byte 對齊）。
            let g = u16::from_ne_bytes([row[2 * x], row[2 * x + 1]]);
            f64::from(g) * gray16_scale
        }
        _ => 0.0,
    };
    msvc_lround(v).clamp(0, 255)
}

/// Otsu：讓兩類的類間變異最大的門檻（亮度 > 門檻算前景）。直方圖只有一個值時回 −1（沒有可分的）。
fn otsu_threshold(histogram: &[usize; 256], total: usize) -> i32 {
    let mut sum_all = 0.0;
    for (i, &count) in histogram.iter().enumerate() {
        sum_all += i as f64 * count as f64;
    }
    let mut sum_back = 0.0;
    let mut weight_back = 0usize;
    let mut best = -1.0;
    let mut threshold = -1;
    for t in 0..255usize {
        weight_back += histogram[t];
        if weight_back == 0 {
            continue;
        }
        let weight_fore = total - weight_back;
        if weight_fore == 0 {
            break;
        }
        sum_back += t as f64 * histogram[t] as f64;
        let mean_back = sum_back / weight_back as f64;
        let mean_fore = (sum_all - sum_back) / weight_fore as f64;
        let between = weight_back as f64 * weight_fore as f64 * (mean_back - mean_fore) * (mean_back - mean_fore);
        if between > best {
            best = between;
            threshold = t as i32;
        }
    }
    threshold
}

#[derive(Clone, Copy, Debug)]
struct IntRect {
    x0: i32,
    y0: i32,
    x1: i32, // 不含
    y1: i32,
}

impl IntRect {
    fn is_empty(&self) -> bool {
        self.x1 <= self.x0 || self.y1 <= self.y0
    }
}

/// 浮點矩形的外接整數像素，裁到 [0, w) × [0, h)。
fn pixels_of(r: &Bounds, w: i32, h: i32) -> IntRect {
    IntRect {
        x0: 0.max(msvc_i32(r.x.floor())),
        y0: 0.max(msvc_i32(r.y.floor())),
        x1: w.min(msvc_i32(r.right().ceil())),
        y1: h.min(msvc_i32(r.bottom().ceil())),
    }
}

/// `pickBrightBlob`：within 與影像的交集裡做 Otsu；點擊落在前景就取它所在的 8 連通塊，否則取離點擊最近的前景像素所在的塊。
/// 回傳 (連通塊, Otsu 的門檻)；沒有前景、within 在影像外或影像無效（`image` 是 None）時 None。不讀 within 以外的像素。
pub fn pick_bright_blob(image: Option<&Image>, within: Bounds, click: Point) -> Res<Option<(PixelMask, i32)>> {
    let Some(image) = image else {
        return Ok(None);
    };
    let area = pixels_of(&within, image.width, image.height);
    if area.is_empty() {
        return Ok(None);
    }
    let w = area.x1 - area.x0;
    let h = area.y1 - area.y0;
    let wu = w as usize;
    let total = wu * h as usize;

    let gray16_scale = 255.0 / f64::from(normalized_max(image.effective_bits));
    let mut lum = vec_filled(total, 0u8)?;
    let mut histogram = [0usize; 256];
    for y in 0..h {
        let row = &image.data[((area.y0 + y) as usize) * (image.stride_bytes as usize)..];
        for x in 0..w {
            let v = luminance8(image.format, row, (area.x0 + x) as usize, gray16_scale);
            lum[y as usize * wu + x as usize] = v as u8;
            histogram[v as usize] += 1;
        }
    }
    let threshold = otsu_threshold(&histogram, total);
    if threshold < 0 {
        return Ok(None);
    }

    // 起點：點擊的像素是前景就用它，否則用最近的前景像素（同距離取先掃到的：由上而下、由左而右）。
    let cx = msvc_i32(click.x.floor()).wrapping_sub(area.x0);
    let cy = msvc_i32(click.y.floor()).wrapping_sub(area.y0);
    let foreground = |x: i32, y: i32| i32::from(lum[y as usize * wu + x as usize]) > threshold;
    let mut sx = -1;
    let mut sy = -1;
    if cx >= 0 && cy >= 0 && cx < w && cy < h && foreground(cx, cy) {
        sx = cx;
        sy = cy;
    } else {
        let mut best_d2 = f64::MAX;
        for y in 0..h {
            for x in 0..w {
                if !foreground(x, y) {
                    continue;
                }
                let dx = (f64::from(x) + 0.5) - (click.x - f64::from(area.x0));
                let dy = (f64::from(y) + 0.5) - (click.y - f64::from(area.y0));
                let d2 = dx * dx + dy * dy;
                if d2 < best_d2 {
                    best_d2 = d2;
                    sx = x;
                    sy = y;
                }
            }
        }
    }
    if sx < 0 {
        return Ok(None);
    }

    // 8 連通的填色（堆疊，不遞迴）。
    let mut in_blob = vec_filled(total, 0u8)?;
    let mut stack: Vec<i32> = Vec::new();
    let start = sy.wrapping_mul(w).wrapping_add(sx);
    stack.try_push(start)?;
    in_blob[start as usize] = 1;
    let (mut min_x, mut min_y, mut max_x, mut max_y) = (sx, sy, sx, sy);
    while let Some(p) = stack.pop() {
        let px = p % w;
        let py = p / w;
        min_x = min_x.min(px);
        max_x = max_x.max(px);
        min_y = min_y.min(py);
        max_y = max_y.max(py);
        for dy in -1..=1 {
            for dx in -1..=1 {
                let nx = px + dx;
                let ny = py + dy;
                if nx < 0 || ny < 0 || nx >= w || ny >= h {
                    continue;
                }
                let k = ny as usize * wu + nx as usize;
                if in_blob[k] != 0 || !foreground(nx, ny) {
                    continue;
                }
                in_blob[k] = 1;
                stack.try_push(ny.wrapping_mul(w).wrapping_add(nx))?;
            }
        }
    }

    let bw = max_x - min_x + 1;
    let bh = max_y - min_y + 1;
    let mut bits = vec_filled((bw as usize) * (bh as usize), 0u8)?;
    for y in min_y..=max_y {
        for x in min_x..=max_x {
            if in_blob[y as usize * wu + x as usize] == 0 {
                continue;
            }
            bits[(y - min_y) as usize * bw as usize + (x - min_x) as usize] = 1;
        }
    }
    let blob = PixelMask {
        x: area.x0 + min_x,
        y: area.y0 + min_y,
        width: bw,
        height: bh,
        bits,
    };
    Ok(Some((blob, threshold)))
}

/// `growMask`：把遮罩往外擴 radius 個像素（圓盤），裁到 clip（外接整數像素）內。radius ≤ 0 ＝ 只裁切。
/// 回傳 None ＝ 空的遮罩（C++ 的 `PixelMask{}`）。
pub fn grow_mask(blob: &MaskRef, radius: i32, clip: Bounds) -> Res<Option<PixelMask>> {
    if blob.is_empty() {
        return Ok(None);
    }
    let r = 0.max(radius);
    let limit = IntRect {
        x0: msvc_i32(clip.x.floor()),
        y0: msvc_i32(clip.y.floor()),
        x1: msvc_i32(clip.right().ceil()),
        y1: msvc_i32(clip.bottom().ceil()),
    };
    let out = IntRect {
        x0: limit.x0.max(blob.x.wrapping_sub(r)),
        y0: limit.y0.max(blob.y.wrapping_sub(r)),
        x1: limit.x1.min(blob.x.wrapping_add(blob.width).wrapping_add(r)),
        y1: limit.y1.min(blob.y.wrapping_add(blob.height).wrapping_add(r)),
    };
    if out.is_empty() {
        return Ok(None);
    }
    let w = out.x1.wrapping_sub(out.x0);
    let h = out.y1.wrapping_sub(out.y0);
    let wu = w as usize;
    let mut bits = vec_filled(wu.wrapping_mul(h as usize), 0u8)?;

    // 圓盤的位移（含邊界：dx² + dy² ≤ r²）。
    let mut disc: Vec<(i32, i32)> = Vec::new();
    let r2 = r.wrapping_mul(r);
    for dy in -r..=r {
        for dx in -r..=r {
            if dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) <= r2 {
                disc.try_push((dx, dy))?;
            }
        }
    }
    let bwu = blob.width as usize;
    for by in 0..blob.height {
        for bx in 0..blob.width {
            if blob.bits[by as usize * bwu + bx as usize] == 0 {
                continue;
            }
            let px = blob.x.wrapping_add(bx);
            let py = blob.y.wrapping_add(by);
            for &(dx, dy) in &disc {
                let x = px.wrapping_add(dx).wrapping_sub(out.x0);
                let y = py.wrapping_add(dy).wrapping_sub(out.y0);
                if x < 0 || y < 0 || x >= w || y >= h {
                    continue;
                }
                bits[y as usize * wu + x as usize] = 1;
            }
        }
    }

    // 裁到實際有像素的外接矩形（裁切後可能縮小）。
    let (mut min_x, mut min_y, mut max_x, mut max_y) = (w, h, -1, -1);
    for y in 0..h {
        for x in 0..w {
            if bits[y as usize * wu + x as usize] == 0 {
                continue;
            }
            min_x = min_x.min(x);
            max_x = max_x.max(x);
            min_y = min_y.min(y);
            max_y = max_y.max(y);
        }
    }
    if max_x < 0 {
        return Ok(None);
    }
    let gw = max_x - min_x + 1;
    let gh = max_y - min_y + 1;
    let mut grown = vec_filled((gw as usize) * (gh as usize), 0u8)?;
    for y in min_y..=max_y {
        for x in min_x..=max_x {
            grown[(y - min_y) as usize * gw as usize + (x - min_x) as usize] = bits[y as usize * wu + x as usize];
        }
    }
    Ok(Some(PixelMask {
        x: out.x0 + min_x,
        y: out.y0 + min_y,
        width: gw,
        height: gh,
        bits: grown,
    }))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn gray8(width: i32, height: i32, pixels: &[u8]) -> Image<'_> {
        Image {
            data: pixels,
            width,
            height,
            stride_bytes: width,
            format: PIXEL_GRAY8,
            effective_bits: 8,
        }
    }

    #[test]
    fn picks_the_blob_nearest_to_the_click() {
        // 10 × 6：左邊一塊 2 × 2、右邊一塊 3 × 2（亮度 200），底 10。
        let mut px = vec![10u8; 60];
        for (x, y) in [(1, 1), (2, 1), (1, 2), (2, 2), (6, 3), (7, 3), (8, 3), (6, 4), (7, 4), (8, 4)] {
            px[y * 10 + x] = 200;
        }
        let image = gray8(10, 6, &px);
        let within = Bounds {
            x: 0.0,
            y: 0.0,
            w: 10.0,
            h: 6.0,
        };
        let (blob, threshold) = pick_bright_blob(Some(&image), within, Point { x: 5.2, y: 4.1 }).unwrap().unwrap();
        assert!((10..200).contains(&threshold));
        assert_eq!((blob.x, blob.y, blob.width, blob.height), (6, 3, 3, 2));
        assert_eq!(blob.as_ref().count(), 6);
        let (left, _) = pick_bright_blob(Some(&image), within, Point { x: 1.5, y: 1.5 }).unwrap().unwrap();
        assert_eq!((left.x, left.y, left.width, left.height), (1, 1, 2, 2));
        // 整塊一樣亮、within 在影像外、NaN 的點擊（沒有最近的前景）。
        assert!(
            pick_bright_blob(Some(&gray8(2, 2, &[5, 5, 5, 5])), within, Point::default())
                .unwrap()
                .is_none()
        );
        assert!(
            pick_bright_blob(
                Some(&image),
                Bounds {
                    x: 50.0,
                    y: 0.0,
                    w: 5.0,
                    h: 5.0
                },
                Point::default()
            )
            .unwrap()
            .is_none()
        );
        assert!(
            pick_bright_blob(Some(&image), within, Point { x: f64::NAN, y: 1.0 })
                .unwrap()
                .is_none()
        );
    }

    #[test]
    fn grow_is_a_clipped_disc() {
        let one = PixelMask {
            x: 10,
            y: 10,
            width: 1,
            height: 1,
            bits: vec![1],
        };
        let grown = grow_mask(
            &one.as_ref(),
            2,
            Bounds {
                x: 0.0,
                y: 0.0,
                w: 100.0,
                h: 100.0,
            },
        )
        .unwrap()
        .unwrap();
        assert_eq!((grown.x, grown.y, grown.width, grown.height), (8, 8, 5, 5));
        assert_eq!(grown.as_ref().count(), 13);
        let clipped = grow_mask(
            &one.as_ref(),
            2,
            Bounds {
                x: 10.0,
                y: 0.0,
                w: 100.0,
                h: 100.0,
            },
        )
        .unwrap()
        .unwrap();
        assert_eq!((clipped.x, clipped.width), (10, 3));
        assert!(
            grow_mask(
                &one.as_ref(),
                2,
                Bounds {
                    x: 50.0,
                    y: 50.0,
                    w: 5.0,
                    h: 5.0
                }
            )
            .unwrap()
            .is_none()
        );
        assert!(
            grow_mask(
                &PixelMask::default().as_ref(),
                2,
                Bounds {
                    x: 0.0,
                    y: 0.0,
                    w: 9.0,
                    h: 9.0
                }
            )
            .unwrap()
            .is_none()
        );
    }

    #[test]
    fn gray16_uses_the_effective_bits() {
        let raw: Vec<u8> = [0u16, 4095, 0, 0].iter().flat_map(|v| v.to_ne_bytes()).collect();
        let image = Image {
            data: &raw,
            width: 2,
            height: 2,
            stride_bytes: 4,
            format: PIXEL_GRAY16,
            effective_bits: 12,
        };
        let row = &raw[..];
        assert_eq!(luminance8(PIXEL_GRAY16, row, 1, 255.0 / f64::from(normalized_max(12))), 255);
        let (blob, _) = pick_bright_blob(
            Some(&image),
            Bounds {
                x: 0.0,
                y: 0.0,
                w: 2.0,
                h: 2.0,
            },
            Point { x: 1.5, y: 0.5 },
        )
        .unwrap()
        .unwrap();
        assert_eq!((blob.x, blob.y, blob.as_ref().count()), (1, 0, 1));

        assert_eq!(luminance8(PIXEL_GRAY16, row, 1, 255.0 / f64::from(normalized_max(16))), 16);
    }
}
