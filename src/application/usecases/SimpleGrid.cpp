#include "application/usecases/SimpleGrid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <utility>

namespace aoi {

namespace {

constexpr std::size_t kMaxEstimatePoints = 800;  // 估計基底向量時最多取幾個點
constexpr std::size_t kNeighbors = 6;            // 每個點看最近幾個鄰居
constexpr double kClusterTolerance = 0.2;        // 兩個位移向量相差在較短者長度的這個比例內，算同一群
constexpr double kParallelSine = 0.5;            // 兩個向量夾角的 sin 小於這個值（約 30°），視為平行
constexpr double kSecondVectorSupport = 0.2;     // 第二個基底向量的票數至少是最多票的這個比例
constexpr double kInlierFraction = 0.25;         // 內點：離擬合位置不超過最短基底向量的這個比例

double length(Point2D v) { return std::hypot(v.x, v.y); }

// 把位移向量放到同一個半平面，v 與 −v（A 看 B、B 看 A）才算同一個向量。
Point2D toHalfPlane(Point2D d) {
    if (d.x < 0.0 || (d.x == 0.0 && d.y < 0.0)) return {-d.x, -d.y};
    return d;
}

struct Cluster {
    Point2D center;
    std::size_t count = 0;
};

std::vector<Cluster> clusterVectors(std::vector<Point2D> vectors) {
    std::sort(vectors.begin(), vectors.end(), [](Point2D a, Point2D b) { return length(a) < length(b); });
    std::vector<Cluster> clusters;
    for (const Point2D d : vectors) {
        bool placed = false;
        for (Cluster& c : clusters) {
            if (distance(c.center, d) <= kClusterTolerance * length(c.center)) {
                const double n = static_cast<double>(c.count);
                c.center = {(c.center.x * n + d.x) / (n + 1.0), (c.center.y * n + d.y) / (n + 1.0)};
                ++c.count;
                placed = true;
                break;
            }
        }
        if (!placed) clusters.push_back({d, 1});
    }
    return clusters;
}

bool parallel(Point2D a, Point2D b) {
    const double cross = std::abs(a.x * b.y - a.y * b.x);
    return cross < kParallelSine * length(a) * length(b);
}

// 解最小平方法的正規方程（參數 2 或 3 個）。奇異時回傳 false。
bool solveLeastSquares(const std::vector<std::array<double, 3>>& rows, const std::vector<double>& rhs, int params,
                       std::array<double, 3>& out) {
    double a[3][4] = {};
    for (std::size_t k = 0; k < rows.size(); ++k) {
        for (int r = 0; r < params; ++r) {
            for (int c = 0; c < params; ++c) a[r][c] += rows[k][static_cast<std::size_t>(r)] * rows[k][static_cast<std::size_t>(c)];
            a[r][params] += rows[k][static_cast<std::size_t>(r)] * rhs[k];
        }
    }
    for (int col = 0; col < params; ++col) {
        int pivot = col;
        for (int r = col + 1; r < params; ++r) {
            if (std::abs(a[r][col]) > std::abs(a[pivot][col])) pivot = r;
        }
        if (std::abs(a[pivot][col]) < 1e-9) return false;
        if (pivot != col) {
            for (int c = 0; c <= params; ++c) std::swap(a[pivot][c], a[col][c]);
        }
        for (int r = col + 1; r < params; ++r) {
            const double f = a[r][col] / a[col][col];
            for (int c = col; c <= params; ++c) a[r][c] -= f * a[col][c];
        }
    }
    for (int r = params - 1; r >= 0; --r) {
        double s = a[r][params];
        for (int c = r + 1; c < params; ++c) s -= a[r][c] * out[static_cast<std::size_t>(c)];
        out[static_cast<std::size_t>(r)] = s / a[r][r];
    }
    return true;
}

std::uint64_t cellKey(int i, int j) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(i)) << 32) | static_cast<std::uint32_t>(j);
}

}  // namespace

std::optional<GridModel> fitGrid(const std::vector<Point2D>& points) {
    const std::size_t n = points.size();
    if (n < 2) return std::nullopt;

    // 1. 每個（抽樣的）點到最近幾個鄰居的位移向量。
    const std::size_t stride = std::max<std::size_t>(1, n / kMaxEstimatePoints);
    std::vector<Point2D> vectors;
    std::vector<std::pair<double, Point2D>> byDistance;
    for (std::size_t i = 0; i < n; i += stride) {
        byDistance.clear();
        for (std::size_t k = 0; k < n; ++k) {
            if (k == i) continue;
            const Point2D d{points[k].x - points[i].x, points[k].y - points[i].y};
            const double len = length(d);
            if (len < 1e-6) continue;  // 重疊的點沒有方向可言
            byDistance.push_back({len, d});
        }
        const std::size_t take = std::min(kNeighbors, byDistance.size());
        std::partial_sort(byDistance.begin(), byDistance.begin() + static_cast<std::ptrdiff_t>(take), byDistance.end(),
                          [](const auto& a, const auto& b) { return a.first < b.first; });
        for (std::size_t k = 0; k < take; ++k) vectors.push_back(toHalfPlane(byDistance[k].second));
    }
    if (vectors.empty()) return std::nullopt;

    // 2. 群聚：最多票的群是 u（票數接近時取較短的，避免選到 2u）；與 u 不平行、票數夠、最短的群是 v。
    const std::vector<Cluster> clusters = clusterVectors(std::move(vectors));
    std::size_t maxCount = 0;
    for (const Cluster& c : clusters) maxCount = std::max(maxCount, c.count);
    const Cluster* uCluster = nullptr;
    for (const Cluster& c : clusters) {
        if (static_cast<double>(c.count) < 0.6 * static_cast<double>(maxCount)) continue;
        if (uCluster == nullptr || length(c.center) < length(uCluster->center)) uCluster = &c;
    }
    const Cluster* vCluster = nullptr;
    for (const Cluster& c : clusters) {
        if (c.count < 2 || static_cast<double>(c.count) < kSecondVectorSupport * static_cast<double>(maxCount)) continue;
        if (parallel(uCluster->center, c.center)) continue;
        if (vCluster == nullptr || length(c.center) < length(vCluster->center)) vCluster = &c;
    }

    GridModel model;
    model.u = uCluster->center;
    model.hasV = vCluster != nullptr;
    if (model.hasV) model.v = vCluster->center;
    const double det = model.u.x * model.v.y - model.u.y * model.v.x;
    if (model.hasV && std::abs(det) < 1e-9 * length(model.u) * length(model.v)) model.hasV = false;
    if (!model.hasV) model.v = {0.0, 0.0};

    // 3. 以最接近重心的點為原點，把每個點指派到最近的格子（索引），離格子太遠的不是內點。
    Point2D centroid{0.0, 0.0};
    for (const Point2D& p : points) {
        centroid.x += p.x;
        centroid.y += p.y;
    }
    centroid = {centroid.x / static_cast<double>(n), centroid.y / static_cast<double>(n)};
    Point2D p0 = points.front();
    for (const Point2D& p : points) {
        if (distance(p, centroid) < distance(p0, centroid)) p0 = p;
    }
    const double tolerance = kInlierFraction * (model.hasV ? std::min(length(model.u), length(model.v)) : length(model.u));
    struct Assigned {
        int i, j;
        Point2D p;
    };
    std::vector<Assigned> inliers;
    for (const Point2D& p : points) {
        const Point2D d{p.x - p0.x, p.y - p0.y};
        int i = 0;
        int j = 0;
        if (model.hasV) {
            i = static_cast<int>(std::lround((d.x * model.v.y - d.y * model.v.x) / det));
            j = static_cast<int>(std::lround((model.u.x * d.y - model.u.y * d.x) / det));
        } else {
            i = static_cast<int>(std::lround((d.x * model.u.x + d.y * model.u.y) / (length(model.u) * length(model.u))));
        }
        const Point2D predicted{p0.x + i * model.u.x + j * model.v.x, p0.y + i * model.u.y + j * model.v.y};
        if (distance(p, predicted) <= tolerance) inliers.push_back({i, j, p});
    }
    if (inliers.size() < 2) return std::nullopt;

    // 4. 最小平方法精修：x = ox + i·ux + j·vx，y 同理（每個座標軸 2 或 3 個參數）。
    model.origin = p0;
    const int params = model.hasV ? 3 : 2;
    std::vector<std::array<double, 3>> rows;
    std::vector<double> rhsX;
    std::vector<double> rhsY;
    for (const Assigned& a : inliers) {
        rows.push_back({1.0, static_cast<double>(a.i), static_cast<double>(a.j)});
        rhsX.push_back(a.p.x);
        rhsY.push_back(a.p.y);
    }
    std::array<double, 3> px{};
    std::array<double, 3> py{};
    if (solveLeastSquares(rows, rhsX, params, px) && solveLeastSquares(rows, rhsY, params, py)) {
        model.origin = {px[0], py[0]};
        model.u = {px[1], py[1]};
        if (model.hasV) model.v = {px[2], py[2]};
    }

    // 5. 索引範圍、佔據的格子與殘差。
    double sumSq = 0.0;
    model.minI = model.maxI = inliers.front().i;
    model.minJ = model.maxJ = inliers.front().j;
    for (const Assigned& a : inliers) {
        model.minI = std::min(model.minI, a.i);
        model.maxI = std::max(model.maxI, a.i);
        model.minJ = std::min(model.minJ, a.j);
        model.maxJ = std::max(model.maxJ, a.j);
        model.occupied.push_back({a.i, a.j});
        const double d = distance(a.p, model.at(a.i, a.j));
        sumSq += d * d;
    }
    model.rmsPx = std::sqrt(sumSq / static_cast<double>(inliers.size()));
    return model;
}

std::vector<Point2D> gridHoles(const GridModel& model, const Rect& bounds, std::size_t maxCount) {
    std::unordered_set<std::uint64_t> taken;
    taken.reserve(model.occupied.size() * 2);
    for (const GridCell& c : model.occupied) taken.insert(cellKey(c.i, c.j));

    std::vector<Point2D> holes;
    const int jFrom = model.hasV ? model.minJ : 0;
    const int jTo = model.hasV ? model.maxJ : 0;
    for (int i = model.minI; i <= model.maxI; ++i) {
        for (int j = jFrom; j <= jTo; ++j) {
            if (taken.count(cellKey(i, j)) != 0) continue;
            const Point2D at = model.at(i, j);
            if (!bounds.contains(at)) continue;
            if (holes.size() >= maxCount) return holes;
            holes.push_back(at);
        }
    }
    return holes;
}

}  // namespace aoi
