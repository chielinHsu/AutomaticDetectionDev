#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

// domain 層：只用 C++ 標準庫。
//
// 晶格的信心分數（0..1）＝ 證據 × 精度 × 涵蓋率 × 一致性，四個都在 0..1：
//   證據 support    ＝ 1 − exp(−e / 3)，e ＝ 內點數 − 1（一維）或 − 2（二維）
//                     ＝ 「定義這組晶格最少要的點數（一維 2、二維 3）」之外多出來的點，再加 1。
//                     一維 2 點、二維 3 點都是 0.28（剛好決定一組晶格，最小平方法一定完全擬合，沒有額外證據）；
//                     e ＝ 2 是 0.49、4 是 0.74、9 是 0.95
//   精度 precision  ＝ (0.6 − rms ÷ 容差) ÷ 0.3，夾在 0..1       rms ≤ 0.3 × 容差（正常的位置雜訊）是 1；
//                                                                 ≥ 0.6 × 容差是 0：碰巧落在容差內的點，rms 約 0.58–0.71 × 容差
//   涵蓋率 coverage ＝ 內點數 ÷ 索引範圍內的格子數                 缺件越多越低；隨機點湊出的「晶格」非常低
//   一致性 consistency ＝ 陣列範圍內的點，有多少落在格點上           真的陣列裡元件之間沒有別的元件：約 100%；
//                     雜訊大時找到「每隔一格」的粗晶格，或隨機點湊出的晶格，範圍內滿是不在格點上的點：很低
// 用途：低於 minConfidence 就當作沒有晶格（呼叫端退回「只有找到的」，不補漏）；低於 checkBelow 就建議人工檢查。
// 這些式子與門檻是起點，不是量測出來的事實（和 GenerateOptions 的其他預設值一樣）；S07 用真實影像校準。
namespace aoi {

struct LatticeQuality {
    double support = 0.0;
    double precision = 0.0;
    double coverage = 0.0;
    double consistency = 0.0;
    double confidence = 0.0;
};

inline LatticeQuality assessLattice(std::size_t inliers, double rmsPx, double tolerancePx, double coverage,
                                    double consistency, bool twoDimensional) noexcept {
    LatticeQuality q;
    const double extra = static_cast<double>(inliers) - (twoDimensional ? 2.0 : 1.0);
    q.support = extra > 0.0 ? 1.0 - std::exp(-extra / 3.0) : 0.0;
    const double ratio = tolerancePx > 0.0 ? rmsPx / tolerancePx : 1.0;
    q.precision = std::clamp((0.6 - ratio) / 0.3, 0.0, 1.0);
    q.coverage = std::clamp(coverage, 0.0, 1.0);
    q.consistency = std::clamp(consistency, 0.0, 1.0);
    q.confidence = q.support * q.precision * q.coverage * q.consistency;
    return q;
}

}  // namespace aoi
