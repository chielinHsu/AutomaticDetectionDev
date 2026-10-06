#pragma once

#include <span>

#include "domain/geometry.h"
#include "domain/lattice/lattice_detail.h"
#include "domain/lattice/lattice_estimator.h"
#include "domain/lattice/lattice_model.h"

// domain 層：只用 C++ 標準庫。晶格估計的接受規則（LatticeEstimator 內部用）：一組擬合出來的晶格「算不算數」。
// 每一條規則都對應 spec.md「已知會失敗的情況與對策」裡的一個情況；tst_lattice 有對應的情境。
namespace aoi::detail {

// 二維要成立，三個條件都要滿足：
//  1. 不是全部在同一個 i（那其實是沿 v 的一排）；
//  2. 主列（同一個 j 的成員最多的那列）以外至少 minSecondRowInliers 個成員（一排加一個離群點，不可以長出整排假的缺格）；
//  3. 列的一致性：主列以外的內點，有一半以上在「可疑的列」裡就不成立。一列（歸到它的點至少 3 個）是可疑的，如果
//     a. 落在格點上的比例低於 minRowConsistency（真的列除了離群點都在格點上；一排「別的間距」或「別的方向」的元件，
//        碰巧落在格點容差內的大約只有一半），或
//     b. 列線上的點，相鄰兩點的間隔大多不是整數格（那一排有自己的間距，個別點碰巧落在格點附近不代表它屬於這組晶格）。
bool twoDimensionalIsValid(const LatticeCandidate& c, std::span<const Point2D> points, const LatticeOptions& o);

// 證據夠不夠：至少 minInliers 個內點；或者每個點都被這組晶格解釋了（只有 2 個點時，它們就是一組 2 點的晶格）。
// 沒有這一條，一堆隨機點裡碰巧相隔整數格的兩個點就會被當成晶格，中間補出假的缺格。
bool enoughEvidence(const LatticeCandidate& c, std::span<const Point2D> points, const LatticeOptions& o);

// 補缺格要有夠多的證據：缺格數 ＜ 內點數 − 3（一維）或 − 4（二維）；沒有缺格的晶格不受限。
// 例：一維 4 個內點不可以補任何缺格，5 個內點最多補 1 格；大陣列等於「至少找到約一半」。
// 隨機散佈的點（板上不成陣列的同型零件）常有 3–5 個點湊巧排得像晶格（S05 的測試：20 團隨機點、黃金角螺線），
// 沒有這一條會在散佈的零件之間補出假的缺格。代價：真的很小的陣列（4 個以下找到）不補缺格。
bool holesAreBacked(const LatticeModel& m);

// 一致性：陣列範圍（索引範圍每邊多半格；一維時是沿 u、寬一個格距的帶子）裡的點，有多少落在有內點的格點上
// （同一格的重複點也算）。真的陣列裡元件之間沒有別的元件，所以接近 1。
double consistency(const LatticeModel& m, std::span<const Point2D> points);

}  // namespace aoi::detail
