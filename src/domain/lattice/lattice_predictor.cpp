#include "domain/lattice/lattice_predictor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>

#include "domain/lattice/flat_map.h"

namespace aoi {

namespace {

// 一次預測最多走訪這麼多個格子（防呆：不合理的模型不會讓呼叫端卡住）。
constexpr double kMaxVisited = 16.0 * 1024.0 * 1024.0;

}  // namespace

std::vector<PredictedCell> LatticePredictor::cellsInRange(int margin, std::size_t maxCount) const {
    std::vector<PredictedCell> out;
    const LatticeBasis& b = model_.basis;
    const int iFrom = model_.minI - margin;
    const int iTo = model_.maxI + margin;
    const int jFrom = b.hasV ? model_.minJ - margin : 0;
    const int jTo = b.hasV ? model_.maxJ + margin : 0;
    if (iTo < iFrom || jTo < jFrom || maxCount == 0) return out;
    if ((static_cast<double>(iTo) - iFrom + 1.0) * (static_cast<double>(jTo) - jFrom + 1.0) > kMaxVisited) return out;
    for (int i = iFrom; i <= iTo; ++i) {
        for (int j = jFrom; j <= jTo; ++j) {
            const Point2D p = b.at(i, j);
            if (!bounds_.contains(p)) continue;
            out.push_back({{i, j}, p});
            if (out.size() >= maxCount) return out;
        }
    }
    return out;
}

std::vector<PredictedCell> LatticePredictor::holes(std::size_t maxCount) const {
    std::vector<PredictedCell> out;
    const LatticeBasis& b = model_.basis;
    const int jFrom = b.hasV ? model_.minJ : 0;
    const int jTo = b.hasV ? model_.maxJ : 0;
    const double w = static_cast<double>(model_.maxI) - model_.minI + 1.0;
    const double h = static_cast<double>(jTo) - jFrom + 1.0;
    if (w < 1.0 || h < 1.0 || maxCount == 0 || w * h > kMaxVisited) return out;

    // 佔用表：索引範圍通常不大（信心夠的晶格涵蓋率不低），用密集陣列；很大時用雜湊表。
    const bool dense = w * h <= 64.0 * static_cast<double>(model_.cells.size()) + 4096.0;
    std::vector<std::uint8_t> grid;
    detail::FlatMap<std::uint8_t> sparse(dense ? 1 : model_.cells.size());
    const auto slot = [&](int i, int j) {
        return static_cast<std::size_t>(j - jFrom) * static_cast<std::size_t>(w) + static_cast<std::size_t>(i - model_.minI);
    };
    if (dense) grid.assign(static_cast<std::size_t>(w * h), 0);
    for (const LatticeIndex& c : model_.cells) {
        if (c.i < model_.minI || c.i > model_.maxI || c.j < jFrom || c.j > jTo) continue;
        if (dense) grid[slot(c.i, c.j)] = 1;
        else sparse[detail::packKey(c.i, c.j)] = 1;
    }
    for (int i = model_.minI; i <= model_.maxI; ++i) {
        for (int j = jFrom; j <= jTo; ++j) {
            const bool occupied = dense ? grid[slot(i, j)] != 0 : sparse.find(detail::packKey(i, j)) != nullptr;
            if (occupied) continue;
            const Point2D p = b.at(i, j);
            if (!bounds_.contains(p)) continue;
            out.push_back({{i, j}, p});
            if (out.size() >= maxCount) return out;
        }
    }
    return out;
}

std::vector<PredictedCell> LatticePredictor::allInBounds(std::size_t maxCount) const {
    std::vector<PredictedCell> out;
    const LatticeBasis& b = model_.basis;
    if (bounds_.empty() || maxCount == 0) return out;
    // bounds 的四個角換成格子座標，取外接的索引範圍，再逐一檢查中心是否落在 bounds 內。
    const Point2D corners[4] = {{bounds_.x, bounds_.y}, {bounds_.right(), bounds_.y}, {bounds_.x, bounds_.bottom()},
                                {bounds_.right(), bounds_.bottom()}};
    double aMin = 0.0, aMax = 0.0, bMin = 0.0, bMax = 0.0;
    for (int k = 0; k < 4; ++k) {
        const std::optional<Point2D> c = b.coordinates(corners[k]);
        if (!c) return out;
        aMin = k == 0 ? c->x : std::min(aMin, c->x);
        aMax = k == 0 ? c->x : std::max(aMax, c->x);
        bMin = k == 0 ? c->y : std::min(bMin, c->y);
        bMax = k == 0 ? c->y : std::max(bMax, c->y);
    }
    const double iFrom = std::floor(aMin);
    const double iTo = std::ceil(aMax);
    const double jFrom = b.hasV ? std::floor(bMin) : 0.0;
    const double jTo = b.hasV ? std::ceil(bMax) : 0.0;
    if ((iTo - iFrom + 1.0) * (jTo - jFrom + 1.0) > kMaxVisited) return out;
    const double limit = LatticeBasis::kMaxIndex;
    if (!(std::abs(iFrom) < limit && std::abs(iTo) < limit && std::abs(jFrom) < limit && std::abs(jTo) < limit)) return out;
    for (int i = static_cast<int>(iFrom); i <= static_cast<int>(iTo); ++i) {
        for (int j = static_cast<int>(jFrom); j <= static_cast<int>(jTo); ++j) {
            const Point2D p = b.at(i, j);
            if (!bounds_.contains(p)) continue;
            out.push_back({{i, j}, p});
            if (out.size() >= maxCount) return out;
        }
    }
    return out;
}

void LatticePredictor::orderNearToFar(std::vector<PredictedCell>& cells, Point2D anchor) {
    std::vector<double> d2(cells.size());
    for (std::size_t k = 0; k < cells.size(); ++k) {
        const double dx = cells[k].position.x - anchor.x;
        const double dy = cells[k].position.y - anchor.y;
        d2[k] = dx * dx + dy * dy;
    }
    std::vector<std::size_t> order(cells.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (d2[a] != d2[b]) return d2[a] < d2[b];
        if (cells[a].cell.i != cells[b].cell.i) return cells[a].cell.i < cells[b].cell.i;
        return cells[a].cell.j < cells[b].cell.j;
    });
    std::vector<PredictedCell> sorted;
    sorted.reserve(cells.size());
    for (const std::size_t k : order) sorted.push_back(cells[k]);
    cells = std::move(sorted);
}

}  // namespace aoi
