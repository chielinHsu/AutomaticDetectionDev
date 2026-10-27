//! 晶格估計的接受規則（`lattice_acceptance.h`）：一組擬合出來的晶格「算不算數」。

use crate::cpp::by_less;
use crate::fallible::{Res, TryPush, vec_filled, vec_with_capacity};
use crate::geometry::Point;
use crate::lattice::basis::Basis;
use crate::lattice::detail::{Assigner, Candidate, CellTable, cross, fast_round, length, minus};
use crate::lattice::model::Model;
use crate::lattice::options::Options;

/// 二維要成立：不是全部在同一個 i、主列以外有夠多的成員、而且那些成員大多不在「可疑的列」。
pub fn two_dimensional_is_valid(c: &Candidate, points: &[Point], o: &Options) -> Res<bool> {
    if !c.basis.has_v || c.cells.is_empty() {
        return Ok(false);
    }
    let mut js: Vec<i32> = vec_with_capacity(c.cells.len())?;
    let front = c.cells[0];
    let mut single_column = true;
    let (mut min_i, mut max_i, mut min_j, mut max_j) = (front.i, front.i, front.j, front.j);
    for cell in &c.cells {
        js.push(cell.j);
        single_column = single_column && cell.i == front.i;
        min_i = min_i.min(cell.i);
        max_i = max_i.max(cell.i);
        min_j = min_j.min(cell.j);
        max_j = max_j.max(cell.j);
    }
    if single_column {
        return Ok(false);
    }
    js.sort_unstable();
    let mut longest = 0usize;
    let mut main_row = js[0];
    let mut a = 0usize;
    while a < js.len() {
        let mut b = a;
        while b < js.len() && js[b] == js[a] {
            b += 1;
        }
        if b - a > longest {
            longest = b - a;
            main_row = js[a];
        }
        a = b;
    }
    if c.members.len().wrapping_sub(longest) < o.min_second_row_inliers {
        return Ok(false);
    }

    let tol2 = c.tolerance * c.tolerance;
    let row_spacing = cross(c.basis.u, c.basis.v).abs() / length(c.basis.u);
    let band = c.tolerance / row_spacing; // 列座標離整數不到這個值：在列線上
    let gap_tolerance = c.tolerance / length(c.basis.u); // 間隔離整數格不到這個值：是整數格
    let assign = Assigner::new(&c.basis);
    let occupied = CellTable::of(&c.cells)?; // 同一格的重複點也算「落在格點上」
    let rows = max_j.wrapping_sub(min_j).wrapping_add(1) as usize;
    let mut in_row: Vec<usize> = vec_filled(rows, 0)?;
    let mut on_node: Vec<usize> = vec_filled(rows, 0)?;
    let mut on_line: Vec<(usize, f64)> = Vec::new(); // (列, 沿 u 的格子座標)
    for &p in points {
        let ab = assign.coordinates(p);
        let j = fast_round(ab.y);
        if j < min_j || j > max_j {
            continue;
        }
        if ab.x < f64::from(min_i) - 0.5 || ab.x > f64::from(max_i) + 0.5 {
            continue;
        }
        let a = assign.assign(p);
        let row = j.wrapping_sub(min_j) as usize;
        in_row[row] += 1;
        if a.residual2 <= tol2 && occupied.get(a.cell.i, a.cell.j) >= 0 {
            on_node[row] += 1;
        }
        if (ab.y - f64::from(j)).abs() <= band {
            on_line.try_push((row, ab.x))?;
        }
    }
    // std::pair<size_t, double> 的 operator<。
    on_line.sort_by(by_less(
        |a: &(usize, f64), b: &(usize, f64)| {
            if a.0 != b.0 { a.0 < b.0 } else { a.1 < b.1 }
        },
    ));
    let mut gaps: Vec<usize> = vec_filled(rows, 0)?;
    let mut whole_gaps: Vec<usize> = vec_filled(rows, 0)?;
    for k in 1..on_line.len() {
        if on_line[k].0 != on_line[k - 1].0 {
            continue;
        }
        let g = on_line[k].1 - on_line[k - 1].1;
        let cells = fast_round(g);
        if cells == 0 {
            continue; // 同一格的重複點：不算間隔
        }
        gaps[on_line[k].0] += 1;
        if (g - f64::from(cells)).abs() <= gap_tolerance {
            whole_gaps[on_line[k].0] += 1;
        }
    }
    let mut secondary = 0usize; // 主列以外落在格點上的點
    let mut suspect = 0usize; // 其中在「可疑的列」裡的
    for row in 0..rows {
        if (row as i32).wrapping_add(min_j) == main_row {
            continue;
        }
        secondary += on_node[row];
        if in_row[row] < 3 {
            continue;
        }
        let few_on_nodes = (on_node[row] as f64) < o.min_row_consistency * in_row[row] as f64;
        let own_spacing = gaps[row] >= 2 && 2 * whole_gaps[row] < gaps[row];
        if few_on_nodes || own_spacing {
            suspect += on_node[row];
        }
    }
    Ok(secondary > 0 && 2 * suspect < secondary)
}

/// 證據夠不夠：至少 min_inliers 個內點；或者每個點都被這組晶格解釋了。
pub fn enough_evidence(c: &Candidate, points: &[Point], o: &Options) -> Res<bool> {
    if c.members.len() < 2 {
        return Ok(false);
    }
    if c.members.len() >= o.min_inliers {
        return Ok(true);
    }
    let tol2 = c.tolerance * c.tolerance;
    let assign = Assigner::new(&c.basis);
    let occupied = CellTable::of(&c.cells)?;
    let mut explained = 0usize;
    for &p in points {
        let a = assign.assign(p);
        if a.residual2 <= tol2 && occupied.get(a.cell.i, a.cell.j) >= 0 {
            explained += 1;
        }
    }
    Ok(explained == points.len())
}

/// 補缺格要有夠多的證據：缺格數 ＜ 內點數 − 3（一維）或 − 4（二維）；沒有缺格的晶格不受限。
pub fn holes_are_backed(m: &Model) -> bool {
    holes_are_backed_of(m.inliers.len(), m.cell_count(), m.basis.has_v)
}

/// `holesAreBacked` 的本體（FFI 只有內點數，不必複製內點）。
pub fn holes_are_backed_of(inlier_count: usize, cell_count: usize, two_dimensional: bool) -> bool {
    let inliers = inlier_count as i64;
    let holes = (cell_count as i64).wrapping_sub(inliers);
    holes == 0 || holes < inliers - if two_dimensional { 4 } else { 3 }
}

/// 一致性：陣列範圍裡的點，有多少落在有內點的格點上。
pub fn consistency(m: &Model, points: &[Point]) -> Res<f64> {
    consistency_of(&m.basis, m.tolerance_px, m.min_i, m.max_i, m.min_j, m.max_j, &m.cells, points)
}

/// `consistency` 的本體（FFI 從 C 的結構直接呼叫，不必組一個 `Model`）。
#[allow(clippy::too_many_arguments)]
pub fn consistency_of(
    b: &Basis,
    tolerance_px: f64,
    min_i: i32,
    max_i: i32,
    min_j: i32,
    max_j: i32,
    cells: &[crate::lattice::basis::Cell],
    points: &[Point],
) -> Res<f64> {
    let assign = Assigner::new(b);
    let occupied = CellTable::of(cells)?;
    let tol2 = tolerance_px * tolerance_px;
    let lu = length(b.u);
    let mut inside = 0usize;
    let mut on_node = 0usize;
    for &p in points {
        let ab = assign.coordinates(p);
        if ab.x < f64::from(min_i) - 0.5 || ab.x > f64::from(max_i) + 0.5 {
            continue;
        }
        if b.has_v {
            if ab.y < f64::from(min_j) - 0.5 || ab.y > f64::from(max_j) + 0.5 {
                continue;
            }
        } else if cross(b.u, minus(p, b.origin)).abs() / lu > 0.5 * lu {
            continue; // 離這一排的線超過半個格距
        }
        inside += 1;
        let a = assign.assign(p);
        if a.residual2 <= tol2 && occupied.get(a.cell.i, a.cell.j) >= 0 {
            on_node += 1;
        }
    }
    Ok(if inside == 0 { 0.0 } else { on_node as f64 / inside as f64 })
}
