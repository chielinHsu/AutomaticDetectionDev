#pragma once

#include <cstddef>
#include <span>

#include "domain/geometry.h"
#include "domain/lattice/lattice_estimator.h"
#include "domain/lattice/lattice_model.h"
#include "domain/lattice/vector_clustering.h"

// domain 層：只用 C++ 標準庫。
//
// 晶格估計的 C++ 實作（S05 起的那一份，R05.2 從 lattice_estimator.cpp 原樣搬過來，ADR-031）。用在哪裡、不要改行為的規則
// 見 lattice_accumulator_legacy.h。流程的說明在 lattice_estimator.h。
// 和原本的差別只有：LatticeEstimator::run 變成自由函式（選項與分群策略由參數給），內部用 legacy 的累加器、找基底、
// 連通與接受規則，結果的 LatticeModel::equations 由 legacy 的累加器換過去（detail::LatticeAccumulatorAccess）。
namespace aoi::legacy {

// LatticeEstimator::run：fit 是 maxModels ＝ 1，fitAll 是 max(maxLattices, 1)。
LatticeFit fitLattices(const LatticeOptions& options, const IVectorClustering& clustering, std::span<const Point2D> points,
                       std::size_t maxModels);

}  // namespace aoi::legacy
