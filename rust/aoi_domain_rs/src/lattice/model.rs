//! `LatticeModel`、`LatticeFit`、`NoLatticeReason`（`lattice_model.h`）與信心分數（`lattice_confidence.h`）。

use crate::cpp::cpp_clamp;
use crate::fallible::Res;
use crate::lattice::accumulator::Accumulator;
use crate::lattice::basis::{Basis, Cell};
use crate::lattice::region::Region;

#[derive(Clone, Debug, Default, PartialEq)]
pub struct Model {
    pub basis: Basis,
    pub rms_px: f64,
    pub tolerance_px: f64,
    pub coverage: f64,
    pub consistency: f64,
    pub confidence: f64,
    pub suggest_check: bool,
    pub min_i: i32,
    pub max_i: i32,
    pub min_j: i32,
    pub max_j: i32,
    pub inliers: Vec<usize>,
    pub cells: Vec<Cell>,
    pub equations: Accumulator,
}

impl Model {
    /// `cellCount()`：索引區域裡的格子數（R07.2 起不是外接矩形，見 `region.rs`；佔用表配置失敗時回錯誤）。
    pub fn cell_count(&self) -> Res<usize> {
        Ok(self.region()?.cell_count(&self.basis))
    }

    pub fn region(&self) -> Res<Region> {
        Region::of(&self.basis, self.min_i, self.max_i, self.min_j, self.max_j, &self.cells)
    }
}

/// `NoLatticeReason`（數值和 C++ 的列舉順序相同）。
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
#[repr(i32)]
pub enum Reason {
    #[default]
    None = 0,
    TooFewPoints = 1,
    NoDirection = 2,
    NoConsistentLattice = 3,
    TooManyHoles = 4,
    LowConfidence = 5,
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct Fit {
    pub models: Vec<Model>,
    pub reason: Reason,
    pub rejected_confidence: f64,
}

/// `assessLattice` 的信心（證據 × 精度 × 涵蓋率 × 一致性）。`exp` 經 Rust 的標準庫呼叫 CRT 的 `exp`，和 MSVC 相同。
pub fn assess_confidence(inliers: usize, rms_px: f64, tolerance_px: f64, coverage: f64, consistency: f64, two_dimensional: bool) -> f64 {
    let extra = inliers as f64 - if two_dimensional { 2.0 } else { 1.0 };
    let support = if extra > 0.0 { 1.0 - (-extra / 3.0).exp() } else { 0.0 };
    let ratio = if tolerance_px > 0.0 { rms_px / tolerance_px } else { 1.0 };
    let precision = cpp_clamp((0.6 - ratio) / 0.3, 0.0, 1.0);
    let coverage = cpp_clamp(coverage, 0.0, 1.0);
    let consistency = cpp_clamp(consistency, 0.0, 1.0);
    support * precision * coverage * consistency
}
