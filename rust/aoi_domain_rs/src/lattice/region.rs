//! 晶格的索引區域（`lattice_region.h`）：哪些格子算「陣列的範圍之內」。缺格數（規則 2）、涵蓋率與缺格的預測都用它。
//!
//! R07.2（2026-10-28）之前，範圍是內點索引的外接矩形（`min_i..=max_i × min_j..=max_j`）。兩種真實的板子它都不對：
//!  - 交錯排列的陣列（相鄰兩欄差半個格距，例如密排端子：u ＝ (0, 154)、v ＝ (133, 77)）在影像上是矩形，在索引空間
//!    卻是平行四邊形，外接矩形多出兩個三角形的空格，規則 2 把二維晶格擋掉、退回一維，缺格一個都不補。
//!  - 陣列中間有大片空的區域（兩個長槽）時，空的區域在外接矩形裡，整片都被當成缺格（使用者看到幾百個假的推算框）。
//!
//! 現在二維的區域是三個條件的交集（一維沒有變：只有 i 的範圍）：
//!  1. 索引的外接矩形（和之前一樣）。
//!  2. 影像空間裡沿 u 的外接矩形：格子中心投影到 û 與它的法向量上，要落在所有內點格子中心投影的範圍之內（容差四分之一個
//!     最短格距：有雜訊時擬合出來的 v 帶一點 u 方向的分量，矩形陣列缺角的格子會多出幾個像素）。交錯陣列的區塊沿 u 對齊時
//!     被精確描述；沒有對齊的斜切陣列仍是外接矩形。
//!  3. 空格要有兩側的證據（規則 4「空 2 格以上就拆開」用在缺格上）：沿 u 或沿 w 的兩側、各在 3 格之內都有內點；或者是角落
//!     （沿 u 與沿 w 各有一個相鄰的內點）。w 是和 u 最接近垂直的格距向量（v、2v − u、2v + u 之中取一個）：交錯陣列的 v 是
//!     斜的，用它判斷「兩側」會從空的區域漏進陣列；w 是同一列的下一個元件。大片空的區域裡的格子沒有兩側的證據，不算。
//!     代價：角落的缺件要兩個相鄰的元件都找到才補（缺角而且相鄰也缺的，不補）。
//!
//! 運算順序和 C++ 的 `lattice_region.cpp` 相同（差異測試逐筆比對）。

use crate::fallible::Res;
use crate::lattice::basis::{Basis, Cell};
use crate::lattice::detail::CellTable;

/// 空格兩側的內點最遠在幾格之內（＝ `LatticeOptions::maxGapCells` 的預設 2 ＋ 1）。
const SUPPORT_REACH: i32 = 3;

/// 一組晶格的索引區域。
pub struct Region {
    pub min_i: i32,
    pub max_i: i32,
    pub min_j: i32,
    pub max_j: i32,
    has_v: bool,
    /// 有沒有影像空間的範圍與佔用表（二維、u 不退化、至少一個格子）；沒有就只是外接矩形。
    aligned: bool,
    ux: f64,
    uy: f64,
    s_min: f64,
    s_max: f64,
    t_min: f64,
    t_max: f64,
    eps: f64,
    /// 沿 w 走一格的索引位移。
    wi: i32,
    wj: i32,
    occupied: Option<CellTable>,
}

/// 一次走訪最多這麼多個格子（和預測器相同的防呆）。
const MAX_VISITED: f64 = 16.0 * 1024.0 * 1024.0;

impl Region {
    /// 內點的格子（`cells`）與索引範圍 → 區域。`min/max` 是呼叫端已經算好的索引範圍（模型的欄位）。
    pub fn of(b: &Basis, min_i: i32, max_i: i32, min_j: i32, max_j: i32, cells: &[Cell]) -> Res<Region> {
        let mut r = Region {
            min_i,
            max_i,
            min_j: if b.has_v { min_j } else { 0 },
            max_j: if b.has_v { max_j } else { 0 },
            has_v: b.has_v,
            aligned: false,
            ux: 0.0,
            uy: 0.0,
            s_min: 0.0,
            s_max: 0.0,
            t_min: 0.0,
            t_max: 0.0,
            eps: 0.0,
            wi: 0,
            wj: 1,
            occupied: None,
        };
        if !b.has_v || cells.is_empty() {
            return Ok(r);
        }
        let uu = b.u.x * b.u.x + b.u.y * b.u.y;
        let lu = uu.sqrt();
        if !(lu > 0.0) {
            return Ok(r);
        }
        r.ux = b.u.x / lu;
        r.uy = b.u.y / lu;
        let row_spacing = (b.u.x * b.v.y - b.u.y * b.v.x).abs() / lu;
        r.eps = 0.25 * if row_spacing < lu { row_spacing } else { lu };
        for (k, c) in cells.iter().enumerate() {
            let p = b.at(c.i, c.j);
            let s = p.x * r.ux + p.y * r.uy;
            let t = p.y * r.ux - p.x * r.uy;
            if k == 0 {
                r.s_min = s;
                r.s_max = s;
                r.t_min = t;
                r.t_max = t;
            } else {
                if s < r.s_min {
                    r.s_min = s;
                }
                if s > r.s_max {
                    r.s_max = s;
                }
                if t < r.t_min {
                    r.t_min = t;
                }
                if t > r.t_max {
                    r.t_max = t;
                }
            }
        }
        // w：v、2v − u、2v + u 之中和 u 最接近垂直的（|cos| 最小；同樣時取前面的）。
        let candidates: [(i32, i32); 3] = [(0, 1), (-1, 2), (1, 2)];
        let mut best = 0usize;
        let mut best_cos = 0.0;
        for (k, &(a, c)) in candidates.iter().enumerate() {
            let wx = f64::from(a) * b.u.x + f64::from(c) * b.v.x;
            let wy = f64::from(a) * b.u.y + f64::from(c) * b.v.y;
            let ww = wx * wx + wy * wy;
            if !(ww > 0.0) {
                continue;
            }
            let cos = (wx * b.u.x + wy * b.u.y).abs() / (ww.sqrt() * lu);
            if k == 0 || cos < best_cos {
                best = k;
                best_cos = cos;
            }
        }
        r.wi = candidates[best].0;
        r.wj = candidates[best].1;
        r.occupied = Some(CellTable::of(cells)?);
        r.aligned = true;
        Ok(r)
    }

    fn occupied(&self, i: i32, j: i32) -> bool {
        match &self.occupied {
            Some(t) => t.get(i, j) >= 0,
            None => false,
        }
    }

    /// 沿 (di, dj) 的正負兩個方向，各在 SUPPORT_REACH 格之內都有內點？
    fn between(&self, i: i32, j: i32, di: i32, dj: i32) -> bool {
        self.reaches(i, j, di, dj) && self.reaches(i, j, -di, -dj)
    }

    fn reaches(&self, i: i32, j: i32, di: i32, dj: i32) -> bool {
        for step in 1..=SUPPORT_REACH {
            if self.occupied(i.wrapping_add(di.wrapping_mul(step)), j.wrapping_add(dj.wrapping_mul(step))) {
                return true;
            }
        }
        false
    }

    fn adjacent(&self, i: i32, j: i32, di: i32, dj: i32) -> bool {
        self.occupied(i.wrapping_add(di), j.wrapping_add(dj)) || self.occupied(i.wrapping_sub(di), j.wrapping_sub(dj))
    }

    /// 格子 (i, j) 在區域裡？內點的格子一定在；空格要在影像空間的範圍內、而且有兩側（或角落）的證據。
    pub fn contains(&self, b: &Basis, i: i32, j: i32) -> bool {
        if i < self.min_i || i > self.max_i || j < self.min_j || j > self.max_j {
            return false;
        }
        if !self.aligned {
            return true;
        }
        if self.occupied(i, j) {
            return true;
        }
        if !self.in_extent(b, i, j) {
            return false;
        }
        self.between(i, j, 1, 0)
            || self.between(i, j, self.wi, self.wj)
            || (self.adjacent(i, j, 1, 0) && self.adjacent(i, j, self.wi, self.wj))
    }

    /// 八個鄰居（沿 u、沿 w、斜的）有沒有內點。
    fn adjacent_any(&self, i: i32, j: i32) -> bool {
        for a in -1i32..=1 {
            for c in -1i32..=1 {
                if a == 0 && c == 0 {
                    continue;
                }
                let di = a.wrapping_add(c.wrapping_mul(self.wi));
                let dj = c.wrapping_mul(self.wj);
                if self.occupied(i.wrapping_add(di), j.wrapping_add(dj)) {
                    return true;
                }
            }
        }
        false
    }

    /// 規則 2 用的格子數：內點的格子 ＋ 區域裡有證據的空格 ＋ 區域裡貼著內點（八鄰）的空格。比 `cell_count` 多算貼著
    /// 內點的空格，規則 2 才擋得住稀疏的假晶格（散佈的點湊出的晶格每個內點旁邊都是空格）；大片空的區域只算貼邊那一圈。
    pub fn evidence_count(&self, b: &Basis) -> usize {
        if !self.aligned {
            return self.box_count();
        }
        let w = f64::from(self.max_i) - f64::from(self.min_i) + 1.0;
        let h = f64::from(self.max_j) - f64::from(self.min_j) + 1.0;
        if w < 1.0 || h < 1.0 || w * h > MAX_VISITED {
            return self.box_count();
        }
        let mut n = 0usize;
        for i in self.min_i..=self.max_i {
            for j in self.min_j..=self.max_j {
                if self.contains(b, i, j) || (self.in_extent(b, i, j) && self.adjacent_any(i, j)) {
                    n += 1;
                }
            }
        }
        n
    }

    fn in_extent(&self, b: &Basis, i: i32, j: i32) -> bool {
        let p = b.at(i, j);
        let s = p.x * self.ux + p.y * self.uy;
        let t = p.y * self.ux - p.x * self.uy;
        s >= self.s_min - self.eps && s <= self.s_max + self.eps && t >= self.t_min - self.eps && t <= self.t_max + self.eps
    }

    /// 外接矩形的格子數（`cellCount()` 之前的定義；size_t 的乘法，溢位時繞回，和 C++ 相同）。
    pub fn box_count(&self) -> usize {
        let w = self.max_i.wrapping_sub(self.min_i).wrapping_add(1) as usize;
        let h = if self.has_v {
            self.max_j.wrapping_sub(self.min_j).wrapping_add(1) as usize
        } else {
            1
        };
        w.wrapping_mul(h)
    }

    /// 區域裡的格子數（內點的格子加上有證據的空格）。不是沿 u 對齊的、或外接矩形大得不合理時，就是外接矩形的格子數。
    pub fn cell_count(&self, b: &Basis) -> usize {
        if !self.aligned {
            return self.box_count();
        }
        let w = f64::from(self.max_i) - f64::from(self.min_i) + 1.0;
        let h = f64::from(self.max_j) - f64::from(self.min_j) + 1.0;
        if w < 1.0 || h < 1.0 || w * h > MAX_VISITED {
            return self.box_count();
        }
        let mut n = 0usize;
        for i in self.min_i..=self.max_i {
            for j in self.min_j..=self.max_j {
                if self.contains(b, i, j) {
                    n += 1;
                }
            }
        }
        n
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::geometry::Point;

    fn orthogonal() -> Basis {
        Basis {
            origin: Point { x: 0.0, y: 0.0 },
            u: Point { x: 10.0, y: 0.0 },
            v: Point { x: 0.0, y: 10.0 },
            has_v: true,
        }
    }

    #[test]
    fn a_rectangle_with_a_missing_corner_still_contains_the_corner() {
        let b = orthogonal();
        let mut cells = Vec::new();
        for i in 0..4 {
            for j in 0..3 {
                if (i == 0 && j == 0) || (i == 2 && j == 1) {
                    continue; // 缺角、缺一個中間的
                }
                cells.push(Cell { i, j });
            }
        }
        let r = Region::of(&b, 0, 3, 0, 2, &cells).unwrap();
        assert_eq!(r.cell_count(&b), 12);
        assert!(r.contains(&b, 0, 0)); // 角落：(1, 0) 與 (0, 1) 都在
        assert!(r.contains(&b, 2, 1)); // 中間：兩側都在
        assert!(!r.contains(&b, 4, 0));
    }

    #[test]
    fn a_corner_without_adjacent_inliers_is_not_a_hole() {
        let b = orthogonal();
        let mut cells = Vec::new();
        for i in 0..4 {
            for j in 0..3 {
                if (i == 0 && j == 0) || (i == 1 && j == 0) || (i == 0 && j == 1) {
                    continue; // 缺角，而且相鄰的兩個也缺
                }
                cells.push(Cell { i, j });
            }
        }
        let r = Region::of(&b, 0, 3, 0, 2, &cells).unwrap();
        assert!(!r.contains(&b, 0, 0));
        assert!(r.contains(&b, 1, 0)); // 沿 u 的兩側：(2, 0)… 不，只有一側；沿 w：(1, 1) 與… 角落規則：(2, 0) 相鄰、(1, 1) 相鄰
        assert_eq!(r.cell_count(&b), 11);
    }

    #[test]
    fn a_big_empty_area_inside_the_box_is_not_full_of_holes() {
        // 兩個 3×6 的區塊，中間空 6 格（像板子上的長槽），左邊一條 1×15 的邊把它們連起來。
        let b = orthogonal();
        let mut cells = Vec::new();
        for j in 0..15 {
            cells.push(Cell { i: 0, j });
        }
        for i in 1..4 {
            for j in 0..3 {
                cells.push(Cell { i, j });
                cells.push(Cell { i, j: j + 9 });
            }
        }
        let r = Region::of(&b, 0, 3, 0, 14, &cells).unwrap();
        // 空的區域 i ∈ 1..4、j ∈ 3..9：沿 u 只有一側（i ＝ 0 那條邊）、沿 w 兩側都超過 3 格 → 不算；角落規則：
        // (1, 3) 沿 u 相鄰 (0, 3)、沿 w 相鄰 (1, 2) → 算（槽口貼著元件的那一格，和板子上的情況相同），(1, 8) 同理；
        // 右邊區塊外的 (1, 12) 也是（(0, 12) 與 (1, 11) 相鄰）。其餘的空格都不算。
        assert_eq!(r.cell_count(&b), cells.len() + 3);
        assert!(r.contains(&b, 1, 3));
        assert!(r.contains(&b, 1, 8));
        assert!(r.contains(&b, 1, 12));
        assert!(!r.contains(&b, 2, 5));
        assert!(!r.contains(&b, 1, 6));
        assert!(!r.contains(&b, 2, 12));
    }

    #[test]
    fn a_staggered_block_excludes_the_corners_of_its_index_box() {
        // 交錯陣列：影像上 10 欄 × 6 列的矩形區塊，欄距 266、列距 154，奇數欄往下差半個列距（77）。
        let b = Basis {
            origin: Point { x: 600.0, y: 400.0 },
            u: Point { x: 0.0, y: 154.0 },
            v: Point { x: 266.0, y: 77.0 },
            has_v: true,
        };
        let mut block = Vec::new();
        for col in 0..10 {
            for row in 0..6 {
                let p = Point {
                    x: 600.0 + 266.0 * f64::from(col),
                    y: 400.0 + 154.0 * f64::from(row) + 77.0 * f64::from(col % 2),
                };
                let c = b.coordinates(p).unwrap();
                block.push(Cell {
                    i: c.x.round() as i32,
                    j: c.y.round() as i32,
                });
            }
        }
        let (mut min_i, mut max_i) = (i32::MAX, i32::MIN);
        for c in &block {
            min_i = min_i.min(c.i);
            max_i = max_i.max(c.i);
        }
        let r = Region::of(&b, min_i, max_i, 0, 9, &block).unwrap();
        assert!(r.box_count() > 60);
        assert_eq!(r.cell_count(&b), 60);
        // w ＝ 2v − u ＝ (532, 0)：同一列隔一欄的元件。
        assert_eq!((r.wi, r.wj), (-1, 2));
        for c in &block {
            assert!(r.contains(&b, c.i, c.j));
        }
    }

    #[test]
    fn one_dimensional_is_the_index_range() {
        let b = Basis {
            origin: Point { x: 0.0, y: 0.0 },
            u: Point { x: 10.0, y: 0.0 },
            v: Point { x: 0.0, y: 0.0 },
            has_v: false,
        };
        let r = Region::of(&b, 0, 6, 0, 0, &[Cell { i: 0, j: 0 }, Cell { i: 6, j: 0 }]).unwrap();
        assert_eq!(r.cell_count(&b), 7);
        assert!(r.contains(&b, 3, 0));
    }
}
