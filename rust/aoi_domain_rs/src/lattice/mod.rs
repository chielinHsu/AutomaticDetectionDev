//! 晶格估計與補漏（R05.2）：`src/domain/lattice/` 的 C++ 實作逐檔對應，運算順序相同（差異測試逐筆比對）。

// 刻意的寫法（不是疏忽）：
// - `!(a > b)`：和 C++ 一樣讓 NaN 走「否」的那一邊（`partial_cmp` 的寫法反而看不出和 C++ 的對應）。
// - 用索引走訪：照 C++ 的迴圈寫，逐行對照比較容易（差異測試要求結果一位不差）。
#![allow(clippy::neg_cmp_op_on_partial_ord, clippy::needless_range_loop)]

pub mod acceptance;
pub mod accumulator;
pub mod basis;
pub mod basis_finder;
pub mod cell_components;
pub mod clustering;
pub mod detail;
pub mod estimator;
pub mod model;
pub mod options;
pub mod predictor;
pub mod spatial_grid;
