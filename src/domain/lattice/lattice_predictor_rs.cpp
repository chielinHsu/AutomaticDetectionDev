// AOI_DOMAIN_IMPL=rust：轉呼叫 Rust 的實作（rust/aoi_domain_rs，C ABI 在 domain/ffi/aoi_domain_rs.h；ADR-031）。
#include "domain/lattice/lattice_predictor.h"

#include <memory>

#include "domain/lattice/lattice_rs_bridge.h"

namespace aoi {

namespace {

// 預測只用到基底、索引範圍與內點的格子；ModelView 也複製內點（多 8 byte／點，和走訪的格子數比很小）。
std::vector<PredictedCell> predict(const LatticeModel& model, const Rect& bounds, int32_t mode, int margin, std::size_t maxCount) {
    rs::checkAbi();
    const rs::ModelView view(model);
    AoiRsPredictedList* handle = nullptr;
    rs::check(aoi_rs_lattice_predict(view.get(), rs::toRs(bounds), mode, margin, maxCount, &handle), "aoi_rs_lattice_predict");
    const std::unique_ptr<AoiRsPredictedList, void (*)(AoiRsPredictedList*)> owner(handle, &aoi_rs_predicted_list_free);
    const AoiRsPredictedCell* data = nullptr;
    std::size_t count = 0;
    rs::check(aoi_rs_predicted_list_get(handle, &data, &count), "aoi_rs_predicted_list_get");
    std::vector<PredictedCell> out;
    out.reserve(count);
    for (std::size_t k = 0; k < count; ++k) out.push_back({rs::fromRs(data[k].cell), rs::fromRs(data[k].position)});
    return out;
}

}  // namespace

std::vector<PredictedCell> LatticePredictor::cellsInRange(int margin, std::size_t maxCount) const {
    return predict(model_, bounds_, AOI_RS_PREDICT_IN_RANGE, margin, maxCount);
}

std::vector<PredictedCell> LatticePredictor::holes(std::size_t maxCount) const {
    return predict(model_, bounds_, AOI_RS_PREDICT_HOLES, 0, maxCount);
}

std::vector<PredictedCell> LatticePredictor::allInBounds(std::size_t maxCount) const {
    return predict(model_, bounds_, AOI_RS_PREDICT_ALL_IN_BOUNDS, 0, maxCount);
}

void LatticePredictor::orderNearToFar(std::vector<PredictedCell>& cells, Point2D anchor) {
    rs::checkAbi();
    std::vector<AoiRsPredictedCell> in;
    in.reserve(cells.size());
    for (const PredictedCell& c : cells) in.push_back({rs::toRs(c.cell), rs::toRs(c.position)});
    rs::check(aoi_rs_lattice_order_near_to_far(in.data(), in.size(), rs::toRs(anchor)), "aoi_rs_lattice_order_near_to_far");
    for (std::size_t k = 0; k < cells.size(); ++k) cells[k] = {rs::fromRs(in[k].cell), rs::fromRs(in[k].position)};
}

}  // namespace aoi
