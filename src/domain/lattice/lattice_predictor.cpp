// AOI_DOMAIN_IMPL=cpp：轉呼叫 C++ 的實作（aoi::legacy，lattice/legacy/lattice_predictor_legacy.cpp）。
// AOI_DOMAIN_IMPL=rust 時編的是 lattice_predictor_rs.cpp（ADR-031）。
#include "domain/lattice/lattice_predictor.h"

#include "domain/lattice/legacy/lattice_predictor_legacy.h"

namespace aoi {

std::vector<PredictedCell> LatticePredictor::cellsInRange(int margin, std::size_t maxCount) const {
    return legacy::LatticePredictor(model_, bounds_).cellsInRange(margin, maxCount);
}

std::vector<PredictedCell> LatticePredictor::holes(std::size_t maxCount) const {
    return legacy::LatticePredictor(model_, bounds_).holes(maxCount);
}

std::vector<PredictedCell> LatticePredictor::allInBounds(std::size_t maxCount) const {
    return legacy::LatticePredictor(model_, bounds_).allInBounds(maxCount);
}

void LatticePredictor::orderNearToFar(std::vector<PredictedCell>& cells, Point2D anchor) {
    legacy::LatticePredictor::orderNearToFar(cells, anchor);
}

}  // namespace aoi
