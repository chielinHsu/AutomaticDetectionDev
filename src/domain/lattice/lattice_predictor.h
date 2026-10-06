#pragma once

#include <cstddef>
#include <limits>
#include <vector>

#include "domain/geometry.h"
#include "domain/lattice/lattice_model.h"

// domain 層：只用 C++ 標準庫。
//
// 由晶格預測格點：哪些格子、在哪裡、什麼順序。只做幾何，不看影像、不搜尋（搜尋是 use case 與 port 的事）。
namespace aoi {

struct PredictedCell {
    LatticeIndex cell;
    Point2D position;  // 影像像素
};

class LatticePredictor {
public:
    // bounds：只預測中心落在這裡面的格點（通常是影像範圍；Rect 的右、下邊是「不含」）。
    LatticePredictor(const LatticeModel& model, const Rect& bounds) noexcept : model_(model), bounds_(bounds) {}

    // 索引範圍內（每邊往外多 margin 格）的格點。順序：依 i，再依 j。最多 maxCount 個。
    std::vector<PredictedCell> cellsInRange(int margin = 0, std::size_t maxCount = kUnlimited) const;

    // 索引範圍內、沒有內點佔據的格點（缺格）。順序：依 i，再依 j。最多 maxCount 個。
    // 不往索引範圍外推算：整排缺在陣列邊上時，分不出「缺件」還是「陣列到此為止」。
    std::vector<PredictedCell> holes(std::size_t maxCount = kUnlimited) const;

    // bounds 內所有的格點，不限索引範圍（晶格延伸到整張影像）。最多 maxCount 個。
    std::vector<PredictedCell> allInBounds(std::size_t maxCount) const;

    // 由近到遠：依與 anchor 的距離排序；同距離時依 i、再依 j（結果與輸入順序無關）。漸進驗證用這個順序：
    // 先驗證離已知點近的（預測誤差小），每找到一個就重新擬合（LatticeAccumulator，O(1)），再往外。
    static void orderNearToFar(std::vector<PredictedCell>& cells, Point2D anchor);

    static constexpr std::size_t kUnlimited = std::numeric_limits<std::size_t>::max();

private:
    const LatticeModel& model_;
    Rect bounds_;
};

}  // namespace aoi
