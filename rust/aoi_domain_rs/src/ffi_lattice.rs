//! 晶格（R05.2）的 C ABI。標頭是 `src/domain/ffi/aoi_domain_rs.h`：改這裡就要改那裡（`AOI_RS_ABI_VERSION` 也要加 1）。
//!
//! 規則和 `ffi` 相同（`catch_unwind`、`try_reserve`、誰配置誰釋放），另外：
//! - 長度不固定的輸出（方向的候選、預測的格點、估計的結果）是 handle：用對應的 `*_get`／`*_model` 讀，用 `*_free` 釋放；
//!   讀到的指標指向 handle 裡的記憶體，釋放之後不可以再用。
//! - 長度有上限的輸出（分群、鄰居、連通的索引）由呼叫端配置。
//! - 自訂的分群策略：C 的函式指標＋`context`（C++ 的 `IVectorClustering`）。回呼回報失敗時整個呼叫回傳
//!   `AOI_RS_CALLBACK_FAILED`（C++ 的包裝重新丟出回呼記下的例外）。回呼的輸出違反契約（群數比向量多、
//!   標號超出群數）也是 `AOI_RS_CALLBACK_FAILED`：C++ 的實作遇到這種輸出是未定義行為，這裡明確失敗。

use std::ffi::c_void;

use crate::fallible::{Fail, Res, vec_filled};
use crate::ffi::{AOI_RS_CALLBACK_FAILED, AOI_RS_INVALID_ARGUMENT, AOI_RS_NO_MEMORY, AOI_RS_OK, guard};
use crate::geometry::Point;
use crate::lattice::acceptance;
use crate::lattice::accumulator::Accumulator;
use crate::lattice::basis::{Basis, Bounds, Cell};
use crate::lattice::basis_finder::BasisFinder;
use crate::lattice::cell_components::largest_component;
use crate::lattice::clustering::{Clustering, VectorCluster, VectorClusters};
use crate::lattice::detail::Candidate;
use crate::lattice::estimator::fit_lattices;
use crate::lattice::model::{Fit, Model};
use crate::lattice::options::Options;
use crate::lattice::predictor::{self, ModelGeometry, PredictedCell};
use crate::lattice::region::Region;
use crate::lattice::spatial_grid::{Neighbor, OwnedSpatialGrid};

pub const AOI_RS_CLUSTER_GRID_VOTE: i32 = 0;
pub const AOI_RS_CLUSTER_GREEDY: i32 = 1;
pub const AOI_RS_CLUSTER_CALLBACK: i32 = 2;

pub const AOI_RS_PREDICT_IN_RANGE: i32 = 0;
pub const AOI_RS_PREDICT_HOLES: i32 = 1;
pub const AOI_RS_PREDICT_ALL_IN_BOUNDS: i32 = 2;

/// `AoiRsBasis`：`has_v` 是 `int32_t`（C 的標頭只用 `<stdint.h>`）。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct FfiBasis {
    pub origin: Point,
    pub u: Point,
    pub v: Point,
    pub has_v: i32,
}

impl From<FfiBasis> for Basis {
    fn from(b: FfiBasis) -> Self {
        Basis {
            origin: b.origin,
            u: b.u,
            v: b.v,
            has_v: b.has_v != 0,
        }
    }
}

impl From<Basis> for FfiBasis {
    fn from(b: Basis) -> Self {
        FfiBasis {
            origin: b.origin,
            u: b.u,
            v: b.v,
            has_v: i32::from(b.has_v),
        }
    }
}

/// 自訂分群的回呼：把 `count` 個向量分群，群寫進 `clusters`（容量 `count`）、群數寫進 `*cluster_count`、
/// 每個向量的標號寫進 `labels`（`count` 個）。成功回傳 `AOI_RS_OK`。
pub type ClusterFn = unsafe extern "C" fn(
    context: *mut c_void,
    vectors: *const Point,
    count: usize,
    scale: f64,
    clusters: *mut VectorCluster,
    cluster_count: *mut usize,
    labels: *mut i32,
) -> i32;

/// `AoiRsClustering`：kind 是 `AOI_RS_CLUSTER_*`；parameter 是網格投票的 binFraction 或貪婪合併的 tolerance。
#[repr(C)]
pub struct FfiClustering {
    pub kind: i32,
    pub parameter: f64,
    pub callback: Option<ClusterFn>,
    pub context: *mut c_void,
}

/// `AoiRsLatticeModel`：C++ 的 `LatticeModel`（輸入時指標指向 C++ 的陣列；從估計的結果讀出時指向 handle 裡的陣列）。
#[repr(C)]
pub struct FfiModel {
    pub basis: FfiBasis,
    pub rms_px: f64,
    pub tolerance_px: f64,
    pub coverage: f64,
    pub consistency: f64,
    pub confidence: f64,
    pub suggest_check: i32,
    pub min_i: i32,
    pub max_i: i32,
    pub min_j: i32,
    pub max_j: i32,
    pub equations: Accumulator,
    pub inliers: *const usize,
    pub inlier_count: usize,
    pub cells: *const Cell,
    pub cell_count: usize,
}

/// `AoiRsLatticeCandidate`：`detail::LatticeCandidate`。
#[repr(C)]
pub struct FfiCandidate {
    pub basis: FfiBasis,
    pub tolerance: f64,
    pub members: *const usize,
    pub member_count: usize,
    pub cells: *const Cell,
    pub cell_count: usize,
}

const _: () = assert!(std::mem::size_of::<FfiBasis>() == 56);
const _: () = assert!(std::mem::size_of::<FfiClustering>() == 32);
const _: () = assert!(std::mem::size_of::<FfiModel>() == 296);
const _: () = assert!(std::mem::size_of::<FfiCandidate>() == 96);
const _: () = assert!(std::mem::size_of::<VectorCluster>() == 24);
const _: () = assert!(std::mem::size_of::<PredictedCell>() == 24);
const _: () = assert!(std::mem::size_of::<Neighbor>() == 16);

/// 方向的候選（`aoi_rs_basis_directions` 的輸出）。
pub struct PointList(Vec<Point>);
/// 預測的格點（`aoi_rs_lattice_predict` 的輸出）。
pub struct PredictedList(Vec<PredictedCell>);
/// 估計的結果（`aoi_rs_lattice_fit` 的輸出）。
pub struct FitResult(Fit);

fn status_of(fail: Fail) -> i32 {
    match fail {
        Fail::NoMemory => AOI_RS_NO_MEMORY,
        Fail::Callback(_) => AOI_RS_CALLBACK_FAILED,
    }
}

/// 呼叫端給的陣列換成切片（`count` 是 0 時指標可以是 null）。
///
/// # Safety
/// `ptr` 指向 `count` 個有效的元素（或 count 是 0）。
unsafe fn slice<'a, T>(ptr: *const T, count: usize) -> Option<&'a [T]> {
    if count == 0 {
        return Some(&[]);
    }
    if ptr.is_null() {
        return None;
    }
    // SAFETY: 呼叫端保證 ptr 指向 count 個元素。
    Some(unsafe { std::slice::from_raw_parts(ptr, count) })
}

/// 回呼：把 C 的函式指標包成 `Clustering::External` 用的閉包。
fn call_external(callback: ClusterFn, context: *mut c_void, vectors: &[Point], scale: f64) -> Res<VectorClusters> {
    let n = vectors.len();
    let mut clusters: Vec<VectorCluster> = vec_filled(n, VectorCluster::default())?;
    let mut labels: Vec<i32> = vec_filled(n, -1)?;
    let mut count = 0usize;
    // SAFETY: 回呼由 C++ 提供（ADR-031 第 5 點）；兩個輸出陣列各有 n 個元素。
    let status = unsafe {
        callback(
            context,
            vectors.as_ptr(),
            n,
            scale,
            clusters.as_mut_ptr(),
            &mut count,
            labels.as_mut_ptr(),
        )
    };
    if status != AOI_RS_OK {
        return Err(Fail::Callback(status));
    }
    if count > n || labels.iter().any(|&l| l >= 0 && l as usize >= count) {
        return Err(Fail::Callback(AOI_RS_INVALID_ARGUMENT));
    }
    clusters.truncate(count);
    Ok(VectorClusters { clusters, labels })
}

/// 依 `AoiRsClustering` 建立策略，交給 `f`。參數不對時回傳狀態碼。
///
/// # Safety
/// `spec` 指向有效的 `FfiClustering`（或 null：回傳 `AOI_RS_INVALID_ARGUMENT`）。
unsafe fn with_clustering(spec: *const FfiClustering, f: impl FnOnce(Clustering<'_>) -> i32) -> i32 {
    if spec.is_null() {
        return AOI_RS_INVALID_ARGUMENT;
    }
    // SAFETY: spec 有效（上面已排除 null）。
    let spec = unsafe { &*spec };
    match spec.kind {
        AOI_RS_CLUSTER_GRID_VOTE => f(Clustering::GridVote {
            bin_fraction: spec.parameter,
        }),
        AOI_RS_CLUSTER_GREEDY => f(Clustering::Greedy { tolerance: spec.parameter }),
        AOI_RS_CLUSTER_CALLBACK => {
            let Some(callback) = spec.callback else {
                return AOI_RS_INVALID_ARGUMENT;
            };
            let context = spec.context;
            let external = move |vectors: &[Point], scale: f64| call_external(callback, context, vectors, scale);
            f(Clustering::External(&external))
        }
        _ => AOI_RS_INVALID_ARGUMENT,
    }
}

fn model_view(m: &Model) -> FfiModel {
    FfiModel {
        basis: m.basis.into(),
        rms_px: m.rms_px,
        tolerance_px: m.tolerance_px,
        coverage: m.coverage,
        consistency: m.consistency,
        confidence: m.confidence,
        suggest_check: i32::from(m.suggest_check),
        min_i: m.min_i,
        max_i: m.max_i,
        min_j: m.min_j,
        max_j: m.max_j,
        equations: m.equations,
        inliers: m.inliers.as_ptr(),
        inlier_count: m.inliers.len(),
        cells: m.cells.as_ptr(),
        cell_count: m.cells.len(),
    }
}

/// C 的候選換成 Rust 的（複製成員與格子）。
///
/// # Safety
/// `c` 的陣列指標指向對應數量的元素。
unsafe fn candidate_from(c: &FfiCandidate) -> Option<Res<Candidate>> {
    // SAFETY: 呼叫端保證陣列有效。
    let members = unsafe { slice(c.members, c.member_count) }?;
    // SAFETY: 同上。
    let cells = unsafe { slice(c.cells, c.cell_count) }?;
    Some((|| {
        Ok(Candidate {
            basis: c.basis.into(),
            tolerance: c.tolerance,
            members: crate::fallible::vec_from_slice(members)?,
            cells: crate::fallible::vec_from_slice(cells)?,
        })
    })())
}

// ---- 累加器（不配置記憶體）----

/// 加（weight > 0）或拿掉（weight ≤ 0）一個點。
///
/// # Safety
/// `sums` 指向有效、可寫的 `AoiRsLatticeSums`。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_sums_accumulate(sums: *mut Accumulator, cell: Cell, p: Point, weight: f64) -> i32 {
    guard(|| {
        if sums.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: sums 有效（上面已排除 null）。
        unsafe { (*sums).accumulate(cell, p, weight) };
        AOI_RS_OK
    })
}

/// 可以解出 v（二維）？`*out` 是 1 或 0。
///
/// # Safety
/// `sums` 指向有效的 `AoiRsLatticeSums`；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_sums_two_dimensional(sums: *const Accumulator, out: *mut i32) -> i32 {
    guard(|| {
        if sums.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 兩個指標有效（上面已排除 null）。
        unsafe { *out = i32::from((*sums).two_dimensional()) };
        AOI_RS_OK
    })
}

/// 最小平方解；解不出時 `*solved` 是 0（`*out` 不變）。
///
/// # Safety
/// `sums` 指向有效的 `AoiRsLatticeSums`；`out`、`solved` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_sums_solve(sums: *const Accumulator, out: *mut FfiBasis, solved: *mut i32) -> i32 {
    guard(|| {
        if sums.is_null() || out.is_null() || solved.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 三個指標有效（上面已排除 null）。
        unsafe {
            match (*sums).solve() {
                Some(b) => {
                    *out = b.into();
                    *solved = 1;
                }
                None => *solved = 0,
            }
        }
        AOI_RS_OK
    })
}

/// 殘差平方和。
///
/// # Safety
/// `sums`、`basis` 指向有效的結構；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_sums_residual_sum_of_squares(
    sums: *const Accumulator,
    basis: *const FfiBasis,
    out: *mut f64,
) -> i32 {
    guard(|| {
        if sums.is_null() || basis.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 三個指標有效（上面已排除 null）。
        unsafe { *out = (*sums).residual_sum_of_squares(&Basis::from(*basis)) };
        AOI_RS_OK
    })
}

// ---- 空間格子 ----

/// 建立空間格子（複製一份點）；成功時 `*out` 是新的 handle（用 `aoi_rs_spatial_grid_free` 釋放）。
///
/// # Safety
/// `points` 指向 `count` 個點；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_spatial_grid_new(
    points: *const Point,
    count: usize,
    cell_size: f64,
    out: *mut *mut OwnedSpatialGrid,
) -> i32 {
    guard(|| {
        if out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 呼叫端保證 points 指向 count 個點。
        let Some(pts) = (unsafe { slice(points, count) }) else {
            return AOI_RS_INVALID_ARGUMENT;
        };
        match OwnedSpatialGrid::new(pts, cell_size) {
            Ok(grid) => {
                // SAFETY: out 有效（上面已排除 null）。
                unsafe { *out = Box::into_raw(Box::new(grid)) };
                AOI_RS_OK
            }
            Err(e) => status_of(e),
        }
    })
}

/// 釋放空間格子。null 不做事。
///
/// # Safety
/// `grid` 是 `aoi_rs_spatial_grid_new` 給的、還沒釋放的 handle（或 null）。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_spatial_grid_free(grid: *mut OwnedSpatialGrid) {
    let _ = guard(|| {
        if !grid.is_null() {
            // SAFETY: grid 由 Box::into_raw 產生、只釋放一次（呼叫端保證）。
            drop(unsafe { Box::from_raw(grid) });
        }
        AOI_RS_OK
    });
}

/// 格子的邊長。
///
/// # Safety
/// `grid` 是有效的 handle；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_spatial_grid_cell_size(grid: *const OwnedSpatialGrid, out: *mut f64) -> i32 {
    guard(|| {
        if grid.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 兩個指標有效（上面已排除 null）。
        unsafe { *out = (*grid).grid.cell_size() };
        AOI_RS_OK
    })
}

/// 最近的 k 個鄰居，由近到遠寫進 `out`（呼叫端配置 `capacity` 個；capacity 至少 min(k, 點數)）。
///
/// # Safety
/// `grid` 是有效的 handle；`out` 可寫 `capacity` 個；`out_count` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_spatial_grid_nearest(
    grid: *const OwnedSpatialGrid,
    self_index: usize,
    k: usize,
    min_distance: f64,
    out: *mut Neighbor,
    capacity: usize,
    out_count: *mut usize,
) -> i32 {
    guard(|| {
        if grid.is_null() || out_count.is_null() || (capacity > 0 && out.is_null()) {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: grid 有效（上面已排除 null）。
        let grid = unsafe { &*grid };
        let mut found = Vec::new();
        if let Err(e) = grid.nearest(self_index, k, min_distance, &mut found) {
            return status_of(e);
        }
        if found.len() > capacity {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: out 可寫 capacity ≥ found.len() 個；out_count 有效。
        unsafe {
            if !found.is_empty() {
                std::ptr::copy_nonoverlapping(found.as_ptr(), out, found.len());
            }
            *out_count = found.len();
        }
        AOI_RS_OK
    })
}

// ---- 分群 ----

/// 分群：群寫進 `clusters`（呼叫端配置 `count` 個）、群數寫進 `*cluster_count`、標號寫進 `labels`（`count` 個）。
///
/// # Safety
/// `clustering` 有效；`vectors` 指向 `count` 個點；`clusters`、`labels` 各可寫 `count` 個；`cluster_count` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_cluster_vectors(
    clustering: *const FfiClustering,
    vectors: *const Point,
    count: usize,
    scale: f64,
    clusters: *mut VectorCluster,
    cluster_count: *mut usize,
    labels: *mut i32,
) -> i32 {
    guard(|| {
        if cluster_count.is_null() || (count > 0 && (clusters.is_null() || labels.is_null())) {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 呼叫端保證 vectors 指向 count 個點。
        let Some(v) = (unsafe { slice(vectors, count) }) else {
            return AOI_RS_INVALID_ARGUMENT;
        };
        // SAFETY: clustering 由 with_clustering 檢查 null。
        unsafe {
            with_clustering(clustering, |c| match c.cluster(v, scale) {
                Ok(r) => {
                    if r.clusters.len() > count || r.labels.len() != count {
                        return AOI_RS_CALLBACK_FAILED;
                    }
                    // SAFETY: 輸出陣列各可寫 count 個（呼叫端保證）。
                    if count > 0 {
                        std::ptr::copy_nonoverlapping(r.clusters.as_ptr(), clusters, r.clusters.len());
                        std::ptr::copy_nonoverlapping(r.labels.as_ptr(), labels, count);
                    }
                    *cluster_count = r.clusters.len();
                    AOI_RS_OK
                }
                Err(e) => status_of(e),
            })
        }
    })
}

// ---- 找基底 ----

/// 方向的候選（`BasisFinder::directions`）；成功時 `*out` 是新的 handle（`aoi_rs_point_list_get` 讀、`aoi_rs_point_list_free` 釋放）。
///
/// # Safety
/// `options`、`clustering` 有效；`points` 指向 `count` 個點；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_basis_directions(
    options: *const Options,
    clustering: *const FfiClustering,
    points: *const Point,
    count: usize,
    min_distance: f64,
    out: *mut *mut PointList,
) -> i32 {
    guard(|| {
        if options.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 呼叫端保證 points 指向 count 個點。
        let Some(pts) = (unsafe { slice(points, count) }) else {
            return AOI_RS_INVALID_ARGUMENT;
        };
        // SAFETY: options 有效（上面已排除 null）；clustering 由 with_clustering 檢查。
        unsafe {
            let o = &*options;
            with_clustering(clustering, |c| match BasisFinder::new(o, c, pts, min_distance).directions() {
                Ok(dirs) => {
                    *out = Box::into_raw(Box::new(PointList(dirs)));
                    AOI_RS_OK
                }
                Err(e) => status_of(e),
            })
        }
    })
}

/// 第二個方向（`BasisFinder::secondDirection`）；沒有時 `*found` 是 0。
///
/// # Safety
/// 同 `aoi_rs_basis_directions`；`directions` 指向 `direction_count` 個點；`out`、`found` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_basis_second_direction(
    options: *const Options,
    clustering: *const FfiClustering,
    points: *const Point,
    count: usize,
    min_distance: f64,
    u: Point,
    directions: *const Point,
    direction_count: usize,
    out: *mut Point,
    found: *mut i32,
) -> i32 {
    guard(|| {
        if options.is_null() || out.is_null() || found.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 呼叫端保證陣列有效。
        let (Some(pts), Some(dirs)) = (unsafe { slice(points, count) }, unsafe { slice(directions, direction_count) }) else {
            return AOI_RS_INVALID_ARGUMENT;
        };
        // SAFETY: options、out、found 有效（上面已排除 null）；clustering 由 with_clustering 檢查。
        unsafe {
            let o = &*options;
            with_clustering(clustering, |c| {
                match BasisFinder::new(o, c, pts, min_distance).second_direction(u, dirs) {
                    Ok(v) => {
                        *found = i32::from(v.is_some());
                        if let Some(v) = v {
                            *out = v;
                        }
                        AOI_RS_OK
                    }
                    Err(e) => status_of(e),
                }
            })
        }
    })
}

/// 讀點的清單。
///
/// # Safety
/// `list` 是有效的 handle；`data`、`count` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_point_list_get(list: *const PointList, data: *mut *const Point, count: *mut usize) -> i32 {
    guard(|| {
        if list.is_null() || data.is_null() || count.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 三個指標有效（上面已排除 null）。
        unsafe {
            *data = (*list).0.as_ptr();
            *count = (*list).0.len();
        }
        AOI_RS_OK
    })
}

/// 釋放點的清單。null 不做事。
///
/// # Safety
/// `list` 是還沒釋放的 handle（或 null）。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_point_list_free(list: *mut PointList) {
    let _ = guard(|| {
        if !list.is_null() {
            // SAFETY: list 由 Box::into_raw 產生、只釋放一次（呼叫端保證）。
            drop(unsafe { Box::from_raw(list) });
        }
        AOI_RS_OK
    });
}

// ---- 連通、接受規則 ----

/// 成員最多的那一塊的索引，寫進 `out_indices`（呼叫端配置 `count` 個）。
///
/// # Safety
/// `cells` 指向 `count` 個格子；`out_indices` 可寫 `count` 個；`out_count` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_largest_component(
    cells: *const Cell,
    count: usize,
    link: i32,
    two_dimensional: i32,
    out_indices: *mut usize,
    out_count: *mut usize,
) -> i32 {
    guard(|| {
        if out_count.is_null() || (count > 0 && out_indices.is_null()) {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 呼叫端保證 cells 指向 count 個格子。
        let Some(c) = (unsafe { slice(cells, count) }) else {
            return AOI_RS_INVALID_ARGUMENT;
        };
        match largest_component(c, link, two_dimensional != 0) {
            Ok(keep) => {
                // SAFETY: out_indices 可寫 count ≥ keep.len() 個；out_count 有效。
                unsafe {
                    if !keep.is_empty() {
                        std::ptr::copy_nonoverlapping(keep.as_ptr(), out_indices, keep.len());
                    }
                    *out_count = keep.len();
                }
                AOI_RS_OK
            }
            Err(e) => status_of(e),
        }
    })
}

/// `twoDimensionalIsValid`（`which` ＝ 0）或 `enoughEvidence`（`which` ＝ 1）。`*out` 是 1 或 0。
///
/// # Safety
/// `candidate` 與它的陣列有效；`points` 指向 `count` 個點；`options` 有效；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_candidate_rule(
    which: i32,
    candidate: *const FfiCandidate,
    points: *const Point,
    count: usize,
    options: *const Options,
    out: *mut i32,
) -> i32 {
    guard(|| {
        if candidate.is_null() || options.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 呼叫端保證指標與陣列有效（null 已排除）。
        let (Some(pts), Some(c)) = (unsafe { slice(points, count) }, unsafe { candidate_from(&*candidate) }) else {
            return AOI_RS_INVALID_ARGUMENT;
        };
        let c = match c {
            Ok(c) => c,
            Err(e) => return status_of(e),
        };
        // SAFETY: options 有效。
        let o = unsafe { &*options };
        let result = match which {
            0 => acceptance::two_dimensional_is_valid(&c, pts, o),
            1 => acceptance::enough_evidence(&c, pts, o),
            _ => return AOI_RS_INVALID_ARGUMENT,
        };
        match result {
            Ok(v) => {
                // SAFETY: out 有效。
                unsafe { *out = i32::from(v) };
                AOI_RS_OK
            }
            Err(e) => status_of(e),
        }
    })
}

/// `holesAreBacked`。`*out` 是 1 或 0。
///
/// # Safety
/// `model` 與它的 `cells` 有效（R07.2 起索引區域要看內點的格子；`inliers` 不讀，只要 `inlier_count`）；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_holes_are_backed(model: *const FfiModel, out: *mut i32) -> i32 {
    guard(|| {
        if model.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: model 有效（上面已排除 null），cells 由呼叫端保證。
        let m = unsafe { &*model };
        let Some(cells) = (unsafe { slice(m.cells, m.cell_count) }) else {
            return AOI_RS_INVALID_ARGUMENT;
        };
        let basis: Basis = m.basis.into();
        let region = match Region::of(&basis, m.min_i, m.max_i, m.min_j, m.max_j, cells) {
            Ok(r) => r,
            Err(e) => return status_of(e),
        };
        let backed = acceptance::holes_are_backed_of(m.inlier_count, region.evidence_count(&basis), basis.has_v);
        // SAFETY: out 有效。
        unsafe { *out = i32::from(backed) };
        AOI_RS_OK
    })
}

/// `consistency`。
///
/// # Safety
/// `model` 與它的陣列有效；`points` 指向 `count` 個點；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_consistency(model: *const FfiModel, points: *const Point, count: usize, out: *mut f64) -> i32 {
    guard(|| {
        if model.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: model 有效（上面已排除 null），陣列由呼叫端保證。
        let m = unsafe { &*model };
        let (Some(pts), Some(cells)) = (unsafe { slice(points, count) }, unsafe { slice(m.cells, m.cell_count) }) else {
            return AOI_RS_INVALID_ARGUMENT;
        };
        match acceptance::consistency_of(&m.basis.into(), m.tolerance_px, m.min_i, m.max_i, m.min_j, m.max_j, cells, pts) {
            Ok(v) => {
                // SAFETY: out 有效。
                unsafe { *out = v };
                AOI_RS_OK
            }
            Err(e) => status_of(e),
        }
    })
}

// ---- 估計 ----

/// 晶格估計（`LatticeEstimator` 的 fit／fitAll，最多 `max_models` 組）；成功時 `*out` 是新的 handle
/// （`aoi_rs_lattice_fit_summary`、`aoi_rs_lattice_fit_model` 讀，`aoi_rs_lattice_fit_free` 釋放）。
///
/// # Safety
/// `options`、`clustering` 有效；`points` 指向 `count` 個點；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_fit(
    options: *const Options,
    clustering: *const FfiClustering,
    points: *const Point,
    count: usize,
    max_models: usize,
    out: *mut *mut FitResult,
) -> i32 {
    guard(|| {
        if options.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 呼叫端保證 points 指向 count 個點。
        let Some(pts) = (unsafe { slice(points, count) }) else {
            return AOI_RS_INVALID_ARGUMENT;
        };
        // SAFETY: options、out 有效（上面已排除 null）；clustering 由 with_clustering 檢查。
        unsafe {
            let o = &*options;
            with_clustering(clustering, |c| match fit_lattices(o, c, pts, max_models) {
                Ok(fit) => {
                    *out = Box::into_raw(Box::new(FitResult(fit)));
                    AOI_RS_OK
                }
                Err(e) => status_of(e),
            })
        }
    })
}

/// 估計結果的摘要：沒有晶格的原因（`NoLatticeReason` 的數值）、被擋掉的信心、晶格數。
///
/// # Safety
/// `fit` 是有效的 handle；三個輸出可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_fit_summary(
    fit: *const FitResult,
    reason: *mut i32,
    rejected_confidence: *mut f64,
    model_count: *mut usize,
) -> i32 {
    guard(|| {
        if fit.is_null() || reason.is_null() || rejected_confidence.is_null() || model_count.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 指標有效（上面已排除 null）。
        unsafe {
            let f = &(*fit).0;
            *reason = f.reason as i32;
            *rejected_confidence = f.rejected_confidence;
            *model_count = f.models.len();
        }
        AOI_RS_OK
    })
}

/// 第 `index` 組晶格；`*out` 的陣列指標指向 handle 裡的記憶體（釋放 handle 之前有效）。
///
/// # Safety
/// `fit` 是有效的 handle；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_fit_model(fit: *const FitResult, index: usize, out: *mut FfiModel) -> i32 {
    guard(|| {
        if fit.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 指標有效（上面已排除 null）。
        unsafe {
            let f = &*fit;
            let Some(m) = f.0.models.get(index) else {
                return AOI_RS_INVALID_ARGUMENT;
            };
            *out = model_view(m);
        }
        AOI_RS_OK
    })
}

/// 釋放估計的結果。null 不做事。
///
/// # Safety
/// `fit` 是還沒釋放的 handle（或 null）。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_fit_free(fit: *mut FitResult) {
    let _ = guard(|| {
        if !fit.is_null() {
            // SAFETY: fit 由 Box::into_raw 產生、只釋放一次（呼叫端保證）。
            drop(unsafe { Box::from_raw(fit) });
        }
        AOI_RS_OK
    });
}

// ---- 預測 ----

/// 預測格點：`mode` 是 `AOI_RS_PREDICT_*`（`margin` 只用在 IN_RANGE）；成功時 `*out` 是新的 handle
/// （`aoi_rs_predicted_list_get` 讀、`aoi_rs_predicted_list_free` 釋放）。
///
/// # Safety
/// `model` 與它的 `cells` 有效；`out` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_predict(
    model: *const FfiModel,
    bounds: Bounds,
    mode: i32,
    margin: i32,
    max_count: usize,
    out: *mut *mut PredictedList,
) -> i32 {
    guard(|| {
        if model.is_null() || out.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: model 有效（上面已排除 null），cells 由呼叫端保證。
        let m = unsafe { &*model };
        let Some(cells) = (unsafe { slice(m.cells, m.cell_count) }) else {
            return AOI_RS_INVALID_ARGUMENT;
        };
        let geometry = ModelGeometry {
            basis: m.basis.into(),
            min_i: m.min_i,
            max_i: m.max_i,
            min_j: m.min_j,
            max_j: m.max_j,
            cells,
        };
        let result = match mode {
            AOI_RS_PREDICT_IN_RANGE => predictor::cells_in_range(&geometry, bounds, margin, max_count),
            AOI_RS_PREDICT_HOLES => predictor::holes(&geometry, bounds, max_count),
            AOI_RS_PREDICT_ALL_IN_BOUNDS => predictor::all_in_bounds(&geometry, bounds, max_count),
            _ => return AOI_RS_INVALID_ARGUMENT,
        };
        match result {
            Ok(list) => {
                // SAFETY: out 有效。
                unsafe { *out = Box::into_raw(Box::new(PredictedList(list))) };
                AOI_RS_OK
            }
            Err(e) => status_of(e),
        }
    })
}

/// 讀預測的格點。
///
/// # Safety
/// `list` 是有效的 handle；`data`、`count` 可寫。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_predicted_list_get(list: *const PredictedList, data: *mut *const PredictedCell, count: *mut usize) -> i32 {
    guard(|| {
        if list.is_null() || data.is_null() || count.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: 三個指標有效（上面已排除 null）。
        unsafe {
            *data = (*list).0.as_ptr();
            *count = (*list).0.len();
        }
        AOI_RS_OK
    })
}

/// 釋放預測的格點。null 不做事。
///
/// # Safety
/// `list` 是還沒釋放的 handle（或 null）。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_predicted_list_free(list: *mut PredictedList) {
    let _ = guard(|| {
        if !list.is_null() {
            // SAFETY: list 由 Box::into_raw 產生、只釋放一次（呼叫端保證）。
            drop(unsafe { Box::from_raw(list) });
        }
        AOI_RS_OK
    });
}

/// 由近到遠排序（就地）。
///
/// # Safety
/// `cells` 指向 `count` 個可寫的格點（count 是 0 時可以是 null）。
#[unsafe(no_mangle)]
pub unsafe extern "C" fn aoi_rs_lattice_order_near_to_far(cells: *mut PredictedCell, count: usize, anchor: Point) -> i32 {
    guard(|| {
        if count == 0 {
            return AOI_RS_OK;
        }
        if cells.is_null() {
            return AOI_RS_INVALID_ARGUMENT;
        }
        // SAFETY: cells 指向 count 個可寫的元素（呼叫端保證）。
        let s = unsafe { std::slice::from_raw_parts_mut(cells, count) };
        match predictor::order_near_to_far(s, anchor) {
            Ok(()) => AOI_RS_OK,
            Err(e) => status_of(e),
        }
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn failing(
        _: *mut c_void,
        _: *const Point,
        _: usize,
        _: f64,
        _: *mut VectorCluster,
        _: *mut usize,
        _: *mut i32,
    ) -> i32 {
        7
    }

    #[test]
    fn a_failing_callback_fails_the_fit() {
        let pts: Vec<Point> = (0..12)
            .map(|k| Point {
                x: f64::from(k % 4) * 10.0,
                y: f64::from(k / 4) * 10.0,
            })
            .collect();
        let spec = FfiClustering {
            kind: AOI_RS_CLUSTER_CALLBACK,
            parameter: 0.0,
            callback: Some(failing),
            context: std::ptr::null_mut(),
        };
        let o = Options::default();
        let mut handle: *mut FitResult = std::ptr::null_mut();
        // SAFETY: 指標都指向這裡的區域變數。
        unsafe {
            assert_eq!(
                aoi_rs_lattice_fit(&o, &spec, pts.as_ptr(), pts.len(), 4, &mut handle),
                AOI_RS_CALLBACK_FAILED
            );
            assert!(handle.is_null());
            let grid = FfiClustering {
                kind: AOI_RS_CLUSTER_GRID_VOTE,
                parameter: 0.1,
                callback: None,
                context: std::ptr::null_mut(),
            };
            assert_eq!(aoi_rs_lattice_fit(&o, &grid, pts.as_ptr(), pts.len(), 4, &mut handle), AOI_RS_OK);
            let (mut reason, mut rejected, mut count) = (-1, -1.0, 0usize);
            assert_eq!(
                aoi_rs_lattice_fit_summary(handle, &mut reason, &mut rejected, &mut count),
                AOI_RS_OK
            );
            assert_eq!((reason, count), (0, 1));
            aoi_rs_lattice_fit_free(handle);
        }
    }
}
