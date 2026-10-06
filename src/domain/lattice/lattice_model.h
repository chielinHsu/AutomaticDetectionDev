#pragma once

#include <cstddef>
#include <vector>

#include "domain/geometry.h"
#include "domain/lattice/lattice_accumulator.h"
#include "domain/lattice/lattice_basis.h"

// domain 層：只用 C++ 標準庫。LatticeEstimator 的輸出。座標是影像像素（geometry.h）。
namespace aoi {

// 擬合出來的一組晶格：基底、品質與成員。
struct LatticeModel {
    LatticeBasis basis;
    double rmsPx = 0.0;        // 內點到格點的均方根距離
    double tolerancePx = 0.0;  // 內點的容差（殘差 ≤ 這個值才算內點）
    double coverage = 0.0;     // 內點數 ÷ 索引範圍內的格子數（0..1）；缺件越多越低
    double consistency = 0.0;  // 陣列範圍內的點有多少落在格點上（0..1）；範圍內有很多不在格點上的點時低
    double confidence = 0.0;   // 0..1，算法見 lattice_confidence.h
    bool suggestCheck = false; // 信心低於 LatticeOptions::checkBelow：結果要人看（UI 的「建議檢查」）
    // 內點的索引範圍（正規化過：minI ＝ minJ ＝ 0；一維時 j 恆為 0）。
    int minI = 0, maxI = 0, minJ = 0, maxJ = 0;
    // 內點：輸入點的索引（由小到大）與它佔的格子（一一對應）。每個格子最多一個內點。
    std::vector<std::size_t> inliers;
    std::vector<LatticeIndex> cells;
    // 內點的正規方程（參考點 ＝ origin、參考格子 ＝ (0, 0)）：之後加點重新擬合是 O(1)（漸進驗證、use case 的修正）。
    LatticeAccumulator equations;

    bool hasV() const noexcept { return basis.hasV; }
    // 索引範圍內的格子數（一維時是 maxI − minI ＋ 1）。
    std::size_t cellCount() const noexcept {
        const auto w = static_cast<std::size_t>(maxI - minI + 1);
        const auto h = basis.hasV ? static_cast<std::size_t>(maxJ - minJ + 1) : std::size_t{1};
        return w * h;
    }
};

// 為什麼沒有晶格（LatticeFit::models 是空的時候）。spec.md 的「退化情況決策樹」。
enum class NoLatticeReason {
    None,                 // 有晶格
    TooFewPoints,         // 有限的點少於 2 個
    NoDirection,          // 所有點重疊：沒有任何位移向量
    NoConsistentLattice,  // 找不到夠多的點（minInliers）符合同一組晶格
    TooManyHoles,         // 有晶格，但要補的缺格比證據多（缺格數 ≥ 內點數 − 定義晶格最少要的點數）
    LowConfidence,        // 有晶格，但信心低於 LatticeOptions::minConfidence（呼叫端退回「只有找到的」）
};

struct LatticeFit {
    std::vector<LatticeModel> models;  // 第一個是主晶格（內點最多）；之後是其餘的點擬合出的其他晶格
    NoLatticeReason reason = NoLatticeReason::None;  // models 是空的時候才有意義
    double rejectedConfidence = 0.0;  // reason ＝ LowConfidence 時，被擋掉的那組晶格的信心
};

}  // namespace aoi
