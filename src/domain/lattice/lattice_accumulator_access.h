#pragma once

#include "domain/lattice/lattice_accumulator.h"

// domain 層：只用 C++ 標準庫。晶格模組內部用（R05.2，ADR-031），不是公開的型別。
//
// LatticeAccumulator 的 friend：讀出與寫回私有的狀態（detail::LatticeSums，定義在 lattice_accumulator.h），公開的介面不變。
// 用途：C++ 的估計器（aoi::legacy）把結果放進公開的 LatticeModel::equations；Rust 的估計結果換回 LatticeModel；
// 兩種實作的公開類別直接對 sums_ 操作（不複製）。
namespace aoi::detail {

struct LatticeAccumulatorAccess {
    static const LatticeSums& get(const LatticeAccumulator& a) noexcept { return a.sums_; }
    static LatticeSums& state(LatticeAccumulator& a) noexcept { return a.sums_; }
    static void set(LatticeAccumulator& a, const LatticeSums& s) noexcept { a.sums_ = s; }

    static LatticeAccumulator make(const LatticeSums& s) noexcept {
        LatticeAccumulator a;
        a.sums_ = s;
        return a;
    }
};

}  // namespace aoi::detail
