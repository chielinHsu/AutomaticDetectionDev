#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "domain/geometry.h"

// application 層。
//
// **S03 的暫代品**：用最簡單的做法從一組中心點擬合二維晶格（兩個基底向量 u、v），並列出晶格內缺的格子。
// 真正的晶格估計（群聚投票、RANSAC、漸進重新擬合、多組晶格、信心分數）在 S05 的 domain/lattice。
// S05 取代這兩個函式的實作，use case 不必改。
//
// 已知限制（簡化版，沒有量測過魯棒性）：
//  - 只看每個點最近的 6 個鄰居來找基底向量：兩個方向的間距比超過約 3 倍時，找不到較遠的那個方向，會當成一維。
//  - 只補「已找到的點的索引範圍之內」的缺格，不往外推算。
//  - 單次擬合，沒有離群點的重複剔除。
//  - 估計基底向量時最多取 800 個點（等距抽樣），其餘仍會被指派索引。
namespace aoi {

struct GridCell {
    int i = 0;
    int j = 0;
};

struct GridModel {
    Point2D origin;
    Point2D u;  // 第一個基底向量
    Point2D v;  // 第二個基底向量；hasV ＝ false（一維陣列）時沒有意義
    bool hasV = false;
    int minI = 0, maxI = 0, minJ = 0, maxJ = 0;  // 內點的索引範圍
    double rmsPx = 0.0;                          // 內點到擬合位置的均方根距離
    std::vector<GridCell> occupied;              // 內點佔據的格子

    Point2D at(int i, int j) const noexcept {
        return {origin.x + i * u.x + j * v.x, origin.y + i * u.y + j * v.y};
    }
};

// 擬合。少於 2 個點、找不到基底向量，或內點少於 2 個時回傳 nullopt（呼叫端退回「只有找到的」）。
std::optional<GridModel> fitGrid(const std::vector<Point2D>& points);

// 晶格索引範圍內、沒有點佔據、而且中心落在 bounds 內的格子中心（依 i 再依 j 的順序）。最多 maxCount 個。
std::vector<Point2D> gridHoles(const GridModel& model, const Rect& bounds, std::size_t maxCount);

}  // namespace aoi
