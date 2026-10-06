#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "domain/geometry.h"

// domain 層：只用 C++ 標準庫。
//
// 晶格估計要從一堆「位移向量」（點到鄰居的向量）找出最常出現的幾個方向與長度，基底向量就從裡面挑。
// 「怎麼分群」是一個可以替換的策略（O：換方法時新增一個類別、注入 LatticeEstimator，不改估計器）。
namespace aoi {

struct VectorCluster {
    Point2D center;           // 成員的平均
    std::size_t members = 0;  // 成員數
};

struct VectorClusters {
    std::vector<VectorCluster> clusters;  // 依成員數由多到少；同數時短的在前（結果與輸入順序無關）
    std::vector<int> labels;              // 每個輸入向量屬於哪一群（clusters 的索引）；−1 ＝ 不屬於任何群
};

// 契約（每個實作都要遵守，tst_lattice 對兩個實作跑同一組情境）：
//  - 不丟例外；空輸入回空結果；不是有限值的向量標 −1。
//  - labels 的長度等於輸入的長度；clusters[k].members 等於 labels 裡 k 出現的次數；center 是那些向量的平均。
//  - 同樣的輸入永遠同樣的輸出。
class IVectorClustering {
public:
    virtual ~IVectorClustering() = default;
    // scale：典型的向量長度（像素），實作用它決定「多近算同一群」。
    virtual VectorClusters cluster(std::span<const Point2D> vectors, double scale) const = 0;
};

// 預設：網格投票。向量量化到邊長 binFraction × scale 的格子，每格計票；票數是局部最大的格子是「峰」，
// 峰與它周圍 3×3 格的向量是一群。O(m)（m ＝ 向量數），與群的數量無關。
// 解析度：相差超過約 2 格（0.2 × scale）的兩種向量會分成兩群；格子太大時，兩種相近的間距（例如 27 與 34）
// 會被併成一群，平均出一個不存在的向量（S05 用測試找到的）。雜訊大到一群跨好幾格時，一群會分成幾個鄰近的峰，
// 估計器仍然挑得到對的方向（之後的最小平方法會修正）。
class GridVoteClustering final : public IVectorClustering {
public:
    explicit GridVoteClustering(double binFraction = 0.1) noexcept : binFraction_(binFraction) {}
    VectorClusters cluster(std::span<const Point2D> vectors, double scale) const override;

private:
    double binFraction_;
};

// 替代：貪婪合併（S03 暫代版 SimpleGrid 的做法）。依長度由短到長，與現有群的中心相差在 tolerance × 中心長度之內就併入。
// O(m × 群數)：向量很亂（群很多）時會變慢；留著做對照與測試「策略可以替換」。
class GreedyClustering final : public IVectorClustering {
public:
    explicit GreedyClustering(double tolerance = 0.2) noexcept : tolerance_(tolerance) {}
    VectorClusters cluster(std::span<const Point2D> vectors, double scale) const override;

private:
    double tolerance_;
};

}  // namespace aoi
