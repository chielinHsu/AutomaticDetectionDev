#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "domain/geometry.h"
#include "domain/lattice/flat_map.h"
#include "domain/lattice/non_max_suppression.h"

// domain 層：只用 C++ 標準庫。
//
// 非極大值抑制的 C++ 實作（S05 起的那一份，R05.1 從 non_max_suppression.cpp 原樣搬過來，ADR-031）。
// 用在兩個地方：
//   - AOI_DOMAIN_IMPL=cpp 的建置：aoi_domain 編進它，non_max_suppression.cpp 轉呼叫這裡；
//   - AOI_DOMAIN_IMPL=rust 的建置：只有差異測試用的 aoi_domain_legacy 程式庫編進它（tests/domain/tst_domain_rs_diff 拿它當對照組）。
// 演算法與規則的說明在 non_max_suppression.h；這裡不要改行為（要改，Rust 那一份與差異測試一起改）。
namespace aoi::legacy {

double overlapArea(const RectObj& a, const RectObj& b) noexcept;
double overlapRatio(const RectObj& a, const RectObj& b, OverlapMeasure measure) noexcept;

class OverlapIndex {
public:
    OverlapIndex(double cellSize, double maxOverlap, OverlapMeasure measure);

    bool overlapsAny(const RectObj& box) const;
    void add(const RectObj& box);
    std::size_t size() const noexcept { return boxes_.size(); }

private:
    int cellOf(double v) const noexcept;

    double cell_;
    double maxOverlap_;
    OverlapMeasure measure_;
    double maxHalfDiagonal_ = 0.0;
    std::vector<RectObj> boxes_;
    std::vector<int> next_;          // 同一格的下一個框（−1 結束）
    detail::FlatMap<int> head_{16};  // 格子 → 第一個框的索引＋1（0 ＝ 空）
};

// NonMaxSuppression::keep 的實作。
std::vector<std::size_t> keep(std::span<const ScoredBox> boxes, const NmsOptions& options);

}  // namespace aoi::legacy
