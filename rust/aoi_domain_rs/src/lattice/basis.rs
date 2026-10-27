//! 晶格的幾何：格子索引與基底（`lattice_basis.h`）。格子 (i, j) 的中心 ＝ origin ＋ i·u ＋ j·v；一維時沒有 v、j 恆為 0。

use crate::geometry::Point;

/// `LatticeIndex`。`#[repr(C)]`：和 C 的 `AoiRsCell` 相同。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Cell {
    pub i: i32,
    pub j: i32,
}

/// `LatticeBasis`（C 的 `AoiRsBasis` 用 `int32_t` 表示 `has_v`，轉換在 `ffi_lattice`）。
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Basis {
    pub origin: Point,
    pub u: Point,
    pub v: Point,
    pub has_v: bool,
}

/// `LatticeBasis::kMaxIndex`：離原點超過這麼多格的點夾住（不會溢位）。
pub const MAX_INDEX: f64 = 1.0e8;

impl Basis {
    /// `at(i, j)`：運算順序和 C++ 相同（`origin.x + i * u.x + j * v.x` 由左到右）。
    pub fn at(&self, i: i32, j: i32) -> Point {
        let fi = f64::from(i);
        if !self.has_v {
            return Point {
                x: self.origin.x + fi * self.u.x,
                y: self.origin.y + fi * self.u.y,
            };
        }
        let fj = f64::from(j);
        Point {
            x: self.origin.x + fi * self.u.x + fj * self.v.x,
            y: self.origin.y + fi * self.u.y + fj * self.v.y,
        }
    }

    pub fn at_cell(&self, c: Cell) -> Point {
        self.at(c.i, c.j)
    }

    /// 連續的格子座標 (a, b)；基底退化時回 `None`。
    pub fn coordinates(&self, p: Point) -> Option<Point> {
        let (o, u, v) = (self.origin, self.u, self.v);
        let dx = p.x - o.x;
        let dy = p.y - o.y;
        if !self.has_v {
            let uu = u.x * u.x + u.y * u.y;
            if !(uu > 0.0) {
                return None;
            }
            return Some(Point {
                x: (dx * u.x + dy * u.y) / uu,
                y: 0.0,
            });
        }
        let det = u.x * v.y - u.y * v.x;
        if !(det.abs() > 1e-12 * (u.x * u.x + u.y * u.y + v.x * v.x + v.y * v.y)) {
            return None;
        }
        Some(Point {
            x: (dx * v.y - dy * v.x) / det,
            y: (u.x * dy - u.y * dx) / det,
        })
    }
}

/// `Rect`（影像像素；右、下邊不含）。`#[repr(C)]`：和 C 的 `AoiRsBounds` 相同。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Bounds {
    pub x: f64,
    pub y: f64,
    pub w: f64,
    pub h: f64,
}

impl Bounds {
    pub fn right(&self) -> f64 {
        self.x + self.w
    }

    pub fn bottom(&self) -> f64 {
        self.y + self.h
    }

    pub fn is_empty(&self) -> bool {
        !(self.w > 0.0 && self.h > 0.0)
    }

    pub fn contains(&self, p: Point) -> bool {
        p.x >= self.x && p.x < self.right() && p.y >= self.y && p.y < self.bottom()
    }
}
