//! 位移向量的分群（`vector_clustering.h`）：網格投票（預設）、貪婪合併，以及 C++ 注入的自訂策略（回呼）。
//!
//! 契約和 C++ 相同：空輸入回空結果；不是有限值的向量標 −1；labels 的長度等於輸入；clusters 依成員數由多到少、
//! 同數時短的在前；同樣的輸入永遠同樣的輸出。

use crate::cpp::{by_less, msvc_i32};
use crate::fallible::{Res, TryPush, vec_filled, vec_with_capacity};
use crate::flat_map::{FlatMap, pack_key, unpack_first, unpack_second};
use crate::geometry::Point;
use crate::lattice::detail::finite_point;

/// `VectorCluster`。`#[repr(C)]`：和 C 的 `AoiRsVectorCluster` 相同。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct VectorCluster {
    /// 成員的平均
    pub center: Point,
    /// 成員數
    pub members: usize,
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct VectorClusters {
    pub clusters: Vec<VectorCluster>,
    pub labels: Vec<i32>,
}

/// 自訂策略：C++ 的 `IVectorClustering`（經 FFI 的回呼）。
pub type ExternalClustering<'a> = &'a dyn Fn(&[Point], f64) -> Res<VectorClusters>;

/// 分群策略：內建的兩種用列舉，自訂的用回呼（ADR-031 第 5 點）。
#[derive(Clone, Copy)]
pub enum Clustering<'a> {
    GridVote { bin_fraction: f64 },
    Greedy { tolerance: f64 },
    External(ExternalClustering<'a>),
}

impl Clustering<'_> {
    pub fn cluster(&self, vectors: &[Point], scale: f64) -> Res<VectorClusters> {
        match *self {
            Clustering::GridVote { bin_fraction } => grid_vote(vectors, scale, bin_fraction),
            Clustering::Greedy { tolerance } => greedy(vectors, scale, tolerance),
            Clustering::External(f) => f(vectors, scale),
        }
    }
}

/// floor 的快速版（呼叫端保證 |v| < 1e9）。
fn fast_floor(v: f64) -> i32 {
    let t = msvc_i32(v);
    if v < f64::from(t) { t.wrapping_sub(1) } else { t }
}

/// 兩個策略共用的收尾：每群的成員數與平均、依契約排序、群號換成排序後的索引。
fn finish(vectors: &[Point], mut labels: Vec<i32>, cluster_count: usize) -> Res<VectorClusters> {
    #[derive(Clone, Copy, Default)]
    struct Acc {
        sx: f64,
        sy: f64,
        n: usize,
    }
    let mut acc: Vec<Acc> = vec_filled(cluster_count, Acc::default())?;
    for (k, v) in vectors.iter().enumerate() {
        if labels[k] < 0 {
            continue;
        }
        let a = &mut acc[labels[k] as usize];
        a.sx += v.x;
        a.sy += v.y;
        a.n += 1;
    }
    let mut raw: Vec<VectorCluster> = vec_filled(cluster_count, VectorCluster::default())?;
    for c in 0..cluster_count {
        if acc[c].n == 0 {
            continue;
        }
        let n = acc[c].n as f64;
        raw[c] = VectorCluster {
            center: Point {
                x: acc[c].sx / n,
                y: acc[c].sy / n,
            },
            members: acc[c].n,
        };
    }
    let mut order: Vec<usize> = vec_with_capacity(cluster_count)?;
    order.extend(0..cluster_count);
    order.sort_by(by_less(|&a: &usize, &b: &usize| {
        let ca = &raw[a];
        let cb = &raw[b];
        if ca.members != cb.members {
            return ca.members > cb.members;
        }
        let la = ca.center.x.hypot(ca.center.y);
        let lb = cb.center.x.hypot(cb.center.y);
        if la != lb {
            return la < lb;
        }
        if ca.center.x != cb.center.x {
            return ca.center.x < cb.center.x;
        }
        ca.center.y < cb.center.y
    }));
    let mut out = VectorClusters {
        clusters: vec_with_capacity(cluster_count)?,
        labels: Vec::new(),
    };
    let mut new_index: Vec<i32> = vec_filled(cluster_count, -1)?;
    for &c in &order {
        if raw[c].members == 0 {
            continue;
        }
        new_index[c] = out.clusters.len() as i32;
        out.clusters.push(raw[c]);
    }
    for label in labels.iter_mut() {
        if *label >= 0 {
            *label = new_index[*label as usize];
        }
    }
    out.labels = labels;
    Ok(out)
}

/// 網格投票（`GridVoteClustering`）。
pub fn grid_vote(vectors: &[Point], scale: f64, bin_fraction: f64) -> Res<VectorClusters> {
    let mut bin = bin_fraction * scale;
    if !(bin > 0.0) || !bin.is_finite() {
        bin = 1.0;
    }

    #[derive(Clone, Copy)]
    struct Bin {
        count: usize,
        cluster: i32,
    }
    impl Default for Bin {
        fn default() -> Self {
            Bin { count: 0, cluster: -1 }
        }
    }
    const MAX_BIN: f64 = 1.0e9; // 超出 int 範圍的向量不參與投票
    let n = vectors.len();
    let mut bins: FlatMap<Bin> = FlatMap::new(n)?;
    let mut key_of: Vec<u64> = vec_filled(n, 0)?;
    let mut voted: Vec<u8> = vec_filled(n, 0)?;
    for (k, &d) in vectors.iter().enumerate() {
        if !finite_point(d) {
            continue;
        }
        let fx = d.x / bin;
        let fy = d.y / bin;
        if !(fx.abs() < MAX_BIN && fy.abs() < MAX_BIN) {
            continue;
        }
        key_of[k] = pack_key(fast_floor(fx), fast_floor(fy));
        voted[k] = 1;
        bins.entry(key_of[k])?.count += 1;
    }

    // 峰：票數比 8 個鄰居都多；同票時鍵比較小的勝出。
    #[derive(Clone, Copy)]
    struct Peak {
        key: u64,
        neighbourhood: usize,
    }
    let mut peaks: Vec<Peak> = Vec::new();
    let mut failed = None;
    bins.for_each(|key, b| {
        if failed.is_some() {
            return;
        }
        let bx = unpack_first(key);
        let by = unpack_second(key);
        let mut sum = b.count;
        for dy in -1..=1 {
            for dx in -1..=1 {
                if dx == 0 && dy == 0 {
                    continue;
                }
                let nk = pack_key(bx.wrapping_add(dx), by.wrapping_add(dy));
                let Some(nb) = bins.find(nk) else { continue };
                if nb.count > b.count || (nb.count == b.count && nk < key) {
                    return; // 不是峰
                }
                sum += nb.count;
            }
        }
        if let Err(e) = peaks.try_push(Peak { key, neighbourhood: sum }) {
            failed = Some(e);
        }
    });
    if let Some(e) = failed {
        return Err(e);
    }
    peaks.sort_by(by_less(|a: &Peak, b: &Peak| {
        if a.neighbourhood != b.neighbourhood {
            a.neighbourhood > b.neighbourhood
        } else {
            a.key < b.key
        }
    }));

    // 每一格歸給「3×3 範圍包含它、優先順序最高」的峰。
    for (p, peak) in peaks.iter().enumerate() {
        let bx = unpack_first(peak.key);
        let by = unpack_second(peak.key);
        for dy in -1..=1 {
            for dx in -1..=1 {
                if let Some(b) = bins.find_mut(pack_key(bx.wrapping_add(dx), by.wrapping_add(dy)))
                    && b.cluster < 0
                {
                    b.cluster = p as i32;
                }
            }
        }
    }
    let mut labels: Vec<i32> = vec_filled(n, -1)?;
    for k in 0..n {
        if voted[k] == 0 {
            continue;
        }
        labels[k] = bins.find(key_of[k]).map_or(-1, |b| b.cluster);
    }
    finish(vectors, labels, peaks.len())
}

/// 貪婪合併（`GreedyClustering`）。
pub fn greedy(vectors: &[Point], scale: f64, tolerance: f64) -> Res<VectorClusters> {
    let floor_length = if scale.is_finite() && scale > 0.0 { 0.05 * scale } else { 0.0 };
    let mut order: Vec<usize> = vec_with_capacity(vectors.len())?;
    for (k, &v) in vectors.iter().enumerate() {
        if finite_point(v) {
            order.push(k);
        }
    }
    order.sort_by(by_less(|&a: &usize, &b: &usize| {
        vectors[a].x.hypot(vectors[a].y) < vectors[b].x.hypot(vectors[b].y)
    }));
    #[derive(Clone, Copy, Default)]
    struct Running {
        sx: f64,
        sy: f64,
        n: usize,
    }
    impl Running {
        fn center(&self) -> Point {
            Point {
                x: self.sx / self.n as f64,
                y: self.sy / self.n as f64,
            }
        }
    }
    let mut clusters: Vec<Running> = Vec::new();
    let mut labels: Vec<i32> = vec_filled(vectors.len(), -1)?;
    for &k in &order {
        let d = vectors[k];
        let mut placed: i32 = -1;
        for (c, r) in clusters.iter().enumerate() {
            let center = r.center();
            let reach = tolerance * crate::cpp::cpp_max(center.x.hypot(center.y), floor_length);
            if (center.x - d.x).hypot(center.y - d.y) <= reach {
                placed = c as i32;
                break;
            }
        }
        if placed < 0 {
            placed = clusters.len() as i32;
            clusters.try_push(Running::default())?;
        }
        let r = &mut clusters[placed as usize];
        r.sx += d.x;
        r.sy += d.y;
        r.n += 1;
        labels[k] = placed;
    }
    finish(vectors, labels, clusters.len())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn p(x: f64, y: f64) -> Point {
        Point { x, y }
    }

    #[test]
    fn grid_vote_finds_two_directions() {
        let mut v = Vec::new();
        for k in 0..30 {
            let e = f64::from(k % 3) * 0.1;
            v.push(p(20.0 + e, 0.0));
            v.push(p(0.0, 15.0 - e));
        }
        v.push(p(f64::NAN, 1.0));
        for c in [Clustering::GridVote { bin_fraction: 0.1 }, Clustering::Greedy { tolerance: 0.2 }] {
            let r = c.cluster(&v, 15.0).unwrap();
            assert_eq!(r.labels.len(), v.len());
            assert_eq!(*r.labels.last().unwrap(), -1);
            assert_eq!(r.clusters.len(), 2);
            assert_eq!(r.clusters[0].members, 30);
            assert!(r.clusters[0].center.y > 14.0); // 同數時短的在前
        }
    }

    #[test]
    fn empty_input_gives_empty_output() {
        let r = grid_vote(&[], 10.0, 0.1).unwrap();
        assert!(r.clusters.is_empty() && r.labels.is_empty());
    }
}
