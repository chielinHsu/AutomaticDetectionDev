#pragma once

#include <cstddef>
#include <vector>

#include "domain/geometry.h"
#include "domain/lattice/lattice_model.h"
#include "domain/lattice/lattice_predictor.h"

// domain 層：只用 C++ 標準庫。
//
// 格點預測的 C++ 實作（S05 起的那一份，R05.2 從 lattice_predictor.cpp 原樣搬過來，ADR-031）。用在哪裡、不要改行為的規則
// 見 lattice_accumulator_legacy.h。說明在 lattice_predictor.h；輸出用公開的 PredictedCell。
namespace aoi::legacy {

class LatticePredictor {
public:
    LatticePredictor(const LatticeModel& model, const Rect& bounds) noexcept : model_(model), bounds_(bounds) {}

    std::vector<PredictedCell> cellsInRange(int margin, std::size_t maxCount) const;
    std::vector<PredictedCell> holes(std::size_t maxCount) const;
    std::vector<PredictedCell> allInBounds(std::size_t maxCount) const;
    static void orderNearToFar(std::vector<PredictedCell>& cells, Point2D anchor);

private:
    const LatticeModel& model_;
    Rect bounds_;
};

}  // namespace aoi::legacy
