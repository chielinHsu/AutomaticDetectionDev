#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include "domain/geometry.h"
#include "domain/lattice/lattice_model.h"
#include "domain/lattice/vector_clustering.h"

// domain 層：只用 C++ 標準庫。
//
// 晶格估計：從一組中心點（搜尋器找到的元件）找出它們排成的陣列。流程（docs/steps/S05-lattice/diagram.md）：
//   1. u：每個點到最近幾個鄰居的位移向量 → 分群（策略，預設網格投票）→ 票數夠多的群裡最短的那個。
//   2. v：沿 u 把點分成「列」（垂直 u 的座標相近的點），相鄰兩列之間的位移向量對 u 取餘 → 分群 →
//      「指向不同點最多」的群裡列距最小的那個。只找到一列就是一維（一排）。
//   3. 原點：RANSAC。隨機挑一個點當原點，數它附近有多少點落在格點上；最好的當起點。
//   4. 精修：由原點附近開始，每次把範圍加倍：指派最近的格子 → 最小平方法（每個軸 3 個參數，和 S00 示範頁相同）。
//      這樣基底的小誤差不會在陣列遠端累積成整格的錯位。
//   5. 連通：格子之間的空隙超過 maxGapCells 就拆開，只留最大的一塊（兩個陣列不會被當成一個，中間不會補出一片假的缺格）。
//   6. 二維要成立：主列以外至少 minSecondRowInliers 個內點，而且那些內點大多不在「可疑的列」
//      （歸到這一列的點只有一小部分在格點上：別的間距或方向的元件碰巧落在格點附近）。否則改用一維。
//   7. 證據：至少 minInliers 個內點（或每個點都被解釋）；信心（lattice_confidence.h）低於 minConfidence 就不算。
//      第一個 u 不成功時，改用下一個 u 的候選（最多 3 個）。都不成功：回報沒有晶格。
// fitAll 再對剩下的點重複，找出其他的晶格（例如上下兩排方向不同）；結果依內點數排序，主晶格在第一個。
//
// 不丟例外；非有限值的點不參與。結果只由輸入決定（RANSAC 用固定種子的亂數），與執行緒、呼叫次數無關。
// 物件建好之後不可變，可以被多個執行緒同時呼叫。
namespace aoi {

// 預設值是起點，不是量測出來的事實；S07 用真實影像校準。
struct LatticeOptions {
    double inlierFraction = 0.25;           // 內點容差 ＝ 這個比例 × 最短的格距（|u| 與列距取小）
    std::size_t neighbors = 4;              // 找 u：每個點看最近幾個鄰居
    std::size_t maxSamples = 256;           // 找方向時最多取幾個點（等距抽樣；1 000 個向量已足夠）；指派與精修一律用全部的點
    double supportFraction = 0.5;           // 基底向量的候選：票數至少是最高票的這個比例（擋掉雜訊），之中取最短的
    int ransacTrials = 16;                  // 原點假設最多試幾次
    int ransacIndexRadius = 6;              // 原點假設的評分只數離原點這麼多格之內的內點（偏好陣列中央的原點）
    int maxGapCells = 2;                    // 同一組晶格裡容許連續缺幾格；更大的空隙把陣列拆開
    std::size_t minSecondRowInliers = 2;    // 二維晶格要成立：主列以外至少要有幾個內點
    double minRowConsistency = 0.6;         // 二維晶格：一列裡落在格點上的比例低於這個值就是可疑的列（lattice_acceptance.h）
    std::size_t minInliers = 3;             // 一組晶格至少幾個內點（例外：每個點都被這組晶格解釋，例如只有 2 個點）
    double minConfidence = 0.15;            // 信心低於這個值：當作沒有晶格
    double checkBelow = 0.6;                // 信心低於這個值：建議人工檢查（LatticeModel::suggestCheck）
    std::size_t maxLattices = 4;            // fitAll 最多找幾組
    std::size_t minExtraLatticeInliers = 3; // 第二組以後至少要有幾個內點
    std::uint64_t seed = 0x5EEDull;         // RANSAC 的亂數種子
};

class LatticeEstimator {
public:
    // clustering 是空指標時用 GridVoteClustering。
    explicit LatticeEstimator(LatticeOptions options = {}, std::shared_ptr<const IVectorClustering> clustering = {});

    // 主晶格（內點最多的那一組）；沒有晶格時回 nullopt。
    std::optional<LatticeModel> fit(std::span<const Point2D> points) const;

    // 所有晶格（最多 maxLattices 組）；沒有晶格時 models 是空的，reason 說明原因。
    LatticeFit fitAll(std::span<const Point2D> points) const;

    const LatticeOptions& options() const noexcept { return options_; }

private:
    LatticeFit run(std::span<const Point2D> points, std::size_t maxModels) const;

    LatticeOptions options_;
    std::shared_ptr<const IVectorClustering> clustering_;
};

}  // namespace aoi
