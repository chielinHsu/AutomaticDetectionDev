//! domain 層的演算法（ADR-031）。只用標準庫（`[dependencies]` 必須是空的）。
//!
//! C++ 的標頭（`src/domain/`）不變：C++ 的 .cpp 是薄的包裝，經 C ABI（`ffi`、`ffi_lattice` 模組，標頭在
//! `src/domain/ffi/aoi_domain_rs.h`）呼叫這裡。數值的規則和 C++ 的實作（`aoi::legacy`）逐步相同，
//! 由 `tests/domain/tst_domain_rs_diff` 逐筆比對。
//!
//! R05.1：非極大值抑制（重疊面積、重疊比例、空間索引、`keep`）。
//! R05.2：晶格（累加器、空間格子、分群、基底、格子分量、接受規則、估計、預測）。
//! R05.3：比對區的換算（`match_region`）與自動擷取（`feature_extract`：Otsu、最近的亮塊、外擴）。

pub mod cpp;
pub mod fallible;
pub mod feature_extract;
pub mod ffi;
pub mod ffi_lattice;
pub mod ffi_match;
pub mod flat_map;
pub mod geometry;
pub mod lattice;
pub mod match_region;
pub mod nms;
