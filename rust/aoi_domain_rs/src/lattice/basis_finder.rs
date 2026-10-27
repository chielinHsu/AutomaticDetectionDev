//! 晶格估計的第一步：從點找出基底向量的候選（`basis_finder.h`）。

use crate::cpp::{by_less, pair_less};
use crate::fallible::{Res, TryPush, vec_filled, vec_with_capacity};
use crate::geometry::Point;
use crate::lattice::clustering::Clustering;
use crate::lattice::detail::{cross, dot, fast_round, half_plane, length, minus, times};
use crate::lattice::options::Options;
use crate::lattice::spatial_grid::{Neighbor, SpatialGrid};

pub struct BasisFinder<'a> {
    o: &'a Options,
    clustering: Clustering<'a>,
    pts: &'a [Point],
    min_distance: f64,
}

/// 「列距更小，或列距相同而且更短」：`fromDirections` 與 `betweenRows` 共用的挑選規則。
fn better(best: Option<Point>, best_spacing: f64, candidate: Point, spacing: f64, lu: f64) -> bool {
    match best {
        None => true,
        Some(b) => spacing < best_spacing - 1e-9 * lu || ((spacing - best_spacing).abs() <= 1e-9 * lu && length(candidate) < length(b)),
    }
}

impl<'a> BasisFinder<'a> {
    pub fn new(o: &'a Options, clustering: Clustering<'a>, pts: &'a [Point], min_distance: f64) -> Self {
        BasisFinder {
            o,
            clustering,
            pts,
            min_distance,
        }
    }

    /// 方向的候選：每個點（抽樣）到最近幾個鄰居的位移向量分群後，票數夠多的群，由短到長。
    pub fn directions(&self) -> Res<Vec<Point>> {
        let n = self.pts.len();
        let grid = SpatialGrid::new(self.pts, 0.0)?;
        let samples = self.o.max_samples.max(1);
        let stride = 1usize.max(n.wrapping_add(samples).wrapping_sub(1) / samples);
        let k = self.o.neighbors.max(1);
        let mut vectors: Vec<Point> = vec_with_capacity((n / stride + 1).saturating_mul(k))?;
        let mut nearest_distances: Vec<f64> = Vec::new();
        let mut neighbours: Vec<Neighbor> = Vec::new();
        let mut i = 0usize;
        while i < n {
            grid.nearest(self.pts, i, k, self.min_distance, &mut neighbours)?;
            if !neighbours.is_empty() {
                nearest_distances.try_push(neighbours[0].distance)?;
                for nb in &neighbours {
                    vectors.try_push(half_plane(minus(self.pts[nb.index], self.pts[i])))?;
                }
            }
            i = i.wrapping_add(stride);
            if i < stride {
                break; // 溢位（C++ 的 size_t 迴圈在這種 n 不會發生）
            }
        }
        if vectors.is_empty() {
            return Ok(Vec::new());
        }
        let middle = nearest_distances.len() / 2;
        // nth_element：距離都是非負的有限值（或 +inf），total_cmp 與 operator< 的排序相同。
        let (_, &mut typical_spacing, _) = nearest_distances.select_nth_unstable_by(middle, f64::total_cmp);

        let clusters = self.clustering.cluster(&vectors, typical_spacing)?;
        let most = clusters.clusters.iter().map(|c| c.members).max().unwrap_or(0);
        let mut candidates: Vec<Point> = Vec::new();
        for c in &clusters.clusters {
            if (c.members as f64) < self.o.support_fraction * most as f64 {
                continue;
            }
            if !(length(c.center) > self.min_distance) {
                continue;
            }
            candidates.try_push(half_plane(c.center))?;
        }
        candidates.sort_by(by_less(|a: &Point, b: &Point| length(*a) < length(*b)));
        Ok(candidates)
    }

    /// u 之外的第二個方向 v（列距最小的），已對 u 取餘、cross(u, v) > 0；只有一列就回 None（一維）。
    pub fn second_direction(&self, u: Point, directions: &[Point]) -> Res<Option<Point>> {
        if let Some(v) = Self::from_directions(u, directions) {
            return Ok(Some(v));
        }
        self.between_rows(u)
    }

    fn from_directions(u: Point, directions: &[Point]) -> Option<Point> {
        let lu = length(u);
        let min_spacing = 0.8 * lu;
        let mut best: Option<Point> = None;
        let mut best_spacing = 0.0;
        for &d in directions {
            let spacing = cross(u, d).abs() / lu;
            if spacing < min_spacing {
                continue; // 和 u 平行（或就是 u），或列距不可能
            }
            if better(best, best_spacing, d, spacing, lu) {
                best = Some(d);
                best_spacing = spacing;
            }
        }
        let best = best?;
        let v = if cross(u, best) < 0.0 { times(best, -1.0) } else { best };
        Some(minus(v, times(u, (dot(v, u) / dot(u, u)).round())))
    }

    fn between_rows(&self, u: Point) -> Res<Option<Point>> {
        let lu = length(u);
        let uh = times(u, 1.0 / lu);
        let nh = Point { x: -uh.y, y: uh.x }; // cross(uh, nh) > 0
        let row_gap = 0.4 * lu;
        let n = self.pts.len();

        let mut by_t: Vec<(f64, u32)> = vec_with_capacity(n)?;
        for (k, &p) in self.pts.iter().enumerate() {
            by_t.push((dot(p, nh), k as u32));
        }
        by_t.sort_by(by_less(pair_less));
        let mut row_start: Vec<usize> = vec_with_capacity(2)?;
        row_start.push(0);
        for k in 1..n {
            if by_t[k].0 - by_t[k - 1].0 > row_gap {
                row_start.try_push(k)?;
            }
        }
        row_start.try_push(n)?;
        let rows = row_start.len() - 1;
        if rows < 2 {
            return Ok(None); // 只有一列：一維
        }
        let mut by_s: Vec<(f64, u32)> = vec_with_capacity(n)?;
        for &(_, index) in &by_t {
            by_s.push((dot(self.pts[index as usize], uh), index));
        }
        for r in 0..rows {
            by_s[row_start[r]..row_start[r + 1]].sort_by(by_less(pair_less));
        }

        const SAMPLES_PER_ROW: usize = 16;
        let capacity = (2 * rows).saturating_mul(SAMPLES_PER_ROW).saturating_add(16);
        let mut vectors: Vec<Point> = vec_with_capacity(capacity)?;
        let mut targets: Vec<u32> = vec_with_capacity(capacity)?;
        for r in 0..rows - 1 {
            let from_begin = row_start[r];
            let from_size = row_start[r + 1] - from_begin;
            let step = 1usize.max(from_size / SAMPLES_PER_ROW);
            for r2 in r + 1..=(r + 2).min(rows - 1) {
                // 下一列與下下一列
                let to = &by_s[row_start[r2]..row_start[r2 + 1]];
                let mut a = 0usize;
                while a < from_size {
                    let from = by_s[from_begin + a];
                    // 另一列裡沿 u 方向最接近的點（二分搜尋）。
                    let it = to.partition_point(|e| e.0 < from.0);
                    let mut best = if it == to.len() { to.len() - 1 } else { it };
                    if it != 0 && (it == to.len() || (to[it - 1].0 - from.0).abs() <= (to[it].0 - from.0).abs()) {
                        best = it - 1;
                    }
                    let target = to[best].1;
                    let mut d = minus(self.pts[target as usize], self.pts[from.1 as usize]);
                    d = minus(d, times(u, f64::from(fast_round(dot(d, uh) / lu))));
                    vectors.try_push(d)?;
                    targets.try_push(target)?;
                    a += step;
                }
            }
        }
        if vectors.is_empty() {
            return Ok(None);
        }

        let clusters = self.clustering.cluster(&vectors, lu)?;
        // 每一群指向幾個不同的點：(群, 目標) 打包成一個 64 位元的鍵，排序後數不重複的。
        let mut pointed: Vec<u64> = vec_with_capacity(vectors.len())?;
        for k in 0..vectors.len() {
            if clusters.labels[k] >= 0 {
                pointed.push((u64::from(clusters.labels[k] as u32) << 32) | u64::from(targets[k]));
            }
        }
        pointed.sort_unstable();
        pointed.dedup();
        let mut distinct: Vec<usize> = vec_filled(clusters.clusters.len(), 0)?;
        for &key in &pointed {
            distinct[(key >> 32) as usize] += 1;
        }
        let most = distinct.iter().copied().max().unwrap_or(0);

        let mut best: Option<Point> = None;
        let mut best_spacing = 0.0;
        for (c, cluster) in clusters.clusters.iter().enumerate() {
            if distinct[c] < 2 || (distinct[c] as f64) < self.o.support_fraction * most as f64 {
                continue;
            }
            let center = cluster.center;
            let spacing = cross(u, center) / lu; // 列距；依建構方式一定是正的
            if spacing < row_gap {
                continue;
            }
            if better(best, best_spacing, center, spacing, lu) {
                best = Some(center);
                best_spacing = spacing;
            }
        }
        Ok(best.map(|b| minus(b, times(u, (dot(b, uh) / lu).round()))))
    }
}
