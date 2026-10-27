#pragma once

#include <span>

#include "domain/geometry.h"
#include "domain/lattice/lattice_detail.h"
#include "domain/lattice/lattice_estimator.h"
#include "domain/lattice/lattice_model.h"

// domain 層：只用 C++ 標準庫。
//
// 晶格接受規則（detail::twoDimensionalIsValid 等）的 C++ 實作（S05 起的那一份，R05.2 從 lattice_acceptance.cpp
// 原樣搬過來，ADR-031）。用在哪裡、不要改行為的規則見 lattice_accumulator_legacy.h。說明在 lattice_acceptance.h。
// 呼叫時要寫 legacy::（引數是 aoi::detail 的型別，ADL 也會找到 aoi::detail 的同名函式）。
namespace aoi::legacy {

bool twoDimensionalIsValid(const detail::LatticeCandidate& c, std::span<const Point2D> points, const LatticeOptions& o);
bool enoughEvidence(const detail::LatticeCandidate& c, std::span<const Point2D> points, const LatticeOptions& o);
bool holesAreBacked(const LatticeModel& m);
double consistency(const LatticeModel& m, std::span<const Point2D> points);

}  // namespace aoi::legacy
