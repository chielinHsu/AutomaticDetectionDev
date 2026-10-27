//! 均勻格子的空間索引（`spatial_grid.h`）：找最近的 k 個鄰居。不存點：每次查詢把建立時的點傳進來
//! （C++ 的 `SpatialGrid` 持有 `span`；FFI 的 handle 自己保存一份點）。

use crate::cpp::{cpp_clamp, cpp_max, cpp_min, msvc_i32};
use crate::fallible::{Res, TryPush, vec_filled, vec_from_slice};
use crate::geometry::Point;

/// `SpatialGrid::Neighbor`。`#[repr(C)]`：和 C 的 `AoiRsNeighbor` 相同。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Neighbor {
    pub index: usize,
    pub distance: f64,
}

pub struct SpatialGrid {
    count: usize,
    cell: f64,
    x0: f64,
    y0: f64,
    nx: i32,
    ny: i32,
    /// 每格在 items 的起點（長度 nx·ny ＋ 1）
    start: Vec<u32>,
    /// 依格子排好的點索引
    items: Vec<u32>,
}

impl SpatialGrid {
    /// `cell_size` ≤ 0 或不是有限值時，自己依點的範圍與點數決定（平均每格約一點）。
    pub fn new(points: &[Point], cell_size: f64) -> Res<Self> {
        let n = points.len();
        let mut min_x = f64::MAX;
        let mut min_y = f64::MAX;
        let mut max_x = f64::MIN;
        let mut max_y = f64::MIN;
        for p in points {
            min_x = cpp_min(min_x, p.x);
            min_y = cpp_min(min_y, p.y);
            max_x = cpp_max(max_x, p.x);
            max_y = cpp_max(max_y, p.y);
        }
        if n == 0 {
            (min_x, min_y, max_x, max_y) = (0.0, 0.0, 0.0, 0.0);
        }
        let w = max_x - min_x;
        let h = max_y - min_y;

        let mut cell = cell_size;
        if !(cell > 0.0) || !cell.is_finite() {
            let count = n.max(1) as f64;
            cell = cpp_max((w * h / count).sqrt(), cpp_max(w, h) / count);
            if !(cell > 0.0) || !cell.is_finite() {
                cell = 1.0;
            }
        }
        // 格子數上限約 4n：點的範圍很大、格子又很小時，放大格子。
        let max_cells = 4.0 * n as f64 + 16.0;
        let (nx, ny);
        loop {
            let cx = (w / cell).floor() + 1.0;
            let cy = (h / cell).floor() + 1.0;
            if cx * cy <= max_cells {
                nx = msvc_i32(cx);
                ny = msvc_i32(cy);
                break;
            }
            cell *= cpp_max(1.01, (cx * cy / max_cells).sqrt());
        }
        let mut grid = SpatialGrid {
            count: n,
            cell,
            x0: min_x,
            y0: min_y,
            nx,
            ny,
            start: Vec::new(),
            items: Vec::new(),
        };

        // 計數排序：每格的點連續放在 items 裡。
        let cells = (nx as usize) * (ny as usize);
        grid.start = vec_filled(cells + 1, 0u32)?;
        let mut cell_of: Vec<u32> = vec_filled(n, 0)?;
        for (k, p) in points.iter().enumerate() {
            let c = (grid.cell_y(p.y) as usize) * (nx as usize) + grid.cell_x(p.x) as usize;
            cell_of[k] = c as u32;
            grid.start[c + 1] = grid.start[c + 1].wrapping_add(1);
        }
        for c in 0..cells {
            grid.start[c + 1] = grid.start[c + 1].wrapping_add(grid.start[c]);
        }
        grid.items = vec_filled(n, 0u32)?;
        let mut fill = vec_from_slice(&grid.start[..cells])?;
        for k in 0..n {
            let slot = &mut fill[cell_of[k] as usize];
            grid.items[*slot as usize] = k as u32;
            *slot = slot.wrapping_add(1);
        }
        Ok(grid)
    }

    pub fn cell_size(&self) -> f64 {
        self.cell
    }

    pub fn len(&self) -> usize {
        self.count
    }

    pub fn is_empty(&self) -> bool {
        self.count == 0
    }

    fn cell_x(&self, x: f64) -> i32 {
        let c = ((x - self.x0) / self.cell).floor();
        msvc_i32(cpp_clamp(c, 0.0, f64::from(self.nx - 1)))
    }

    fn cell_y(&self, y: f64) -> i32 {
        let c = ((y - self.y0) / self.cell).floor();
        msvc_i32(cpp_clamp(c, 0.0, f64::from(self.ny - 1)))
    }

    /// points[self_index] 最近的 k 個鄰居（距離 > min_distance），由近到遠寫進 out（先清空）。
    /// `points` 必須是建立時的那一組點。
    pub fn nearest(&self, points: &[Point], self_index: usize, k: usize, min_distance: f64, out: &mut Vec<Neighbor>) -> Res<()> {
        out.clear();
        if k == 0 || self_index >= points.len() {
            return Ok(());
        }
        let p = points[self_index];
        let cx = self.cell_x(p.x);
        let cy = self.cell_y(p.y);
        let min2 = min_distance * min_distance;

        // 比較的是距離的平方（不開根號）；找完才換回距離。
        let visit = |gx: i32, gy: i32, out: &mut Vec<Neighbor>| -> Res<()> {
            if gx < 0 || gy < 0 || gx >= self.nx || gy >= self.ny {
                return Ok(());
            }
            let c = (gy as usize) * (self.nx as usize) + gx as usize;
            for s in self.start[c]..self.start[c + 1] {
                let q = self.items[s as usize] as usize;
                if q == self_index {
                    continue;
                }
                let dx = points[q].x - p.x;
                let dy = points[q].y - p.y;
                let d2 = dx * dx + dy * dy;
                if !(d2 > min2) {
                    continue; // 重疊的點沒有方向
                }
                if out.len() == k && d2 >= out[out.len() - 1].distance {
                    continue;
                }
                // std::upper_bound：第一個 d2 < e.distance 的位置。
                let at = out.partition_point(|e| !(d2 < e.distance));
                out.try_insert(at, Neighbor { index: q, distance: d2 })?;
                if out.len() > k {
                    out.pop();
                }
            }
            Ok(())
        };

        // 一圈一圈往外找（Chebyshev 距離 r 的那一圈格子）。
        let max_ring = self.nx.max(self.ny);
        let mut r = 0i32;
        while r <= max_ring {
            if r == 0 {
                visit(cx, cy, out)?;
            } else {
                for dx in -r..=r {
                    visit(cx.wrapping_add(dx), cy.wrapping_sub(r), out)?;
                    visit(cx.wrapping_add(dx), cy.wrapping_add(r), out)?;
                }
                for dy in (-r + 1)..=(r - 1) {
                    visit(cx.wrapping_sub(r), cy.wrapping_add(dy), out)?;
                    visit(cx.wrapping_add(r), cy.wrapping_add(dy), out)?;
                }
            }
            let reach = f64::from(r) * self.cell;
            if out.len() == k && out[out.len() - 1].distance <= reach * reach {
                break;
            }
            r += 1;
        }
        for n in out.iter_mut() {
            n.distance = n.distance.sqrt();
        }
        Ok(())
    }
}

/// FFI 的 handle：自己保存一份點（C++ 的 `SpatialGrid` 只借用 span）。
pub struct OwnedSpatialGrid {
    pub points: Vec<Point>,
    pub grid: SpatialGrid,
}

impl OwnedSpatialGrid {
    pub fn new(points: &[Point], cell_size: f64) -> Res<Self> {
        let points = vec_from_slice(points)?;
        let grid = SpatialGrid::new(&points, cell_size)?;
        Ok(OwnedSpatialGrid { points, grid })
    }

    pub fn nearest(&self, self_index: usize, k: usize, min_distance: f64, out: &mut Vec<Neighbor>) -> Res<()> {
        self.grid.nearest(&self.points, self_index, k, min_distance, out)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn finds_the_nearest_neighbours_in_order() {
        let pts: Vec<Point> = (0..25)
            .map(|k| Point {
                x: f64::from(k % 5) * 10.0,
                y: f64::from(k / 5) * 7.0,
            })
            .collect();
        let grid = SpatialGrid::new(&pts, 0.0).unwrap();
        let mut out = Vec::new();
        grid.nearest(&pts, 12, 4, 1e-9, &mut out).unwrap();
        assert_eq!(out.len(), 4);
        assert_eq!(out[0].distance, 7.0);
        assert_eq!(out[1].distance, 7.0);
        assert_eq!(out[2].distance, 10.0);
        let mut brute: Vec<f64> = pts
            .iter()
            .filter(|p| **p != pts[12])
            .map(|p| crate::lattice::detail::squared_distance(*p, pts[12]).sqrt())
            .collect();
        brute.sort_by(f64::total_cmp);
        assert_eq!(out.iter().map(|n| n.distance).collect::<Vec<_>>(), brute[..4].to_vec());
    }
}
