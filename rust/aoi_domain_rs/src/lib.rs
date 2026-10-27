//! domain 層的演算法（ADR-031）。只用標準庫（`[dependencies]` 必須是空的）。
//!
//! C++ 的標頭（`src/domain/`）不變：C++ 的 .cpp 是薄的包裝，經 C ABI（`ffi` 模組，標頭在
//! `src/domain/ffi/aoi_domain_rs.h`）呼叫這裡。數值的規則和 C++ 的實作（`aoi::legacy`）逐步相同，
//! 由 `tests/domain/tst_domain_rs_diff` 逐筆比對。
//!
//! R05.1：非極大值抑制（重疊面積、重疊比例、空間索引、`keep`）。

pub mod ffi;
pub mod flat_map;
pub mod geometry;
pub mod nms;
