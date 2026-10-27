//! 晶格的最小平方法：正規方程累加器（`lattice_accumulator.h`）。只存 16 個總和，加點、拿掉、求解都是 O(1)。
//!
//! `#[repr(C)]`：欄位順序和 C 的 `AoiRsLatticeSums`、C++ 的 `detail::LatticeSums` 相同（C++ 的
//! `LatticeAccumulator` 經 FFI 傳整份狀態進來，算完再寫回去）。總和相對於參考點與參考格子。

use crate::cpp::cpp_max;
use crate::geometry::Point;
use crate::lattice::basis::{Basis, Cell};

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Accumulator {
    pub reference_point: Point,
    pub reference_cell: Cell,
    pub count: i64,
    // 索引的總和（相對於參考格子；整數，精確）
    pub n: f64,
    pub si: f64,
    pub sj: f64,
    pub sii: f64,
    pub sij: f64,
    pub sjj: f64,
    // 座標的總和（相對於參考點）
    pub sx: f64,
    pub six: f64,
    pub sjx: f64,
    pub sxx: f64,
    pub sy: f64,
    pub siy: f64,
    pub sjy: f64,
    pub syy: f64,
}

const _: () = assert!(std::mem::size_of::<Accumulator>() == 16 + 8 + 8 + 14 * 8);

impl Accumulator {
    pub fn new(reference_point: Point, reference_cell: Cell) -> Self {
        Accumulator {
            reference_point,
            reference_cell,
            ..Accumulator::default()
        }
    }

    pub fn add(&mut self, cell: Cell, p: Point) {
        self.accumulate(cell, p, 1.0);
    }

    pub fn accumulate(&mut self, cell: Cell, p: Point, weight: f64) {
        let di = f64::from(cell.i) - f64::from(self.reference_cell.i);
        let dj = f64::from(cell.j) - f64::from(self.reference_cell.j);
        let x = p.x - self.reference_point.x;
        let y = p.y - self.reference_point.y;
        self.count = self.count.wrapping_add(if weight > 0.0 { 1 } else { -1 });
        self.n += weight;
        self.si += weight * di;
        self.sj += weight * dj;
        self.sii += weight * di * di;
        self.sij += weight * di * dj;
        self.sjj += weight * dj * dj;
        self.sx += weight * x;
        self.six += weight * di * x;
        self.sjx += weight * dj * x;
        self.sxx += weight * x * x;
        self.sy += weight * y;
        self.siy += weight * di * y;
        self.sjy += weight * dj * y;
        self.syy += weight * y * y;
    }

    pub fn two_dimensional(&self) -> bool {
        if self.count < 3 {
            return false;
        }
        let sii = self.n * self.sii - self.si * self.si;
        let sjj = self.n * self.sjj - self.sj * self.sj;
        let sij = self.n * self.sij - self.si * self.sj;
        if sjj < 0.5 || sii < 0.5 {
            return false;
        }
        let det = sii * sjj - sij * sij;
        det > cpp_max(0.5, 1e-12 * sii * sjj)
    }

    pub fn solve(&self) -> Option<Basis> {
        if self.count < 2 {
            return None;
        }
        let sii = self.n * self.sii - self.si * self.si;
        let six = self.n * self.six - self.si * self.sx;
        let siy = self.n * self.siy - self.si * self.sy;

        let mut basis = Basis::default();
        let (ox, oy) = if self.two_dimensional() {
            let sjj = self.n * self.sjj - self.sj * self.sj;
            let sij = self.n * self.sij - self.si * self.sj;
            let sjx = self.n * self.sjx - self.sj * self.sx;
            let sjy = self.n * self.sjy - self.sj * self.sy;
            let det = sii * sjj - sij * sij;
            basis.has_v = true;
            basis.u = Point {
                x: (sjj * six - sij * sjx) / det,
                y: (sjj * siy - sij * sjy) / det,
            };
            basis.v = Point {
                x: (sii * sjx - sij * six) / det,
                y: (sii * sjy - sij * siy) / det,
            };
            (
                (self.sx - basis.u.x * self.si - basis.v.x * self.sj) / self.n,
                (self.sy - basis.u.y * self.si - basis.v.y * self.sj) / self.n,
            )
        } else {
            if sii < 0.5 {
                return None; // 所有點同一個 i：沒有方向
            }
            basis.has_v = false;
            basis.u = Point {
                x: six / sii,
                y: siy / sii,
            };
            basis.v = Point { x: 0.0, y: 0.0 };
            ((self.sx - basis.u.x * self.si) / self.n, (self.sy - basis.u.y * self.si) / self.n)
        };
        // 中心化的原點換回影像座標：origin ＝ 參考點 ＋ o′ − ri·u − rj·v。
        let ri = f64::from(self.reference_cell.i);
        let rj = if basis.has_v { f64::from(self.reference_cell.j) } else { 0.0 };
        basis.origin = Point {
            x: self.reference_point.x + ox - ri * basis.u.x - rj * basis.v.x,
            y: self.reference_point.y + oy - ri * basis.u.y - rj * basis.v.y,
        };
        let finite = [basis.origin.x, basis.origin.y, basis.u.x, basis.u.y, basis.v.x, basis.v.y];
        if finite.iter().any(|v| !v.is_finite()) {
            return None;
        }
        Some(basis)
    }

    pub fn residual_sum_of_squares(&self, basis: &Basis) -> f64 {
        if self.count <= 0 {
            return 0.0;
        }
        let ri = f64::from(self.reference_cell.i);
        let rj = f64::from(self.reference_cell.j);
        let vx = if basis.has_v { basis.v.x } else { 0.0 };
        let vy = if basis.has_v { basis.v.y } else { 0.0 };
        // 中心化座標下的參數：x′ ＝ ox′ ＋ ux·di ＋ vx·dj。
        let ox = basis.origin.x - self.reference_point.x + basis.u.x * ri + vx * rj;
        let oy = basis.origin.y - self.reference_point.y + basis.u.y * ri + vy * rj;
        let axis = |o: f64, u: f64, v: f64, s: f64, si: f64, sj: f64, ss: f64| {
            let cross = o * s + u * si + v * sj;
            let quad = o * o * self.n + u * u * self.sii + v * v * self.sjj + 2.0 * (o * u * self.si + o * v * self.sj + u * v * self.sij);
            ss - 2.0 * cross + quad
        };
        let rss =
            axis(ox, basis.u.x, vx, self.sx, self.six, self.sjx, self.sxx) + axis(oy, basis.u.y, vy, self.sy, self.siy, self.sjy, self.syy);
        cpp_max(0.0, rss)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn exact_grid_solves_exactly() {
        let mut a = Accumulator::new(Point { x: 10.0, y: 20.0 }, Cell { i: 0, j: 0 });
        for i in 0..4 {
            for j in 0..3 {
                a.add(
                    Cell { i, j },
                    Point {
                        x: 10.0 + 30.0 * f64::from(i),
                        y: 20.0 + 25.0 * f64::from(j),
                    },
                );
            }
        }
        assert!(a.two_dimensional());
        let b = a.solve().unwrap();
        assert!((b.u.x - 30.0).abs() < 1e-12 && (b.v.y - 25.0).abs() < 1e-12);
        assert!(a.residual_sum_of_squares(&b) < 1e-9);
        a.accumulate(Cell { i: 3, j: 2 }, Point { x: 100.0, y: 70.0 }, -1.0);
        assert_eq!(a.count, 11);
    }

    #[test]
    fn single_column_has_no_direction() {
        let mut a = Accumulator::new(Point::default(), Cell::default());
        a.add(Cell { i: 0, j: 0 }, Point { x: 0.0, y: 0.0 });
        assert!(a.solve().is_none());
        a.add(Cell { i: 0, j: 1 }, Point { x: 0.0, y: 5.0 });
        assert!(a.solve().is_none());
    }
}
