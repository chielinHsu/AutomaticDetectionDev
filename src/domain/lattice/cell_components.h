#pragma once

#include <cstddef>
#include <vector>

#include "domain/lattice/lattice_basis.h"

// domain 層：只用 C++ 標準庫。晶格模組內部用。
//
// 格子的連通：兩個格子的 Chebyshev 距離（max(|Δi|, |Δj|)）≤ link 就相連（中間最多空 link − 1 格）。
// 用途：兩個同樣間距的陣列不會被當成一個（中間不會補出一片假的缺格），遠處碰巧落在格點上的離群點也會被分開。
namespace aoi::detail {

// 成員最多的那一塊：回傳它在 cells 裡的索引（由小到大）。一樣大時取逐列掃描先遇到的那一塊（結果與 cells 的順序無關）。
// twoDimensional ＝ false 時只看同一列（j 相同）。空輸入回空。
std::vector<std::size_t> largestComponent(const std::vector<LatticeIndex>& cells, int link, bool twoDimensional);

}  // namespace aoi::detail
