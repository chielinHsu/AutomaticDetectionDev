#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "domain/geometry.h"

// domain 層：只用 C++ 標準庫。
//
// 非極大值抑制（NMS）：同一個元件被找到好幾次（位置差幾個像素的幾個匹配）時，只留分數最高的那一個。
// 重疊用「旋轉矩形」精確計算（凸多邊形裁切），不是外接矩形的近似：斜放的密集元件，外接矩形會重疊、實際上不重疊。
// 鄰居用空間雜湊找：每個框只和附近格子裡已留下的框比，排序之後是 O(n)。
//
// 實作有兩份（ADR-031，qmake 的 AOI_DOMAIN_IMPL）：cpp ＝ lattice/legacy/non_max_suppression_legacy.cpp（aoi::legacy），
// rust ＝ rust/aoi_domain_rs（經 ffi/aoi_domain_rs.h）。本標頭兩種建置都一樣；差異由 tests/domain/tst_domain_rs_diff 逐筆比對。
namespace aoi {

enum class OverlapMeasure {
    IntersectionOverUnion,    // 交集 ÷ 聯集（IoU）
    IntersectionOverSmaller,  // 交集 ÷ 兩者中較小的面積：小框整個落在大框裡也算完全重疊
};

// 兩個旋轉矩形（RectObj：中心、未旋轉的寬高、角度）的重疊面積。寬或高 ≤ 0 的矩形面積是 0。
double overlapArea(const RectObj& a, const RectObj& b) noexcept;
// 重疊比例（0..1）。任一個面積是 0 時回 0。
double overlapRatio(const RectObj& a, const RectObj& b, OverlapMeasure measure) noexcept;

// 已留下的框的空間索引：「這個框和已留下的哪一個重疊超過容差？」只看附近的格子。
class OverlapIndex {
public:
    // cellSize：格子邊長，用框的對角線長度最合適（太小也正確，只是看的格子多）。
    OverlapIndex(double cellSize, double maxOverlap, OverlapMeasure measure);
    ~OverlapIndex();
    // 不可複製、可以移動（實作可能是 Rust 的 handle）。移動之後的物件只能解構或被指定：查詢回 false、size() 回 0、add 不做事。
    OverlapIndex(OverlapIndex&&) noexcept;
    OverlapIndex& operator=(OverlapIndex&&) noexcept;
    OverlapIndex(const OverlapIndex&) = delete;
    OverlapIndex& operator=(const OverlapIndex&) = delete;

    // 和已加入的任何一個框的重疊比例 > maxOverlap？
    bool overlapsAny(const RectObj& box) const;
    // 位姿或寬高不是有限值的框不加。配置失敗時丟 std::bad_alloc。
    void add(const RectObj& box);
    std::size_t size() const noexcept;

private:
    struct Impl;  // cpp：aoi::legacy::OverlapIndex；rust：Rust 物件的 handle
    std::unique_ptr<Impl> impl_;
};

struct ScoredBox {
    RectObj rect;
    double score = 0.0;
};

struct NmsOptions {
    double maxOverlap = 0.5;  // 容差：重疊比例超過這個值，分數較低的被壓掉
    OverlapMeasure measure = OverlapMeasure::IntersectionOverUnion;
};

class NonMaxSuppression {
public:
    explicit NonMaxSuppression(NmsOptions options = {}) noexcept : options_(options) {}

    // 留下的框（輸入的索引），依分數由高到低；同分時依 y、再依 x、再依索引（和 IPatternSearcher 的排序規則相同）。
    // 分數或位姿不是有限值的框直接丟掉。O(n log n) 排序 ＋ O(n) 比對。
    std::vector<std::size_t> keep(std::span<const ScoredBox> boxes) const;

    const NmsOptions& options() const noexcept { return options_; }

private:
    NmsOptions options_;
};

}  // namespace aoi
