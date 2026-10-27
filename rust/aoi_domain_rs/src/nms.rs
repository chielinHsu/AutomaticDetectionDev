//! 非極大值抑制（NMS）：`src/domain/lattice/non_max_suppression.h` 的實作（R05.1，ADR-031）。
//!
//! 數值的規則和 C++ 的實作（`aoi::legacy`）一步一步相同：運算順序、`std::min`／`std::max`／`std::clamp`
//! 遇到 NaN 的行為（`cpp_min` 等）、`static_cast<int>` 超出範圍的結果（`msvc_i32`）。這樣差異測試才能要求
//! 索引完全相同，而不是「大致相同」。

use std::cmp::Ordering;
use std::collections::TryReserveError;

use crate::cpp::{cpp_clamp, cpp_max, cpp_min, cpp_min3, msvc_i32};
use crate::flat_map::{FlatMap, pack_key};
use crate::geometry::{Point, RectObj, rotate};

/// 重疊的量法（C++ 的 `OverlapMeasure`）。
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum OverlapMeasure {
    /// 交集 ÷ 聯集（IoU）。
    IntersectionOverUnion,
    /// 交集 ÷ 兩者中較小的面積：小框整個落在大框裡也算完全重疊。
    IntersectionOverSmaller,
}

/// 一個框與它的分數（C++ 的 `ScoredBox`）。`#[repr(C)]`：直接當成 FFI 的結構。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ScoredBox {
    pub rect: RectObj,
    pub score: f64,
}

// ---- 凸多邊形的裁切 ----

/// 凸多邊形，頂點放在固定大小的陣列裡：四邊形被四條邊裁切，最多 8 個頂點。不配置記憶體。
#[derive(Clone, Copy)]
struct Polygon {
    v: [Point; 12],
    n: usize,
}

impl Polygon {
    fn new() -> Self {
        Polygon {
            v: [Point::default(); 12],
            n: 0,
        }
    }

    fn push(&mut self, p: Point) {
        self.v[self.n] = p;
        self.n += 1;
    }
}

fn cross_at(a: Point, b: Point, p: Point) -> f64 {
    (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x)
}

fn signed_area(v: &[Point]) -> f64 {
    let n = v.len();
    let mut s = 0.0;
    for k in 0..n {
        let a = v[k];
        let b = v[(k + 1) % n];
        s += a.x * b.y - b.x * a.y;
    }
    0.5 * s
}

/// 矩形的四個角，方向統一成「有號面積為正」（裁切時內側在每條邊的同一邊）。
fn corners(r: &RectObj) -> [Point; 4] {
    let hw = 0.5 * r.width;
    let hh = 0.5 * r.height;
    let local = [
        Point { x: -hw, y: -hh },
        Point { x: hw, y: -hh },
        Point { x: hw, y: hh },
        Point { x: -hw, y: hh },
    ];
    let mut out = [Point::default(); 4];
    for k in 0..4 {
        let d = rotate(local[k], r.angle_deg);
        out[k] = Point {
            x: r.x + d.x,
            y: r.y + d.y,
        };
    }
    if signed_area(&out) < 0.0 {
        out.swap(1, 3);
    }
    out
}

/// Sutherland–Hodgman：用凸多邊形 clipper（有號面積為正）的每條邊裁切 subject。
fn clip(subject_corners: &[Point; 4], clipper: &[Point; 4]) -> Polygon {
    let mut subject = Polygon::new();
    for &p in subject_corners {
        subject.push(p);
    }
    let mut e = 0;
    while e < clipper.len() && subject.n > 0 {
        let a = clipper[e];
        let b = clipper[(e + 1) % clipper.len()];
        let input = subject;
        subject.n = 0;
        for k in 0..input.n {
            let p = input.v[k];
            let q = input.v[(k + 1) % input.n];
            let cp = cross_at(a, b, p);
            let cq = cross_at(a, b, q);
            if cp >= 0.0 {
                subject.push(p);
            }
            if (cp >= 0.0) != (cq >= 0.0) {
                let t = cp / (cp - cq);
                subject.push(Point {
                    x: p.x + t * (q.x - p.x),
                    y: p.y + t * (q.y - p.y),
                });
            }
        }
        e += 1;
    }
    subject
}

fn rect_area(r: &RectObj) -> f64 {
    if r.width > 0.0 && r.height > 0.0 { r.width * r.height } else { 0.0 }
}

fn half_diagonal(r: &RectObj) -> f64 {
    0.5 * (r.width * r.width + r.height * r.height).sqrt()
}

fn finite_box(r: &RectObj) -> bool {
    r.x.is_finite() && r.y.is_finite() && r.angle_deg.is_finite() && r.width.is_finite() && r.height.is_finite()
}

/// 兩個旋轉矩形的重疊面積。寬或高 ≤ 0 的矩形面積是 0。
pub fn overlap_area(a: &RectObj, b: &RectObj) -> f64 {
    if rect_area(a) <= 0.0 || rect_area(b) <= 0.0 {
        return 0.0;
    }
    // 快速排除：中心距離大於兩個半對角線的和，不可能重疊（比平方，不開根號）。
    let reach = half_diagonal(a) + half_diagonal(b);
    let dx = a.x - b.x;
    let dy = a.y - b.y;
    if dx * dx + dy * dy >= reach * reach {
        return 0.0;
    }
    let inter = clip(&corners(a), &corners(b));
    if inter.n < 3 {
        return 0.0;
    }
    cpp_min3(signed_area(&inter.v[..inter.n]).abs(), rect_area(a), rect_area(b))
}

/// 重疊比例（0..1）。任一個面積是 0 時回 0。
pub fn overlap_ratio(a: &RectObj, b: &RectObj, measure: OverlapMeasure) -> f64 {
    let area_a = rect_area(a);
    let area_b = rect_area(b);
    if area_a <= 0.0 || area_b <= 0.0 {
        return 0.0;
    }
    let inter = overlap_area(a, b);
    if inter <= 0.0 {
        return 0.0;
    }
    let denominator = if measure == OverlapMeasure::IntersectionOverUnion {
        area_a + area_b - inter
    } else {
        cpp_min(area_a, area_b)
    };
    cpp_clamp(inter / denominator, 0.0, 1.0)
}

/// 已留下的框的空間索引：「這個框和已留下的哪一個重疊超過容差？」只看附近的格子。
pub struct OverlapIndex {
    cell: f64,
    max_overlap: f64,
    measure: OverlapMeasure,
    max_half_diagonal: f64,
    boxes: Vec<RectObj>,
    next: Vec<i32>,     // 同一格的下一個框（−1 結束）
    head: FlatMap<i32>, // 格子 → 第一個框的索引＋1（0 ＝ 空）
}

impl OverlapIndex {
    /// `cell_size`：格子邊長，用框的對角線長度最合適（不是正的有限值時用 1）。
    pub fn new(cell_size: f64, max_overlap: f64, measure: OverlapMeasure) -> Result<Self, TryReserveError> {
        Ok(OverlapIndex {
            cell: if cell_size > 0.0 && cell_size.is_finite() { cell_size } else { 1.0 },
            max_overlap,
            measure,
            max_half_diagonal: 0.0,
            boxes: Vec::new(),
            next: Vec::new(),
            head: FlatMap::new(16)?,
        })
    }

    fn cell_of(&self, v: f64) -> i32 {
        let c = (v / self.cell).floor();
        cpp_clamp(c, -1.0e9, 1.0e9) as i32
    }

    /// 和已加入的任何一個框的重疊比例 > max_overlap？
    pub fn overlaps_any(&self, b: &RectObj) -> bool {
        if self.boxes.is_empty() || !finite_box(b) {
            return false;
        }
        // 會重疊的框，中心距離 < 兩個半對角線的和；換成要看幾圈格子。
        let reach = half_diagonal(b) + self.max_half_diagonal;
        let rings = msvc_i32((reach / self.cell).ceil()).max(1);
        let cx = self.cell_of(b.x);
        let cy = self.cell_of(b.y);
        for dy in -rings..=rings {
            for dx in -rings..=rings {
                let Some(&first) = self.head.find(pack_key(cx.wrapping_add(dx), cy.wrapping_add(dy))) else {
                    continue;
                };
                let mut k = first - 1;
                while k >= 0 {
                    if overlap_ratio(b, &self.boxes[k as usize], self.measure) > self.max_overlap {
                        return true;
                    }
                    k = self.next[k as usize];
                }
            }
        }
        false
    }

    /// 加入一個框（不是有限值的框不加）。配置失敗時索引不變。
    pub fn add(&mut self, b: &RectObj) -> Result<(), TryReserveError> {
        if !finite_box(b) {
            return Ok(());
        }
        self.boxes.try_reserve(1)?;
        self.next.try_reserve(1)?;
        let index = self.boxes.len() as i32;
        let key = pack_key(self.cell_of(b.x), self.cell_of(b.y));
        let first = self.head.entry(key)?;
        let previous = *first;
        *first = index + 1;
        self.boxes.push(*b);
        self.next.push(previous - 1);
        self.max_half_diagonal = cpp_max(self.max_half_diagonal, half_diagonal(b));
        Ok(())
    }

    pub fn len(&self) -> usize {
        self.boxes.len()
    }

    pub fn is_empty(&self) -> bool {
        self.boxes.is_empty()
    }
}

/// 留下的框（輸入的索引），依分數由高到低；同分時依 y、再依 x、再依索引。分數或位姿不是有限值的框直接丟掉。
pub fn keep(boxes: &[ScoredBox], max_overlap: f64, measure: OverlapMeasure) -> Result<Vec<usize>, TryReserveError> {
    let mut order: Vec<usize> = Vec::new();
    order.try_reserve_exact(boxes.len())?;
    let mut cell = 0.0;
    for (k, b) in boxes.iter().enumerate() {
        if !finite_box(&b.rect) || !b.score.is_finite() {
            continue;
        }
        order.push(k);
        cell = cpp_max(cell, 2.0 * half_diagonal(&b.rect));
    }
    // 所有值都是有限值，所以這是全序：排序的結果和 C++ 的 std::sort 相同（不論演算法）。
    order.sort_unstable_by(|&a, &b| {
        let x = &boxes[a];
        let y = &boxes[b];
        if x.score != y.score {
            return if x.score > y.score { Ordering::Less } else { Ordering::Greater };
        }
        if x.rect.y != y.rect.y {
            return if x.rect.y < y.rect.y { Ordering::Less } else { Ordering::Greater };
        }
        if x.rect.x != y.rect.x {
            return if x.rect.x < y.rect.x { Ordering::Less } else { Ordering::Greater };
        }
        a.cmp(&b)
    });
    let mut kept = OverlapIndex::new(cell, max_overlap, measure)?;
    let mut out: Vec<usize> = Vec::new();
    out.try_reserve_exact(order.len())?;
    for k in order {
        if kept.overlaps_any(&boxes[k].rect) {
            continue;
        }
        kept.add(&boxes[k].rect)?;
        out.push(k);
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn rect(x: f64, y: f64, w: f64, h: f64, angle: f64) -> RectObj {
        RectObj {
            x,
            y,
            angle_deg: angle,
            width: w,
            height: h,
        }
    }

    // 和 tests/domain/tst_nms.cpp 的 overlapAreasHaveExactValues 相同的數字。
    #[test]
    fn overlap_areas_have_exact_values() {
        let near = |a: f64, b: f64| (a - b).abs() < 1e-9;
        assert!(near(overlap_area(&rect(0., 0., 10., 10., 0.), &rect(0., 0., 10., 10., 0.)), 100.0));
        assert!(near(overlap_area(&rect(0., 0., 10., 10., 0.), &rect(5., 0., 10., 10., 0.)), 50.0));
        assert!(near(overlap_area(&rect(0., 0., 10., 10., 0.), &rect(5., 5., 10., 10., 0.)), 25.0));
        assert_eq!(overlap_area(&rect(0., 0., 10., 10., 0.), &rect(10., 0., 10., 10., 0.)), 0.0);
        assert_eq!(overlap_area(&rect(0., 0., 10., 10., 0.), &rect(30., 0., 10., 10., 0.)), 0.0);
        assert!(near(overlap_area(&rect(0., 0., 10., 10., 0.), &rect(0., 0., 4., 4., 0.)), 16.0));
        let octagon = 200.0 * (std::f64::consts::SQRT_2 - 1.0);
        assert!(near(
            overlap_area(&rect(0., 0., 10., 10., 0.), &rect(0., 0., 10., 10., 45.)),
            octagon
        ));
        assert!(near(overlap_area(&rect(0., 0., 20., 4., 0.), &rect(0., 0., 20., 4., 90.)), 16.0));
        assert_eq!(overlap_area(&rect(0., 0., 0., 10., 0.), &rect(0., 0., 10., 10., 0.)), 0.0);
        assert_eq!(overlap_area(&rect(0., 0., -3., 10., 0.), &rect(0., 0., 10., 10., 0.)), 0.0);
    }

    #[test]
    fn overlap_ratio_measures() {
        let a = rect(0., 0., 10., 10., 0.);
        let small = rect(0., 0., 4., 4., 0.);
        assert!((overlap_ratio(&a, &small, OverlapMeasure::IntersectionOverUnion) - 0.16).abs() < 1e-12);
        assert_eq!(overlap_ratio(&a, &small, OverlapMeasure::IntersectionOverSmaller), 1.0);
        assert_eq!(
            overlap_ratio(&a, &rect(0., 0., 0., 4., 0.), OverlapMeasure::IntersectionOverSmaller),
            0.0
        );
    }

    #[test]
    fn keep_suppresses_lower_scores_and_skips_non_finite() {
        let boxes = [
            ScoredBox {
                rect: rect(0., 0., 10., 10., 0.),
                score: 0.5,
            },
            ScoredBox {
                rect: rect(1., 0., 10., 10., 0.),
                score: 0.9,
            },
            ScoredBox {
                rect: rect(50., 0., 10., 10., 0.),
                score: 0.7,
            },
            ScoredBox {
                rect: rect(f64::NAN, 0., 10., 10., 0.),
                score: 1.0,
            },
            ScoredBox {
                rect: rect(80., 0., 10., 10., 0.),
                score: f64::INFINITY,
            },
        ];
        assert_eq!(keep(&boxes, 0.5, OverlapMeasure::IntersectionOverUnion).unwrap(), vec![1, 2]);
    }

    #[test]
    fn keep_breaks_ties_by_y_then_x_then_index() {
        let boxes = [
            ScoredBox {
                rect: rect(20., 10., 4., 4., 0.),
                score: 0.8,
            },
            ScoredBox {
                rect: rect(10., 10., 4., 4., 0.),
                score: 0.8,
            },
            ScoredBox {
                rect: rect(30., 0., 4., 4., 0.),
                score: 0.8,
            },
            ScoredBox {
                rect: rect(10., 10., 4., 4., 0.),
                score: 0.8,
            },
        ];
        // 3 和 1 完全重疊：同分、同位置，索引小的（1）留下。
        assert_eq!(keep(&boxes, 0.5, OverlapMeasure::IntersectionOverUnion).unwrap(), vec![2, 1, 0]);
    }

    #[test]
    fn overlap_index_finds_neighbours_in_other_cells() {
        let mut index = OverlapIndex::new(5.0, 0.1, OverlapMeasure::IntersectionOverSmaller).unwrap();
        index.add(&rect(0., 0., 40., 40., 0.)).unwrap();
        assert_eq!(index.len(), 1);
        // 中心在好幾格以外，但大框蓋到它：要看夠多圈格子才找得到。
        assert!(index.overlaps_any(&rect(18., 18., 4., 4., 0.)));
        assert!(!index.overlaps_any(&rect(40., 40., 4., 4., 0.)));
        index.add(&rect(f64::INFINITY, 0., 4., 4., 0.)).unwrap();
        assert_eq!(index.len(), 1);
    }
}
