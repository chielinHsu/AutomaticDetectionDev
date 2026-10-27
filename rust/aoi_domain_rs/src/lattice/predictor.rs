//! 由晶格預測格點（`lattice_predictor.h`）：哪些格子、在哪裡、什麼順序。只做幾何。

use crate::cpp::{by_less, cpp_max, cpp_min, msvc_i32};
use crate::fallible::{Res, TryPush, vec_filled, vec_with_capacity};
use crate::flat_map::{FlatMap, pack_key};
use crate::geometry::Point;
use crate::lattice::basis::{Basis, Bounds, Cell, MAX_INDEX};

/// `PredictedCell`。`#[repr(C)]`：和 C 的 `AoiRsPredictedCell` 相同。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct PredictedCell {
    pub cell: Cell,
    pub position: Point,
}

/// 預測需要的那部分模型：基底、索引範圍、內點的格子。
pub struct ModelGeometry<'a> {
    pub basis: Basis,
    pub min_i: i32,
    pub max_i: i32,
    pub min_j: i32,
    pub max_j: i32,
    pub cells: &'a [Cell],
}

/// 一次預測最多走訪這麼多個格子（防呆：不合理的模型不會讓呼叫端卡住）。
const MAX_VISITED: f64 = 16.0 * 1024.0 * 1024.0;

/// 索引範圍內（每邊往外多 margin 格）的格點。順序：依 i，再依 j。最多 max_count 個。
pub fn cells_in_range(m: &ModelGeometry, bounds: Bounds, margin: i32, max_count: usize) -> Res<Vec<PredictedCell>> {
    let mut out = Vec::new();
    let b = &m.basis;
    let i_from = m.min_i.wrapping_sub(margin);
    let i_to = m.max_i.wrapping_add(margin);
    let j_from = if b.has_v { m.min_j.wrapping_sub(margin) } else { 0 };
    let j_to = if b.has_v { m.max_j.wrapping_add(margin) } else { 0 };
    if i_to < i_from || j_to < j_from || max_count == 0 {
        return Ok(out);
    }
    if (f64::from(i_to) - f64::from(i_from) + 1.0) * (f64::from(j_to) - f64::from(j_from) + 1.0) > MAX_VISITED {
        return Ok(out);
    }
    for i in i_from..=i_to {
        for j in j_from..=j_to {
            let p = b.at(i, j);
            if !bounds.contains(p) {
                continue;
            }
            out.try_push(PredictedCell {
                cell: Cell { i, j },
                position: p,
            })?;
            if out.len() >= max_count {
                return Ok(out);
            }
        }
    }
    Ok(out)
}

/// 索引範圍內、沒有內點佔據的格點（缺格）。順序：依 i，再依 j。最多 max_count 個。
pub fn holes(m: &ModelGeometry, bounds: Bounds, max_count: usize) -> Res<Vec<PredictedCell>> {
    let mut out = Vec::new();
    let b = &m.basis;
    let j_from = if b.has_v { m.min_j } else { 0 };
    let j_to = if b.has_v { m.max_j } else { 0 };
    let w = f64::from(m.max_i) - f64::from(m.min_i) + 1.0;
    let h = f64::from(j_to) - f64::from(j_from) + 1.0;
    if w < 1.0 || h < 1.0 || max_count == 0 || w * h > MAX_VISITED {
        return Ok(out);
    }

    // 佔用表：索引範圍通常不大，用密集陣列；很大時用雜湊表。
    let dense = w * h <= 64.0 * m.cells.len() as f64 + 4096.0;
    let width = w as usize;
    let slot = |i: i32, j: i32| (j.wrapping_sub(j_from) as usize) * width + i.wrapping_sub(m.min_i) as usize;
    let mut grid: Vec<u8> = Vec::new();
    let mut sparse: FlatMap<u8> = FlatMap::new(if dense { 1 } else { m.cells.len() })?;
    if dense {
        grid = vec_filled((w * h) as usize, 0)?;
    }
    for c in m.cells {
        if c.i < m.min_i || c.i > m.max_i || c.j < j_from || c.j > j_to {
            continue;
        }
        if dense {
            grid[slot(c.i, c.j)] = 1;
        } else {
            *sparse.entry(pack_key(c.i, c.j))? = 1;
        }
    }
    for i in m.min_i..=m.max_i {
        for j in j_from..=j_to {
            let occupied = if dense {
                grid[slot(i, j)] != 0
            } else {
                sparse.find(pack_key(i, j)).is_some()
            };
            if occupied {
                continue;
            }
            let p = b.at(i, j);
            if !bounds.contains(p) {
                continue;
            }
            out.try_push(PredictedCell {
                cell: Cell { i, j },
                position: p,
            })?;
            if out.len() >= max_count {
                return Ok(out);
            }
        }
    }
    Ok(out)
}

/// bounds 內所有的格點，不限索引範圍。最多 max_count 個。
pub fn all_in_bounds(m: &ModelGeometry, bounds: Bounds, max_count: usize) -> Res<Vec<PredictedCell>> {
    let mut out = Vec::new();
    let b = &m.basis;
    if bounds.is_empty() || max_count == 0 {
        return Ok(out);
    }
    // bounds 的四個角換成格子座標，取外接的索引範圍，再逐一檢查中心是否落在 bounds 內。
    let corners = [
        Point { x: bounds.x, y: bounds.y },
        Point {
            x: bounds.right(),
            y: bounds.y,
        },
        Point {
            x: bounds.x,
            y: bounds.bottom(),
        },
        Point {
            x: bounds.right(),
            y: bounds.bottom(),
        },
    ];
    let (mut a_min, mut a_max, mut b_min, mut b_max) = (0.0, 0.0, 0.0, 0.0);
    for (k, &corner) in corners.iter().enumerate() {
        let Some(c) = b.coordinates(corner) else {
            return Ok(out);
        };
        a_min = if k == 0 { c.x } else { cpp_min(a_min, c.x) };
        a_max = if k == 0 { c.x } else { cpp_max(a_max, c.x) };
        b_min = if k == 0 { c.y } else { cpp_min(b_min, c.y) };
        b_max = if k == 0 { c.y } else { cpp_max(b_max, c.y) };
    }
    let i_from = a_min.floor();
    let i_to = a_max.ceil();
    let j_from = if b.has_v { b_min.floor() } else { 0.0 };
    let j_to = if b.has_v { b_max.ceil() } else { 0.0 };
    if (i_to - i_from + 1.0) * (j_to - j_from + 1.0) > MAX_VISITED {
        return Ok(out);
    }
    let limit = MAX_INDEX;
    if !(i_from.abs() < limit && i_to.abs() < limit && j_from.abs() < limit && j_to.abs() < limit) {
        return Ok(out);
    }
    for i in msvc_i32(i_from)..=msvc_i32(i_to) {
        for j in msvc_i32(j_from)..=msvc_i32(j_to) {
            let p = b.at(i, j);
            if !bounds.contains(p) {
                continue;
            }
            out.try_push(PredictedCell {
                cell: Cell { i, j },
                position: p,
            })?;
            if out.len() >= max_count {
                return Ok(out);
            }
        }
    }
    Ok(out)
}

/// 由近到遠：依與 anchor 的距離排序；同距離時依 i、再依 j。
pub fn order_near_to_far(cells: &mut [PredictedCell], anchor: Point) -> Res<()> {
    let mut d2: Vec<f64> = vec_with_capacity(cells.len())?;
    for c in cells.iter() {
        let dx = c.position.x - anchor.x;
        let dy = c.position.y - anchor.y;
        d2.push(dx * dx + dy * dy);
    }
    let mut order: Vec<usize> = vec_with_capacity(cells.len())?;
    order.extend(0..cells.len());
    order.sort_by(by_less(|&a: &usize, &b: &usize| {
        if d2[a] != d2[b] {
            return d2[a] < d2[b];
        }
        if cells[a].cell.i != cells[b].cell.i {
            return cells[a].cell.i < cells[b].cell.i;
        }
        cells[a].cell.j < cells[b].cell.j
    }));
    let mut sorted: Vec<PredictedCell> = vec_with_capacity(cells.len())?;
    for &k in &order {
        sorted.push(cells[k]);
    }
    cells.copy_from_slice(&sorted);
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn geometry(cells: &[Cell]) -> ModelGeometry<'_> {
        ModelGeometry {
            basis: Basis {
                origin: Point { x: 10.0, y: 10.0 },
                u: Point { x: 10.0, y: 0.0 },
                v: Point { x: 0.0, y: 10.0 },
                has_v: true,
            },
            min_i: 0,
            max_i: 2,
            min_j: 0,
            max_j: 1,
            cells,
        }
    }

    #[test]
    fn holes_are_the_unoccupied_cells_in_order() {
        let cells = [Cell { i: 0, j: 0 }, Cell { i: 1, j: 1 }, Cell { i: 2, j: 0 }, Cell { i: 2, j: 1 }];
        let big = Bounds {
            x: 0.0,
            y: 0.0,
            w: 1000.0,
            h: 1000.0,
        };
        let h = holes(&geometry(&cells), big, usize::MAX).unwrap();
        assert_eq!(h.iter().map(|c| (c.cell.i, c.cell.j)).collect::<Vec<_>>(), vec![(0, 1), (1, 0)]);
        assert_eq!(cells_in_range(&geometry(&cells), big, 1, usize::MAX).unwrap().len(), 5 * 4);
        let mut all = all_in_bounds(&geometry(&cells), big, 7).unwrap();
        assert_eq!(all.len(), 7);
        order_near_to_far(&mut all, Point { x: 0.0, y: 30.0 }).unwrap();
        assert_eq!(all[0].position, Point { x: 0.0, y: 30.0 });
    }
}
