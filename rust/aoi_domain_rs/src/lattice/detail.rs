//! 晶格模組內部共用的小工具（`lattice_detail.h`）：向量運算、四捨五入、亂數、指派器、容差、格子表。

use crate::cpp::{cpp_min, msvc_i32};
use crate::fallible::{Res, vec_filled};
use crate::flat_map::{FlatMap, pack_key};
use crate::geometry::Point;
use crate::lattice::basis::{Basis, Cell, MAX_INDEX};

pub fn length(v: Point) -> f64 {
    (v.x * v.x + v.y * v.y).sqrt()
}

pub fn dot(a: Point, b: Point) -> f64 {
    a.x * b.x + a.y * b.y
}

pub fn cross(a: Point, b: Point) -> f64 {
    a.x * b.y - a.y * b.x
}

pub fn minus(a: Point, b: Point) -> Point {
    Point {
        x: a.x - b.x,
        y: a.y - b.y,
    }
}

pub fn times(a: Point, s: f64) -> Point {
    Point { x: a.x * s, y: a.y * s }
}

pub fn finite_point(p: Point) -> bool {
    p.x.is_finite() && p.y.is_finite()
}

pub fn squared_distance(a: Point, b: Point) -> f64 {
    let dx = a.x - b.x;
    let dy = a.y - b.y;
    dx * dx + dy * dy
}

/// `fastRound`：四捨五入（遠離 0），超出 ±kMaxIndex 的夾住；NaN 得到 −kMaxIndex。
pub fn fast_round(a: f64) -> i32 {
    if !(a > -MAX_INDEX) {
        return msvc_i32(-MAX_INDEX);
    }
    if !(a < MAX_INDEX) {
        return msvc_i32(MAX_INDEX);
    }
    msvc_i32(if a < 0.0 { a - 0.5 } else { a + 0.5 })
}

/// 把位移向量放到同一個半平面。
pub fn half_plane(d: Point) -> Point {
    if d.x < 0.0 || (d.x == 0.0 && d.y < 0.0) {
        return Point { x: -d.x, y: -d.y };
    }
    d
}

/// `reach`：`max(|i|, |j|)`（`std::abs(INT_MIN)` 在 MSVC 是 INT_MIN，`wrapping_abs` 相同）。
pub fn reach(c: Cell) -> i32 {
    c.i.wrapping_abs().max(c.j.wrapping_abs())
}

/// splitmix64（`SplitMix64`）。
pub struct SplitMix64 {
    state: u64,
}

impl SplitMix64 {
    pub fn new(seed: u64) -> Self {
        SplitMix64 { state: seed }
    }

    pub fn next_u64(&mut self) -> u64 {
        self.state = self.state.wrapping_add(0x9E37_79B9_7F4A_7C15);
        let mut z = self.state;
        z = (z ^ (z >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
        z = (z ^ (z >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
        z ^ (z >> 31)
    }

    pub fn below(&mut self, n: usize) -> usize {
        (self.next_u64() % n as u64) as usize
    }
}

#[derive(Clone, Copy, Debug)]
pub struct Assigned {
    pub cell: Cell,
    /// 到格點距離的平方
    pub residual2: f64,
}

impl Default for Assigned {
    fn default() -> Self {
        Assigned {
            cell: Cell::default(),
            residual2: f64::INFINITY,
        }
    }
}

/// 指派器（`Assigner`）：反矩陣先算好，每個點只要幾次乘加就換成格子座標。
pub struct Assigner {
    b: Basis,
    ok: bool,
    au: f64,
    bu: f64,
    av: f64,
    bv: f64,
}

impl Assigner {
    pub fn new(basis: &Basis) -> Self {
        let mut a = Assigner {
            b: *basis,
            ok: false,
            au: 0.0,
            bu: 0.0,
            av: 0.0,
            bv: 0.0,
        };
        let (u, v) = (basis.u, basis.v);
        if !basis.has_v {
            let uu = dot(u, u);
            a.ok = uu > 0.0;
            if a.ok {
                a.au = u.x / uu;
                a.bu = u.y / uu;
            }
            a.b.v = Point { x: 0.0, y: 0.0 };
        } else {
            let det = cross(u, v);
            a.ok = det.abs() > 1e-12 * (dot(u, u) + dot(v, v));
            if a.ok {
                a.au = v.y / det;
                a.bu = -v.x / det;
                a.av = -u.y / det;
                a.bv = u.x / det;
            }
        }
        a
    }

    /// 連續的格子座標 (a, b)；一維時 b 是 0。
    pub fn coordinates(&self, p: Point) -> Point {
        let dx = p.x - self.b.origin.x;
        let dy = p.y - self.b.origin.y;
        Point {
            x: dx * self.au + dy * self.bu,
            y: dx * self.av + dy * self.bv,
        }
    }

    pub fn assign(&self, p: Point) -> Assigned {
        if !self.ok {
            return Assigned::default();
        }
        let dx = p.x - self.b.origin.x;
        let dy = p.y - self.b.origin.y;
        let i = fast_round(dx * self.au + dy * self.bu);
        let j = if self.b.has_v { fast_round(dx * self.av + dy * self.bv) } else { 0 };
        let (fi, fj) = (f64::from(i), f64::from(j));
        let ex = dx - fi * self.b.u.x - fj * self.b.v.x;
        let ey = dy - fi * self.b.u.y - fj * self.b.v.y;
        Assigned {
            cell: Cell { i, j },
            residual2: ex * ex + ey * ey,
        }
    }
}

/// 內點容差：最短格距的一個比例（`toleranceFor`）。
pub fn tolerance_for(b: &Basis, fraction: f64) -> f64 {
    let lu = length(b.u);
    if !(lu > 0.0) {
        return 0.0;
    }
    if !b.has_v {
        return fraction * lu;
    }
    let row_spacing = cross(b.u, b.v).abs() / lu;
    fraction * cpp_min(lu, row_spacing)
}

/// 最小平方法的解換進目前的基底（`mergeSolution`）。
pub fn merge_solution(current: &Basis, solved: &Basis) -> Basis {
    if current.has_v && !solved.has_v {
        let mut b = *current;
        b.origin = solved.origin;
        b.u = solved.u;
        return b;
    }
    *solved
}

/// 一個假設擬合出來的候選（`LatticeCandidate`）：成員是點的索引，cells 與它一一對應。
#[derive(Clone, Debug, Default)]
pub struct Candidate {
    pub basis: Basis,
    pub tolerance: f64,
    pub members: Vec<usize>,
    pub cells: Vec<Cell>,
}

/// 格子 → 整數的表（`CellTable`；−1 ＝ 空）。範圍不大時用密集陣列，很大時用雜湊表。
pub struct CellTable {
    min_i: i32,
    max_i: i32,
    min_j: i32,
    max_j: i32,
    width: usize,
    values: Vec<i32>,
    sparse: Option<FlatMap<i32>>,
}

impl CellTable {
    pub fn new(min_i: i32, max_i: i32, min_j: i32, max_j: i32, expected: usize) -> Res<Self> {
        let w = f64::from(max_i) - f64::from(min_i) + 1.0;
        let h = f64::from(max_j) - f64::from(min_j) + 1.0;
        let dense = w * h <= 64.0 * expected as f64 + 4096.0;
        let mut t = CellTable {
            min_i,
            max_i,
            min_j,
            max_j,
            width: 1,
            values: Vec::new(),
            sparse: None,
        };
        if dense {
            t.width = w as usize;
            t.values = vec_filled((w * h) as usize, -1)?;
        } else {
            t.sparse = Some(FlatMap::new(expected)?);
        }
        Ok(t)
    }

    /// cells 的每一格放它在 cells 裡的索引（`CellTable::of`）。
    pub fn of(cells: &[Cell]) -> Res<Self> {
        let (mut min_i, mut max_i, mut min_j, mut max_j) = (0, 0, 0, 0);
        if let Some(f) = cells.first() {
            min_i = f.i;
            max_i = f.i;
            min_j = f.j;
            max_j = f.j;
        }
        for c in cells {
            min_i = min_i.min(c.i);
            max_i = max_i.max(c.i);
            min_j = min_j.min(c.j);
            max_j = max_j.max(c.j);
        }
        let mut t = CellTable::new(min_i, max_i, min_j, max_j, cells.len())?;
        for (k, c) in cells.iter().enumerate() {
            t.set(c.i, c.j, k as i32)?;
        }
        Ok(t)
    }

    fn index(&self, i: i32, j: i32) -> usize {
        (j.wrapping_sub(self.min_j) as usize) * self.width + i.wrapping_sub(self.min_i) as usize
    }

    pub fn get(&self, i: i32, j: i32) -> i32 {
        if i < self.min_i || i > self.max_i || j < self.min_j || j > self.max_j {
            return -1;
        }
        match &self.sparse {
            None => self.values[self.index(i, j)],
            Some(map) => map.find(pack_key(i, j)).map_or(-1, |v| v - 1),
        }
    }

    pub fn set(&mut self, i: i32, j: i32, value: i32) -> Res<()> {
        let index = self.index(i, j);
        match &mut self.sparse {
            None => self.values[index] = value,
            Some(map) => *map.entry(pack_key(i, j))? = value.wrapping_add(1),
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fast_round_rounds_away_from_zero_and_clamps() {
        assert_eq!(fast_round(2.5), 3);
        assert_eq!(fast_round(-2.5), -3);
        assert_eq!(fast_round(1.0e12), 100_000_000);
        assert_eq!(fast_round(f64::NAN), -100_000_000);
    }

    #[test]
    fn splitmix_matches_the_cpp_sequence() {
        // 和 tests/domain/LatticeTestSupport.h 的 Rng(1) 相同的第一個值。
        let mut r = SplitMix64::new(1);
        assert_eq!(r.next_u64(), 0x910A_2DEC_8902_5CC1);
    }

    #[test]
    fn cell_table_dense_and_sparse() {
        let cells = [Cell { i: 0, j: 0 }, Cell { i: 3, j: 1 }];
        let t = CellTable::of(&cells).unwrap();
        assert_eq!(t.get(3, 1), 1);
        assert_eq!(t.get(1, 1), -1);
        let far = [Cell { i: 0, j: 0 }, Cell { i: 100_000, j: 100_000 }];
        let s = CellTable::of(&far).unwrap();
        assert!(s.sparse.is_some());
        assert_eq!(s.get(100_000, 100_000), 1);
        assert_eq!(s.get(5, 5), -1);
    }
}
