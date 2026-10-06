#pragma once

#include <QtTest>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "domain/geometry.h"
#include "domain/lattice/lattice_model.h"
#include "domain/lattice/lattice_predictor.h"

// 晶格測試共用的輔助：可重現的亂數、合成陣列，以及「和標準答案比對」的檢查。
// 標準答案來自產生器（我們知道每個點屬於哪一格、哪些格子缺件），不是來自被測的估計器。
namespace aoi::test {

// splitmix64：只用整數運算，各平台、各組態結果相同（不用 std 的分佈）。
class Rng {
public:
    explicit Rng(std::uint64_t seed) : state_(seed) {}
    std::uint64_t next() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    double uniform() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }  // [0, 1)
    double symmetric(double amplitude) { return (uniform() * 2.0 - 1.0) * amplitude; }        // [−a, a)
    std::size_t below(std::size_t n) { return static_cast<std::size_t>(next() % n); }

private:
    std::uint64_t state_;
};

struct GridSpec {
    int cols = 5;
    int rows = 4;
    double pitchX = 30.0;
    double pitchY = 20.0;
    double angleDeg = 0.0;  // 整個陣列的角度（geometry.h 的慣例）
    Point2D origin{100.0, 100.0};
};

inline Point2D cellPosition(const GridSpec& g, int col, int row) {
    const Point2D offset = rotate({col * g.pitchX, row * g.pitchY}, g.angleDeg);
    return {g.origin.x + offset.x, g.origin.y + offset.y};
}

inline std::vector<Point2D> makePoints(const GridSpec& g, const std::vector<std::pair<int, int>>& skip = {}) {
    std::vector<Point2D> points;
    for (int c = 0; c < g.cols; ++c) {
        for (int r = 0; r < g.rows; ++r) {
            if (std::find(skip.begin(), skip.end(), std::make_pair(c, r)) != skip.end()) continue;
            points.push_back(cellPosition(g, c, r));
        }
    }
    return points;
}

inline bool hasPointNear(const std::vector<Point2D>& points, Point2D target, double tolerance) {
    return std::any_of(points.begin(), points.end(), [&](Point2D p) { return distance(p, target) <= tolerance; });
}

// 失敗訊息用：一行說明一組晶格。
inline QByteArray describe(const LatticeModel& m) {
    return QStringLiteral("u=(%1,%2) v=(%3,%4) hasV=%5 inliers=%6 box=%7x%8 rms=%9 tol=%10 coverage=%11 confidence=%12")
        .arg(m.basis.u.x, 0, 'f', 3).arg(m.basis.u.y, 0, 'f', 3).arg(m.basis.v.x, 0, 'f', 3).arg(m.basis.v.y, 0, 'f', 3)
        .arg(m.hasV()).arg(m.inliers.size()).arg(m.maxI - m.minI + 1).arg(m.maxJ - m.minJ + 1)
        .arg(m.rmsPx, 0, 'f', 3).arg(m.tolerancePx, 0, 'f', 3).arg(m.coverage, 0, 'f', 3).arg(m.confidence, 0, 'f', 3)
        .toUtf8();
}

inline std::vector<Point2D> positions(const std::vector<PredictedCell>& cells) {
    std::vector<Point2D> out;
    for (const PredictedCell& c : cells) out.push_back(c.position);
    return out;
}

// 掃描用的情境：陣列、雜訊（每軸均勻 ±noisePx）、缺件比例、離群點數。
struct Scenario {
    GridSpec grid;
    double noisePx = 0.0;
    double missingFraction = 0.0;
    int outliers = 0;
    std::uint64_t seed = 1;
};

struct Observed {
    std::vector<Point2D> points;
    std::vector<int> col;  // 每個點的標準答案；離群點是 −1
    std::vector<int> row;
    std::vector<std::pair<int, int>> missing;  // 缺件的格子
};

// 先決定缺件（固定數量：round(比例 × 格數)，以亂數洗牌挑），再加雜訊，最後在陣列外接矩形（每邊多一格）內撒離群點。
// 離群點刻意避開格點（離最近的格點至少 0.35 格距），否則它其實就是一個「在格點上的元件」。
inline Observed generate(const Scenario& s) {
    Rng rng(s.seed);
    const GridSpec& g = s.grid;
    const int cells = g.cols * g.rows;
    std::vector<int> order(static_cast<std::size_t>(cells));
    for (int k = 0; k < cells; ++k) order[static_cast<std::size_t>(k)] = k;
    for (int k = cells - 1; k > 0; --k) std::swap(order[static_cast<std::size_t>(k)], order[rng.below(static_cast<std::size_t>(k) + 1)]);
    const int missingCount = static_cast<int>(std::lround(s.missingFraction * cells));
    std::vector<char> isMissing(static_cast<std::size_t>(cells), 0);
    for (int k = 0; k < missingCount; ++k) isMissing[static_cast<std::size_t>(order[static_cast<std::size_t>(k)])] = 1;

    Observed o;
    for (int c = 0; c < g.cols; ++c) {
        for (int r = 0; r < g.rows; ++r) {
            const int id = c * g.rows + r;
            const double nx = rng.symmetric(s.noisePx);
            const double ny = rng.symmetric(s.noisePx);
            if (isMissing[static_cast<std::size_t>(id)] != 0) {
                o.missing.push_back({c, r});
                continue;
            }
            const Point2D p = cellPosition(g, c, r);
            o.points.push_back({p.x + nx, p.y + ny});
            o.col.push_back(c);
            o.row.push_back(r);
        }
    }
    const double minPitch = std::min(g.pitchX, g.pitchY);
    for (int k = 0; k < s.outliers;) {
        const double a = rng.uniform() * (g.cols + 1) - 1.0;  // 格子座標，外接矩形每邊多一格
        const double b = rng.uniform() * (g.rows + 1) - 1.0;
        const Point2D offset = rotate({a * g.pitchX, b * g.pitchY}, g.angleDeg);
        const Point2D p{g.origin.x + offset.x, g.origin.y + offset.y};
        const Point2D nearest = cellPosition(g, static_cast<int>(std::lround(a)), static_cast<int>(std::lround(b)));
        if (distance(p, nearest) < 0.35 * minPitch) continue;
        o.points.push_back(p);
        o.col.push_back(-1);
        o.row.push_back(-1);
        ++k;
    }
    return o;
}

// 與標準答案比對的結果。
struct Agreement {
    std::size_t inliers = 0;          // 模型的內點數
    std::size_t trueInliers = 0;      // 其中真的是陣列元件、而且格點就是它的標準答案位置
    std::size_t wrongCells = 0;       // 陣列元件被指派到別的格點，或離群點被當成內點
    std::size_t holes = 0;            // 預測的缺格
    std::size_t trueHoles = 0;        // 其中落在真的缺件位置（標準答案）
    std::size_t missedHoles = 0;      // 內點索引範圍內真的缺件、卻沒有預測
    double maxHoleErrorPx = 0.0;      // 預測位置與標準答案的最大距離（只算 trueHoles）
};

// 模型與產生器的標準答案逐點比對。tolerance：「同一個格點」的距離上限（通常是格距的 0.25）。
inline Agreement compare(const LatticeModel& m, const Observed& o, const GridSpec& g, double tolerance,
                         const Rect& bounds = {-1e9, -1e9, 2e9, 2e9}) {
    Agreement a;
    a.inliers = m.inliers.size();
    int minCol = 1 << 30, maxCol = -(1 << 30), minRow = 1 << 30, maxRow = -(1 << 30);
    for (std::size_t k = 0; k < m.inliers.size(); ++k) {
        const std::size_t p = m.inliers[k];
        if (o.col[p] < 0) {
            ++a.wrongCells;
            continue;
        }
        const Point2D truth = cellPosition(g, o.col[p], o.row[p]);
        if (distance(m.basis.at(m.cells[k]), truth) <= tolerance) {
            ++a.trueInliers;
            minCol = std::min(minCol, o.col[p]);
            maxCol = std::max(maxCol, o.col[p]);
            minRow = std::min(minRow, o.row[p]);
            maxRow = std::max(maxRow, o.row[p]);
        } else {
            ++a.wrongCells;
        }
    }
    const std::vector<PredictedCell> holes = LatticePredictor(m, bounds).holes();
    a.holes = holes.size();
    for (const PredictedCell& h : holes) {
        for (const auto& [c, r] : o.missing) {
            const double d = distance(h.position, cellPosition(g, c, r));
            if (d <= tolerance) {
                ++a.trueHoles;
                a.maxHoleErrorPx = std::max(a.maxHoleErrorPx, d);
                break;
            }
        }
    }
    for (const auto& [c, r] : o.missing) {
        if (c < minCol || c > maxCol || r < minRow || r > maxRow) continue;  // 範圍外的缺件本來就不預測
        const Point2D truth = cellPosition(g, c, r);
        if (!bounds.contains(truth)) continue;
        if (!hasPointNear(positions(holes), truth, tolerance)) ++a.missedHoles;
    }
    return a;
}

}  // namespace aoi::test
