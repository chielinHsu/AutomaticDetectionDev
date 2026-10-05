#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "domain/geometry.h"
#include "domain/object_state.h"

// infrastructure/backend_fake：不碰任何視覺函式庫的「假 back end」，只用標準庫。
// 用途：讓 use case 與 JobRunner 能在沒有 OpenCV／HALCON 的情況下端到端跑通、可重現地測試（S03）；
// S04 的 Presenter 與 UI 在 S07 之前也用它。它不看影像內容。
//
// 假的「世界」：一個以使用者框的樣本為中心的合成網格。格子中心距是「樣本大小的倍數」，
// 所以不管影像多大、樣本框在哪裡，網格都貼著它排出來；使用者框一個樣本就得到一排可預期的結果。
// 每個格子的行為（缺件、低分、位置雜訊）由 (seed, i, j) 的雜湊決定，與走訪順序、呼叫次數、執行緒無關，所以完全可重現。
namespace aoi::fake {

struct FakeSpec {
    // 格子中心距 ＝ 倍數 × 樣本大小。
    double pitchFactorX = 1.5;
    double pitchFactorY = 1.5;
    // 格子與每個零件的角度（度，座標慣例見 domain/geometry.h）。
    double gridAngleDeg = 0.0;
    // 樣本向左右（欄）與上下（列）最多各排幾格；0 ＝ 排滿影像。
    int halfCols = 0;
    int halfRows = 0;
    // 位置的決定性雜訊，每個軸 ±jitterPx（樣本本身沒有雜訊）。
    double jitterPx = 0.0;
    // 沒有零件的格子（缺件）的比例。
    double missingFraction = 0.0;
    // 有零件、但分數低的格子比例：種子搜尋（分數下限高）找不到，視窗驗證（下限低）找得到。
    double weakFraction = 0.0;
    double strongScoreMin = 0.90;
    double strongScoreMax = 0.99;
    double weakScoreMin = 0.40;
    double weakScoreMax = 0.65;
    std::uint64_t seed = 1;
    // 每次 find 假裝花這麼多毫秒（以 1 ms 為單位睡，每次都檢查取消）。用來測 latest-wins 與取消。
    double latencyMs = 0.0;
    // 讓範本建立器回報一個樣本問題（模擬「框到不只一個元件」之類，真的 back end 才看得出來的事）。
    std::optional<NoMatchReason> forcedSampleProblem;
};

enum class FakeCellKind { Strong, Weak, Missing };

struct FakeCell {
    int i = 0;  // 欄索引（樣本在 0）
    int j = 0;  // 列索引
    FakeCellKind kind = FakeCellKind::Strong;
    Pose2D pose;         // 搜尋器會回報的位姿（含雜訊）；Missing 的是格子的理想位置
    double score = 0.0;  // Missing 是 0
};

// 依 spec 排出整個網格（含缺件的格子；中心在影像範圍外的格子不產生）。測試拿它當標準答案，
// 範本建立器也用它：兩邊用同一份產生器，所以「標準答案」與「搜尋器看到的世界」一定一致。
// sample 是已夾到影像內的樣本框。順序：依 i 再依 j。
std::vector<FakeCell> makeFakeGrid(const FakeSpec& spec, const Rect& sample, int imageWidth, int imageHeight);

}  // namespace aoi::fake
