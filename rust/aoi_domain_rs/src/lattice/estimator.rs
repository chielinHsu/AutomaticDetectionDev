//! 晶格估計（`lattice_estimator.h`）：找基底 → 原點（RANSAC）→ 精修 → 連通 → 接受規則 → 定形式與品質。
//! 流程與每一步的理由寫在 C++ 的標頭與 `docs/steps/S05-lattice/`；這裡照 C++ 的運算順序寫，結果一位不差。

use crate::cpp::{by_less, cpp_max, cpp_min, msvc_i32};
use crate::fallible::{Res, TryPush, vec_with_capacity};
use crate::geometry::Point;
use crate::lattice::acceptance::{consistency, enough_evidence, holes_are_backed, two_dimensional_is_valid};
use crate::lattice::accumulator::Accumulator;
use crate::lattice::basis::{Basis, Cell};
use crate::lattice::basis_finder::BasisFinder;
use crate::lattice::cell_components::largest_component;
use crate::lattice::clustering::Clustering;
use crate::lattice::detail::{
    Assigned, Assigner, Candidate, CellTable, SplitMix64, cross, dot, finite_point, length, merge_solution, minus, reach, squared_distance,
    times, tolerance_for,
};
use crate::lattice::model::{Fit, Model, Reason, assess_confidence};
use crate::lattice::options::Options;

struct Outcome {
    model: Option<Model>,
    reason: Reason,
    rejected_confidence: f64,
}

/// 對一組點找「一組」晶格（主晶格）。
struct Fitter<'a> {
    o: &'a Options,
    clustering: Clustering<'a>,
    pts: &'a [Point],
    rng: SplitMix64,
    hits: Vec<(usize, Assigned)>,
}

impl<'a> Fitter<'a> {
    fn new(o: &'a Options, clustering: Clustering<'a>, pts: &'a [Point]) -> Self {
        Fitter {
            o,
            clustering,
            pts,
            rng: SplitMix64::new(o.seed),
            hits: Vec::new(),
        }
    }

    fn run(&mut self) -> Res<Outcome> {
        let mut out = Outcome {
            model: None,
            reason: Reason::None,
            rejected_confidence: 0.0,
        };
        let pts = self.pts;
        if pts.len() < 2 {
            out.reason = Reason::TooFewPoints;
            return Ok(out);
        }
        let (mut min_x, mut max_x, mut min_y, mut max_y) = (pts[0].x, pts[0].x, pts[0].y, pts[0].y);
        for p in pts {
            min_x = cpp_min(min_x, p.x);
            max_x = cpp_max(max_x, p.x);
            min_y = cpp_min(min_y, p.y);
            max_y = cpp_max(max_y, p.y);
        }
        let extent = cpp_max(max_x - min_x, max_y - min_y);
        if !(extent > 0.0) {
            out.reason = Reason::NoDirection; // 全部重疊
            return Ok(out);
        }
        let finder = BasisFinder::new(self.o, self.clustering, pts, 1e-9 * extent);
        let directions = finder.directions()?;
        if directions.is_empty() {
            out.reason = Reason::NoDirection;
            return Ok(out);
        }
        // u 的候選：票數夠多的方向裡最短的 3 個；同一個 u 先試二維、再試一維。
        out.reason = Reason::NoConsistentLattice;
        let tries = directions.len().min(3);
        for &u in &directions[..tries] {
            if let Some(v) = finder.second_direction(u, &directions)? {
                let plane = self.fit_hypothesis(Basis {
                    origin: Point::default(),
                    u,
                    v,
                    has_v: true,
                })?;
                if let Some(plane) = plane
                    && two_dimensional_is_valid(&plane, pts, self.o)?
                    && self.accept(&plane, &mut out)?
                {
                    return Ok(out);
                }
            }
            let line = self.fit_hypothesis(Basis {
                origin: Point::default(),
                u,
                v: Point::default(),
                has_v: false,
            })?;
            if let Some(line) = line
                && self.accept(&line, &mut out)?
            {
                return Ok(out);
            }
        }
        Ok(out)
    }

    /// 證據、缺格、信心三條規則都過：放進 out 回傳 true。沒過：在 out 記下最好的失敗原因。
    fn accept(&self, candidate: &Candidate, out: &mut Outcome) -> Res<bool> {
        if !enough_evidence(candidate, self.pts, self.o)? {
            return Ok(false);
        }
        let model = self.finalize(candidate)?;
        if !holes_are_backed(&model)? {
            if out.reason != Reason::LowConfidence {
                out.reason = Reason::TooManyHoles;
            }
            return Ok(false);
        }
        if model.confidence < self.o.min_confidence {
            if out.reason != Reason::LowConfidence || model.confidence > out.rejected_confidence {
                out.reason = Reason::LowConfidence;
                out.rejected_confidence = model.confidence;
            }
            return Ok(false);
        }
        out.model = Some(model);
        out.reason = Reason::None;
        out.rejected_confidence = 0.0;
        Ok(true)
    }

    /// RANSAC：隨機挑點當原點（第一次固定用最靠近中位數的點），數它附近 ransac_index_radius 格內有幾個點落在格點上。
    fn pick_origin(&mut self, mut basis: Basis, tol: f64) -> Res<usize> {
        let pts = self.pts;
        let n = pts.len();
        let mut xs: Vec<f64> = vec_with_capacity(n)?;
        let mut ys: Vec<f64> = vec_with_capacity(n)?;
        for p in pts {
            xs.push(p.x);
            ys.push(p.y);
        }
        let mid = n / 2;
        // nth_element：點都是有限值；total_cmp 只把 −0 排在 +0 前面，選出來的值和 C++ 只差在零的正負號，距離相同。
        xs.select_nth_unstable_by(mid, f64::total_cmp);
        ys.select_nth_unstable_by(mid, f64::total_cmp);
        let median = Point { x: xs[mid], y: ys[mid] };
        let mut central = 0usize;
        let mut central_distance = squared_distance(pts[0], median);
        for (k, &p) in pts.iter().enumerate().skip(1) {
            let d = squared_distance(p, median);
            if d < central_distance {
                central_distance = d;
                central = k;
            }
        }

        let tol2 = tol * tol;
        let trials = 1.max(self.o.ransac_trials.min(n.min(1 << 20) as i32));
        let mut best_index = central;
        let mut best_score = 0usize;
        let mut best_error = f64::INFINITY;
        for trial in 0..trials {
            let candidate = if trial == 0 { central } else { self.rng.below(n) };
            basis.origin = pts[candidate];
            let assign = Assigner::new(&basis);
            let mut score = 0usize;
            let mut nearby = 0usize;
            let mut error = 0.0;
            for &p in pts {
                let a = assign.assign(p);
                if reach(a.cell) > self.o.ransac_index_radius {
                    continue;
                }
                nearby += 1;
                if a.residual2 <= tol2 {
                    score += 1;
                    error += a.residual2;
                }
            }
            if trial == 0 || score > best_score || (score == best_score && error < best_error) {
                best_index = candidate;
                best_score = score;
                best_error = error;
            }
            if score == nearby && score >= 2 {
                break; // 附近每個點都在格點上：這個原點夠好了
            }
        }
        Ok(best_index)
    }

    /// 由原點附近開始精修，每次把範圍（格數）加倍：指派 → 最小平方法。範圍蓋住所有點時停止。
    fn grow(&self, mut b: Basis, tol: f64) -> Basis {
        let tol2 = tol * tol;
        let mut limit = 2.max(self.o.ransac_index_radius);
        loop {
            let assign = Assigner::new(&b);
            let mut acc = Accumulator::new(b.origin, Cell { i: 0, j: 0 });
            let mut farthest = 0;
            for &p in self.pts {
                let a = assign.assign(p);
                let r = reach(a.cell);
                farthest = farthest.max(r);
                if r > limit || !(a.residual2 <= tol2) {
                    continue;
                }
                acc.add(a.cell, p);
            }
            if let Some(solved) = acc.solve() {
                b = merge_solution(&b, &solved);
            }
            if limit >= farthest {
                break;
            }
            limit = if limit >= (1 << 26) { farthest } else { limit * 2 };
        }
        b
    }

    /// 每個點指派到最近的格子；殘差 ≤ 容差才算成員；同一格有兩個點時留殘差小的。
    /// allowed 有值時，只接受在這個索引範圍內的格子（[minI, maxI, minJ, maxJ]）。
    fn assign_all(&mut self, b: Basis, tol: f64, c: &mut Candidate, allowed: Option<[i32; 4]>) -> Res<()> {
        c.basis = b;
        c.tolerance = tol;
        c.members.clear();
        c.cells.clear();
        let tol2 = tol * tol;
        let assign = Assigner::new(&b);
        let pts = self.pts;
        self.hits.clear();
        let (mut min_i, mut max_i, mut min_j, mut max_j) = (0, 0, 0, 0);
        for (k, &p) in pts.iter().enumerate() {
            let a = assign.assign(p);
            if !(a.residual2 <= tol2) {
                continue;
            }
            if let Some(al) = allowed
                && (a.cell.i < al[0] || a.cell.i > al[1] || a.cell.j < al[2] || a.cell.j > al[3])
            {
                continue;
            }
            if self.hits.is_empty() {
                min_i = a.cell.i;
                max_i = a.cell.i;
                min_j = a.cell.j;
                max_j = a.cell.j;
            }
            min_i = min_i.min(a.cell.i);
            max_i = max_i.max(a.cell.i);
            min_j = min_j.min(a.cell.j);
            max_j = max_j.max(a.cell.j);
            self.hits.try_push((k, a))?;
        }
        if self.hits.is_empty() {
            return Ok(());
        }
        let mut best_hit = CellTable::new(min_i, max_i, min_j, max_j, self.hits.len())?;
        for (h, hit) in self.hits.iter().enumerate() {
            let cell = hit.1.cell;
            let current = best_hit.get(cell.i, cell.j);
            if current < 0 || hit.1.residual2 < self.hits[current as usize].1.residual2 {
                best_hit.set(cell.i, cell.j, h as i32)?;
            }
        }
        for (h, hit) in self.hits.iter().enumerate() {
            let cell = hit.1.cell;
            if best_hit.get(cell.i, cell.j) != h as i32 {
                continue;
            }
            c.members.try_push(hit.0)?;
            c.cells.try_push(cell)?;
        }
        Ok(())
    }

    fn refit(&self, c: &Candidate) -> Option<Basis> {
        let mut acc = Accumulator::new(c.basis.origin, Cell { i: 0, j: 0 });
        for (m, &member) in c.members.iter().enumerate() {
            acc.add(c.cells[m], self.pts[member]);
        }
        let solved = acc.solve()?;
        Some(merge_solution(&c.basis, &solved))
    }

    /// 連通：只留成員最多的那一塊。回傳有沒有拿掉成員。
    fn keep_largest_component(&self, c: &mut Candidate) -> Res<bool> {
        let keep = largest_component(&c.cells, self.o.max_gap_cells.wrapping_add(1), c.basis.has_v)?;
        if keep.len() == c.members.len() {
            return Ok(false);
        }
        let mut members: Vec<usize> = vec_with_capacity(keep.len())?;
        let mut cells: Vec<Cell> = vec_with_capacity(keep.len())?;
        for &k in &keep {
            members.push(c.members[k]);
            cells.push(c.cells[k]);
        }
        c.members = members;
        c.cells = cells;
        Ok(true)
    }

    fn fit_hypothesis(&mut self, mut basis: Basis) -> Res<Option<Candidate>> {
        let inlier_fraction = self.o.inlier_fraction;
        let mut tol = tolerance_for(&basis, inlier_fraction);
        if !(tol > 0.0) {
            return Ok(None);
        }
        basis.origin = self.pts[self.pick_origin(basis, tol)?];
        basis = self.grow(basis, tol);

        // grow 的最後一次已經用全部的點精修過；再指派一次（同一格只留一個點）、精修。
        let mut c = Candidate::default();
        tol = tolerance_for(&basis, inlier_fraction);
        self.assign_all(basis, tol, &mut c, None)?;
        if c.members.len() < 2 {
            return Ok(None);
        }
        if let Some(solved) = self.refit(&c) {
            basis = solved;
        }
        tol = tolerance_for(&basis, inlier_fraction);
        self.assign_all(basis, tol, &mut c, None)?;
        if c.members.len() < 2 {
            return Ok(None);
        }
        if self.keep_largest_component(&mut c)? {
            if let Some(solved) = self.refit(&c) {
                c.basis = solved;
            }
            // 重新指派，但只收這一塊的索引範圍內的點。
            let mut bx = [c.cells[0].i, c.cells[0].i, c.cells[0].j, c.cells[0].j];
            for cell in &c.cells {
                bx[0] = bx[0].min(cell.i);
                bx[1] = bx[1].max(cell.i);
                bx[2] = bx[2].min(cell.j);
                bx[3] = bx[3].max(cell.j);
            }
            let b = c.basis;
            self.assign_all(b, tolerance_for(&b, inlier_fraction), &mut c, Some(bx))?;
            if c.members.len() < 2 {
                return Ok(None);
            }
            if let Some(solved) = self.refit(&c) {
                c.basis = solved;
            }
        }
        Ok(Some(c))
    }

    /// 定形式（約化、方向、索引從 0 起）、最後一次最小平方法、品質與信心。
    fn finalize(&self, c: &Candidate) -> Res<Model> {
        let mut b = c.basis;
        let mut order: Vec<usize> = vec_with_capacity(c.members.len())?;
        order.extend(0..c.members.len());
        order.sort_by(by_less(|&a: &usize, &bb: &usize| c.members[a] < c.members[bb]));
        let mut members: Vec<usize> = vec_with_capacity(order.len())?;
        let mut cells: Vec<Cell> = vec_with_capacity(order.len())?;
        for &k in &order {
            members.push(c.members[k]);
            cells.push(c.cells[k]);
        }
        // Lagrange–Gauss 約化：|u| ≤ |v|，v 在 u 方向的分量不超過 |u| 的一半；索引跟著換。
        if b.has_v {
            for _guard in 0..64 {
                if length(b.v) < length(b.u) {
                    std::mem::swap(&mut b.u, &mut b.v);
                    for cell in cells.iter_mut() {
                        std::mem::swap(&mut cell.i, &mut cell.j);
                    }
                }
                let k = (dot(b.u, b.v) / dot(b.u, b.u)).round();
                if k == 0.0 {
                    break;
                }
                b.v = minus(b.v, times(b.u, k));
                let step = msvc_i32(k);
                for cell in cells.iter_mut() {
                    cell.i = cell.i.wrapping_add(step.wrapping_mul(cell.j));
                }
            }
        }
        // 方向：u 在右半平面、cross(u, v) > 0。
        if b.u.x < 0.0 || (b.u.x == 0.0 && b.u.y < 0.0) {
            b.u = times(b.u, -1.0);
            for cell in cells.iter_mut() {
                cell.i = cell.i.wrapping_neg();
            }
        }
        if b.has_v && cross(b.u, b.v) < 0.0 {
            b.v = times(b.v, -1.0);
            for cell in cells.iter_mut() {
                cell.j = cell.j.wrapping_neg();
            }
        }
        if !b.has_v {
            b.v = Point { x: 0.0, y: 0.0 };
            for cell in cells.iter_mut() {
                cell.j = 0;
            }
        }
        let (mut min_i, mut min_j) = (cells[0].i, cells[0].j);
        for cell in &cells {
            min_i = min_i.min(cell.i);
            min_j = min_j.min(cell.j);
        }
        b.origin = b.at(min_i, min_j);
        for cell in cells.iter_mut() {
            cell.i = cell.i.wrapping_sub(min_i);
            cell.j = cell.j.wrapping_sub(min_j);
        }

        let mut model = Model {
            equations: Accumulator::new(b.origin, Cell { i: 0, j: 0 }),
            ..Model::default()
        };
        for (k, &member) in members.iter().enumerate() {
            model.equations.add(cells[k], self.pts[member]);
        }
        if let Some(solved) = model.equations.solve() {
            b = merge_solution(&b, &solved);
        }
        model.basis = b;

        let mut sum_squares = 0.0;
        for (k, &member) in members.iter().enumerate() {
            sum_squares += squared_distance(self.pts[member], b.at_cell(cells[k]));
        }
        model.rms_px = (sum_squares / members.len() as f64).sqrt();
        model.tolerance_px = tolerance_for(&b, self.o.inlier_fraction);
        for cell in &cells {
            model.max_i = model.max_i.max(cell.i);
            model.max_j = model.max_j.max(cell.j);
        }
        model.inliers = members;
        model.cells = cells;
        model.coverage = model.inliers.len() as f64 / model.cell_count()? as f64;
        model.consistency = consistency(&model, self.pts)?;
        model.confidence = assess_confidence(
            model.inliers.len(),
            model.rms_px,
            model.tolerance_px,
            model.coverage,
            model.consistency,
            b.has_v,
        );
        model.suggest_check = model.confidence < self.o.check_below;
        Ok(model)
    }
}

/// `LatticeEstimator::run`：主晶格與其他晶格（最多 max_models 組）；沒有晶格時 models 是空的，reason 說明原因。
pub fn fit_lattices(o: &Options, clustering: Clustering<'_>, points: &[Point], max_models: usize) -> Res<Fit> {
    let mut result = Fit::default();
    let mut active: Vec<usize> = vec_with_capacity(points.len())?;
    for (k, &p) in points.iter().enumerate() {
        if finite_point(p) {
            active.push(k);
        }
    }
    let mut local: Vec<Point> = Vec::new();
    while result.models.len() < max_models {
        let need = if result.models.is_empty() {
            2
        } else {
            2.max(o.min_extra_lattice_inliers)
        };
        if active.len() < need {
            if result.models.is_empty() {
                result.reason = Reason::TooFewPoints;
            }
            break;
        }
        local.clear();
        local.try_reserve(active.len())?;
        local.extend(active.iter().map(|&k| points[k]));
        let outcome = Fitter::new(o, clustering, &local).run()?;
        let Some(mut model) = outcome.model else {
            if result.models.is_empty() {
                result.reason = outcome.reason;
                result.rejected_confidence = outcome.rejected_confidence;
            }
            break;
        };
        if !result.models.is_empty() && model.inliers.len() < o.min_extra_lattice_inliers {
            break;
        }
        for index in model.inliers.iter_mut() {
            *index = active[*index]; // 換回輸入的索引（仍由小到大）
        }

        // 這組晶格「認領」的點：在它的索引範圍內、離格點不到 2 倍容差。認領的點不再參與下一組。
        let assign = Assigner::new(&model.basis);
        let claim2 = 4.0 * model.tolerance_px * model.tolerance_px;
        let mut rest: Vec<usize> = vec_with_capacity(active.len())?;
        for &k in &active {
            let a = assign.assign(points[k]);
            let in_box = a.cell.i >= model.min_i && a.cell.i <= model.max_i && a.cell.j >= model.min_j && a.cell.j <= model.max_j;
            if in_box && a.residual2 <= claim2 {
                continue;
            }
            rest.push(k);
        }
        active = rest;
        result.models.try_push(model)?;
    }
    if !result.models.is_empty() {
        result.reason = Reason::None;
    }
    // 主晶格放第一個：內點最多的（同數時維持找到的順序）。
    result
        .models
        .sort_by(by_less(|a: &Model, b: &Model| a.inliers.len() > b.inliers.len()));
    Ok(result)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn grid(cols: i32, rows: i32, px: f64, py: f64) -> Vec<Point> {
        let mut v = Vec::new();
        for c in 0..cols {
            for r in 0..rows {
                v.push(Point {
                    x: 100.0 + f64::from(c) * px,
                    y: 100.0 + f64::from(r) * py,
                });
            }
        }
        v
    }

    #[test]
    fn a_complete_grid_has_no_holes_and_the_right_pitch() {
        let pts = grid(5, 4, 30.0, 20.0);
        let fit = fit_lattices(&Options::default(), Clustering::GridVote { bin_fraction: 0.1 }, &pts, 1).unwrap();
        assert_eq!(fit.models.len(), 1);
        let m = &fit.models[0];
        assert!(m.basis.has_v);
        assert_eq!(m.inliers.len(), 20);
        assert!(m.rms_px < 1e-6);
        let mut pitches = [length(m.basis.u), length(m.basis.v)];
        pitches.sort_by(f64::total_cmp);
        assert!((pitches[0] - 20.0).abs() < 1e-6 && (pitches[1] - 30.0).abs() < 1e-6);
    }

    #[test]
    fn too_few_points_and_overlapping_points() {
        let o = Options::default();
        let c = Clustering::GridVote { bin_fraction: 0.1 };
        assert_eq!(fit_lattices(&o, c, &[Point::default()], 4).unwrap().reason, Reason::TooFewPoints);
        let same = [Point { x: 1.0, y: 1.0 }; 3];
        assert_eq!(fit_lattices(&o, c, &same, 4).unwrap().reason, Reason::NoDirection);
    }

    #[test]
    fn a_single_row_is_one_dimensional() {
        let pts = grid(8, 1, 25.0, 0.0);
        let fit = fit_lattices(&Options::default(), Clustering::Greedy { tolerance: 0.2 }, &pts, 4).unwrap();
        assert_eq!(fit.models.len(), 1);
        assert!(!fit.models[0].basis.has_v);
        assert_eq!(fit.models[0].max_i, 7);
    }
}
