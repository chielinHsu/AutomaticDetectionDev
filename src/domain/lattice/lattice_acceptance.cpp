#include "domain/lattice/lattice_acceptance.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace aoi::detail {

bool twoDimensionalIsValid(const LatticeCandidate& c, std::span<const Point2D> points, const LatticeOptions& o) {
    if (!c.basis.hasV || c.cells.empty()) return false;
    std::vector<int> js;
    js.reserve(c.cells.size());
    bool singleColumn = true;
    int minI = c.cells.front().i, maxI = minI, minJ = c.cells.front().j, maxJ = minJ;
    for (const LatticeIndex& cell : c.cells) {
        js.push_back(cell.j);
        singleColumn = singleColumn && cell.i == c.cells.front().i;
        minI = std::min(minI, cell.i);
        maxI = std::max(maxI, cell.i);
        minJ = std::min(minJ, cell.j);
        maxJ = std::max(maxJ, cell.j);
    }
    if (singleColumn) return false;
    std::sort(js.begin(), js.end());
    std::size_t longest = 0;
    int mainRow = js.front();
    for (std::size_t a = 0; a < js.size();) {
        std::size_t b = a;
        while (b < js.size() && js[b] == js[a]) ++b;
        if (b - a > longest) {
            longest = b - a;
            mainRow = js[a];
        }
        a = b;
    }
    if (c.members.size() - longest < o.minSecondRowInliers) return false;

    // 逐列判斷（不和主列比：主列的點不多，碰巧沒有離群點時比例是 100%，拿它當基準，離群點多的真陣列反而會被拒絕）。
    // 每個點歸到最近的一列（列座標四捨五入），不只看正好在線上的：斜著穿過某一列的另一排元件，只有幾個點正好在線上，
    // 其中幾個又碰巧在格點上（S05 的說明頁情境抓到過，曾補出 9 個假缺格）。一列（至少 3 個點）有下列任一種情形就是可疑的：
    //  a. 落在格點上的比例 < minRowConsistency；
    //  b. 列線上（垂直方向在容差內）的點，相鄰兩點沿 u 的間隔大多不是整數格：那是一排「自己的間距」的元件
    //     （例如間距 34 的一排配上 20 的格子，個別點有 6 成碰巧落在格點附近，但間隔永遠是 34 ＝ 1.7 格）。
    const double tol2 = c.tolerance * c.tolerance;
    const double rowSpacing = std::abs(cross(c.basis.u, c.basis.v)) / length(c.basis.u);
    const double band = c.tolerance / rowSpacing;            // 列座標離整數不到這個值：在列線上
    const double gapTolerance = c.tolerance / length(c.basis.u);  // 間隔離整數格不到這個值：是整數格
    const Assigner assign(c.basis);
    const CellTable occupied = CellTable::of(c.cells);  // 同一格的重複點也算「落在格點上」
    const std::size_t rows = static_cast<std::size_t>(maxJ - minJ + 1);
    std::vector<std::size_t> inRow(rows, 0), onNode(rows, 0);
    std::vector<std::pair<std::size_t, double>> onLine;  // (列, 沿 u 的格子座標)
    for (const Point2D& p : points) {
        const Point2D ab = assign.coordinates(p);
        const int j = fastRound(ab.y);
        if (j < minJ || j > maxJ) continue;
        if (ab.x < minI - 0.5 || ab.x > maxI + 0.5) continue;
        const Assigned a = assign(p);
        const auto row = static_cast<std::size_t>(j - minJ);
        ++inRow[row];
        if (a.residual2 <= tol2 && occupied.get(a.cell.i, a.cell.j) >= 0) ++onNode[row];
        if (std::abs(ab.y - j) <= band) onLine.push_back({row, ab.x});
    }
    std::sort(onLine.begin(), onLine.end());
    std::vector<std::size_t> gaps(rows, 0), wholeGaps(rows, 0);
    for (std::size_t k = 1; k < onLine.size(); ++k) {
        if (onLine[k].first != onLine[k - 1].first) continue;
        const double g = onLine[k].second - onLine[k - 1].second;
        const int cells = fastRound(g);
        if (cells == 0) continue;  // 同一格的重複點：不算間隔
        ++gaps[onLine[k].first];
        if (std::abs(g - cells) <= gapTolerance) ++wholeGaps[onLine[k].first];
    }
    std::size_t secondary = 0;  // 主列以外落在格點上的點
    std::size_t suspect = 0;    // 其中在「可疑的列」裡的
    for (std::size_t row = 0; row < rows; ++row) {
        if (static_cast<int>(row) + minJ == mainRow) continue;
        secondary += onNode[row];
        if (inRow[row] < 3) continue;
        const bool fewOnNodes = static_cast<double>(onNode[row]) < o.minRowConsistency * static_cast<double>(inRow[row]);
        const bool ownSpacing = gaps[row] >= 2 && 2 * wholeGaps[row] < gaps[row];
        if (fewOnNodes || ownSpacing) suspect += onNode[row];
    }
    return secondary > 0 && 2 * suspect < secondary;
}

bool enoughEvidence(const LatticeCandidate& c, std::span<const Point2D> points, const LatticeOptions& o) {
    if (c.members.size() < 2) return false;
    if (c.members.size() >= o.minInliers) return true;
    const double tol2 = c.tolerance * c.tolerance;
    const Assigner assign(c.basis);
    const CellTable occupied = CellTable::of(c.cells);
    std::size_t explained = 0;
    for (const Point2D& p : points) {
        const Assigned a = assign(p);
        if (a.residual2 <= tol2 && occupied.get(a.cell.i, a.cell.j) >= 0) ++explained;
    }
    return explained == points.size();
}

bool holesAreBacked(const LatticeModel& m) {
    const long long inliers = static_cast<long long>(m.inliers.size());
    const long long holes = static_cast<long long>(m.cellCount()) - inliers;
    return holes == 0 || holes < inliers - (m.hasV() ? 4 : 3);
}

double consistency(const LatticeModel& m, std::span<const Point2D> points) {
    const LatticeBasis& b = m.basis;
    const Assigner assign(b);
    const CellTable occupied = CellTable::of(m.cells);
    const double tol2 = m.tolerancePx * m.tolerancePx;
    const double lu = length(b.u);
    std::size_t inside = 0;
    std::size_t onNode = 0;
    for (const Point2D& p : points) {
        const Point2D ab = assign.coordinates(p);
        if (ab.x < m.minI - 0.5 || ab.x > m.maxI + 0.5) continue;
        if (b.hasV) {
            if (ab.y < m.minJ - 0.5 || ab.y > m.maxJ + 0.5) continue;
        } else if (std::abs(cross(b.u, minus(p, b.origin))) / lu > 0.5 * lu) {
            continue;  // 離這一排的線超過半個格距
        }
        ++inside;
        const Assigned a = assign(p);
        if (a.residual2 <= tol2 && occupied.get(a.cell.i, a.cell.j) >= 0) ++onNode;
    }
    return inside == 0 ? 0.0 : static_cast<double>(onNode) / static_cast<double>(inside);
}

}  // namespace aoi::detail
