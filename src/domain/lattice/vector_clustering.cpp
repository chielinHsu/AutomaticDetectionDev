#include "domain/lattice/vector_clustering.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>

#include "domain/lattice/flat_map.h"

namespace aoi {

namespace {

bool finitePoint(Point2D p) { return std::isfinite(p.x) && std::isfinite(p.y); }

// floor 的快速版（std::floor 在這個工具鏈上是函式呼叫，投票時每個向量要呼叫兩次）。呼叫端保證 |v| < 1e9。
int fastFloor(double v) {
    const int t = static_cast<int>(v);
    return v < static_cast<double>(t) ? t - 1 : t;
}

// 兩個策略共用的收尾：由「每個向量的暫時群號」算出每群的成員數與平均，依契約排序，再把群號換成排序後的索引。
VectorClusters finish(std::span<const Point2D> vectors, std::vector<int> labels, std::size_t clusterCount) {
    struct Acc {
        double sx = 0.0;
        double sy = 0.0;
        std::size_t n = 0;
    };
    std::vector<Acc> acc(clusterCount);
    for (std::size_t k = 0; k < vectors.size(); ++k) {
        if (labels[k] < 0) continue;
        Acc& a = acc[static_cast<std::size_t>(labels[k])];
        a.sx += vectors[k].x;
        a.sy += vectors[k].y;
        ++a.n;
    }
    std::vector<VectorCluster> raw(clusterCount);
    for (std::size_t c = 0; c < clusterCount; ++c) {
        if (acc[c].n == 0) continue;
        const double n = static_cast<double>(acc[c].n);
        raw[c] = {{acc[c].sx / n, acc[c].sy / n}, acc[c].n};
    }
    std::vector<std::size_t> order(clusterCount);
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const VectorCluster& ca = raw[a];
        const VectorCluster& cb = raw[b];
        if (ca.members != cb.members) return ca.members > cb.members;
        const double la = std::hypot(ca.center.x, ca.center.y);
        const double lb = std::hypot(cb.center.x, cb.center.y);
        if (la != lb) return la < lb;
        if (ca.center.x != cb.center.x) return ca.center.x < cb.center.x;
        return ca.center.y < cb.center.y;
    });
    VectorClusters out;
    std::vector<int> newIndex(clusterCount, -1);
    for (const std::size_t c : order) {
        if (raw[c].members == 0) continue;
        newIndex[c] = static_cast<int>(out.clusters.size());
        out.clusters.push_back(raw[c]);
    }
    for (int& label : labels) {
        if (label >= 0) label = newIndex[static_cast<std::size_t>(label)];
    }
    out.labels = std::move(labels);
    return out;
}

}  // namespace

VectorClusters GridVoteClustering::cluster(std::span<const Point2D> vectors, double scale) const {
    double bin = binFraction_ * scale;
    if (!(bin > 0.0) || !std::isfinite(bin)) bin = 1.0;

    struct Bin {
        std::size_t count = 0;
        int cluster = -1;
    };
    constexpr double kMaxBin = 1.0e9;  // 超出 int 範圍的向量不參與投票
    detail::FlatMap<Bin> bins(vectors.size());
    std::vector<std::uint64_t> keyOf(vectors.size(), 0);
    std::vector<std::uint8_t> voted(vectors.size(), 0);
    for (std::size_t k = 0; k < vectors.size(); ++k) {
        const Point2D d = vectors[k];
        if (!finitePoint(d)) continue;
        const double fx = d.x / bin;
        const double fy = d.y / bin;
        if (!(std::abs(fx) < kMaxBin && std::abs(fy) < kMaxBin)) continue;
        keyOf[k] = detail::packKey(fastFloor(fx), fastFloor(fy));
        voted[k] = 1;
        ++bins[keyOf[k]].count;
    }

    // 峰：票數比 8 個鄰居都多；同票時鍵比較小的勝出（同票的一片格子只選出一個峰）。
    struct Peak {
        std::uint64_t key = 0;
        std::size_t neighbourhood = 0;  // 3×3 的票數總和：決定峰的優先順序
    };
    std::vector<Peak> peaks;
    bins.forEach([&](std::uint64_t key, const Bin& b) {
        const int bx = detail::unpackFirst(key);
        const int by = detail::unpackSecond(key);
        std::size_t sum = b.count;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                const std::uint64_t nk = detail::packKey(bx + dx, by + dy);
                const Bin* nb = bins.find(nk);
                if (nb == nullptr) continue;
                if (nb->count > b.count || (nb->count == b.count && nk < key)) return;  // 不是峰
                sum += nb->count;
            }
        }
        peaks.push_back({key, sum});
    });
    std::sort(peaks.begin(), peaks.end(), [](const Peak& a, const Peak& b) {
        return a.neighbourhood != b.neighbourhood ? a.neighbourhood > b.neighbourhood : a.key < b.key;
    });

    // 每一格歸給「3×3 範圍包含它、優先順序最高」的峰。
    for (std::size_t p = 0; p < peaks.size(); ++p) {
        const int bx = detail::unpackFirst(peaks[p].key);
        const int by = detail::unpackSecond(peaks[p].key);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                Bin* b = bins.find(detail::packKey(bx + dx, by + dy));
                if (b != nullptr && b->cluster < 0) b->cluster = static_cast<int>(p);
            }
        }
    }
    std::vector<int> labels(vectors.size(), -1);
    for (std::size_t k = 0; k < vectors.size(); ++k) {
        if (voted[k] == 0) continue;
        labels[k] = bins.find(keyOf[k])->cluster;
    }
    return finish(vectors, std::move(labels), peaks.size());
}

VectorClusters GreedyClustering::cluster(std::span<const Point2D> vectors, double scale) const {
    const double floorLength = std::isfinite(scale) && scale > 0.0 ? 0.05 * scale : 0.0;
    std::vector<std::size_t> order;
    order.reserve(vectors.size());
    for (std::size_t k = 0; k < vectors.size(); ++k) {
        if (finitePoint(vectors[k])) order.push_back(k);
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return std::hypot(vectors[a].x, vectors[a].y) < std::hypot(vectors[b].x, vectors[b].y);
    });
    struct Running {
        double sx = 0.0;
        double sy = 0.0;
        std::size_t n = 0;
        Point2D center() const { return {sx / static_cast<double>(n), sy / static_cast<double>(n)}; }
    };
    std::vector<Running> clusters;
    std::vector<int> labels(vectors.size(), -1);
    for (const std::size_t k : order) {
        const Point2D d = vectors[k];
        int placed = -1;
        for (std::size_t c = 0; c < clusters.size(); ++c) {
            const Point2D center = clusters[c].center();
            const double reach = tolerance_ * std::max(std::hypot(center.x, center.y), floorLength);
            if (distance(center, d) <= reach) {
                placed = static_cast<int>(c);
                break;
            }
        }
        if (placed < 0) {
            placed = static_cast<int>(clusters.size());
            clusters.push_back({});
        }
        Running& r = clusters[static_cast<std::size_t>(placed)];
        r.sx += d.x;
        r.sy += d.y;
        ++r.n;
        labels[k] = placed;
    }
    return finish(vectors, std::move(labels), clusters.size());
}

}  // namespace aoi
