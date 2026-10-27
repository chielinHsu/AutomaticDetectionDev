#pragma once

#include <cstddef>
#include <vector>

#include "domain/lattice/lattice_basis.h"

// domain 層：只用 C++ 標準庫。
//
// 格子連通（detail::largestComponent）的 C++ 實作（S05 起的那一份，R05.2 從 cell_components.cpp 原樣搬過來，ADR-031）。
// 用在哪裡、不要改行為的規則見 lattice_accumulator_legacy.h。說明在 cell_components.h。
namespace aoi::legacy {

std::vector<std::size_t> largestComponent(const std::vector<LatticeIndex>& cells, int link, bool twoDimensional);

}  // namespace aoi::legacy
