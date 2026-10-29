#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "domain/lattice/lattice_basis.h"
#include "domain/lattice/lattice_detail.h"

// domain 層：只用 C++ 標準庫。晶格的索引區域：哪些格子算「陣列的範圍之內」。缺格數（ADR-013 規則 2）、涵蓋率與
// 缺格的預測（規則 6「不往外推」）都用它。兩種建置都編這一份（AOI_DOMAIN_IMPL=cpp 的實作直接用；rust 建置裡
// LatticeModel::cellCount() 與差異測試用，Rust 那一側是 lattice/region.rs，運算順序相同）。
//
// R07.2（2026-10-28）之前，範圍是內點索引的外接矩形（minI..maxI × minJ..maxJ）。兩種真實的板子它都不對：
//  - 交錯排列的陣列（相鄰兩欄差半個格距，例如密排端子：u ＝ (0, 154)、v ＝ (133, 77)）在影像上是矩形，在索引空間
//    卻是平行四邊形，外接矩形多出兩個三角形的空格，規則 2 把二維晶格擋掉、退回一維，缺格一個都不補。
//  - 陣列中間有大片空的區域（兩個長槽）時，空的區域在外接矩形裡，整片都被當成缺格（使用者看到幾百個假的推算框）。
//
// 現在二維的區域是三個條件的交集（一維沒有變：只有 i 的範圍）：
//  1. 索引的外接矩形（和之前一樣）。
//  2. 影像空間裡沿 u 的外接矩形：格子中心投影到 û 與它的法向量上，要落在所有內點格子中心投影的範圍之內（容差四分之一個
//     最短格距：有雜訊時擬合出來的 v 帶一點 u 方向的分量，矩形陣列缺角的格子會多出幾個像素）。交錯陣列的區塊沿 u 對齊時
//     被精確描述；沒有對齊的斜切陣列仍是外接矩形。
//  3. 空格要有兩側的證據（規則 4「空 2 格以上就拆開」用在缺格上）：沿 u 或沿 w 的兩側、各在 3 格之內都有內點；或者是角落
//     （沿 u 與沿 w 各有一個相鄰的內點）。w 是和 u 最接近垂直的格距向量（v、2v − u、2v + u 之中取一個）：交錯陣列的 v 是
//     斜的，用它判斷「兩側」會從空的區域漏進陣列；w 是同一列的下一個元件。大片空的區域裡的格子沒有兩側的證據，不算。
//     代價：角落的缺件要兩個相鄰的元件都找到才補（缺角而且相鄰也缺的，不補）。
namespace aoi::detail {

class LatticeRegion {
public:
    // cells：內點的格子；minI..maxJ：呼叫端已經算好的索引範圍（模型的欄位）。
    LatticeRegion(const LatticeBasis& b, int minI, int maxI, int minJ, int maxJ, const std::vector<LatticeIndex>& cells);

    // 格子 (i, j) 在區域裡？內點的格子一定在；空格要在影像空間的範圍內、而且有兩側（或角落）的證據。
    bool contains(int i, int j) const;
    // 外接矩形的格子數（之前 cellCount() 的定義；size_t 的乘法，溢位時繞回）。
    std::size_t boxCount() const noexcept;
    // 區域裡的格子數（內點的格子加上有證據的空格）。不是沿 u 對齊的、或外接矩形大得不合理時，就是外接矩形的格子數。
    std::size_t cellCount() const;
    // 規則 2 用的格子數：內點的格子 ＋ 區域裡有證據的空格 ＋ 區域裡貼著內點（八鄰）的空格。比 cellCount 多算貼著內點的
    // 空格，規則 2 才擋得住稀疏的假晶格（散佈的點湊出的晶格每個內點旁邊都是空格）；大片空的區域只算貼邊那一圈。
    std::size_t evidenceCount() const;

    int minI() const noexcept { return minI_; }
    int maxI() const noexcept { return maxI_; }
    int minJ() const noexcept { return minJ_; }
    int maxJ() const noexcept { return maxJ_; }
    // 沿 w 走一格的索引位移（測試用）。
    LatticeIndex wStep() const noexcept { return {wi_, wj_}; }

    // 空格兩側的內點最遠在幾格之內（＝ LatticeOptions::maxGapCells 的預設 2 ＋ 1）。
    static constexpr int kSupportReach = 3;

private:
    bool occupied(int i, int j) const;
    bool inExtent(int i, int j) const;
    bool adjacentAny(int i, int j) const;
    bool reaches(int i, int j, int di, int dj) const;
    bool between(int i, int j, int di, int dj) const { return reaches(i, j, di, dj) && reaches(i, j, -di, -dj); }
    bool adjacent(int i, int j, int di, int dj) const { return occupied(i + di, j + dj) || occupied(i - di, j - dj); }

    LatticeBasis b_;
    int minI_, maxI_, minJ_, maxJ_;
    bool aligned_ = false;  // 有沒有影像空間的範圍與佔用表（二維、u 不退化、至少一個格子）；沒有就只是外接矩形
    double ux_ = 0.0, uy_ = 0.0;
    double sMin_ = 0.0, sMax_ = 0.0, tMin_ = 0.0, tMax_ = 0.0;
    double eps_ = 0.0;
    int wi_ = 0, wj_ = 1;
    std::optional<CellTable> occupied_;
};

}  // namespace aoi::detail
