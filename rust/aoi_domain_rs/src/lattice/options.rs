//! `LatticeOptions`。`#[repr(C)]`：和 C 的 `AoiRsLatticeOptions` 相同（C++ 的包裝逐欄複製）。

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Options {
    pub inlier_fraction: f64,
    pub neighbors: usize,
    pub max_samples: usize,
    pub support_fraction: f64,
    pub ransac_trials: i32,
    pub ransac_index_radius: i32,
    pub max_gap_cells: i32,
    pub min_second_row_inliers: usize,
    pub min_row_consistency: f64,
    pub min_inliers: usize,
    pub min_confidence: f64,
    pub check_below: f64,
    pub max_lattices: usize,
    pub min_extra_lattice_inliers: usize,
    pub seed: u64,
}

const _: () = assert!(std::mem::size_of::<Options>() == 112);

impl Default for Options {
    /// 和 C++ 的 `LatticeOptions{}` 相同（只給 Rust 的測試用；C++ 一律傳自己的值進來）。
    fn default() -> Self {
        Options {
            inlier_fraction: 0.25,
            neighbors: 4,
            max_samples: 256,
            support_fraction: 0.5,
            ransac_trials: 16,
            ransac_index_radius: 6,
            max_gap_cells: 2,
            min_second_row_inliers: 2,
            min_row_consistency: 0.6,
            min_inliers: 3,
            min_confidence: 0.15,
            check_below: 0.6,
            max_lattices: 4,
            min_extra_lattice_inliers: 3,
            seed: 0x5EED,
        }
    }
}
