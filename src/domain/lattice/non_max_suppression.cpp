#include "domain/lattice/non_max_suppression.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace aoi {

namespace {

// 凸多邊形，頂點放在固定大小的陣列裡：四邊形被四條邊裁切，最多 8 個頂點。不配置記憶體（NMS 每對鄰居都要裁一次）。
struct Polygon {
    std::array<Point2D, 12> v{};
    int n = 0;
    void push(Point2D p) { v[static_cast<std::size_t>(n++)] = p; }
};

double crossAt(Point2D a, Point2D b, Point2D p) { return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x); }

double signedArea(const Point2D* v, int n) {
    double s = 0.0;
    for (int k = 0; k < n; ++k) {
        const Point2D a = v[k];
        const Point2D b = v[(k + 1) % n];
        s += a.x * b.y - b.x * a.y;
    }
    return 0.5 * s;
}

// 矩形的四個角，方向統一成「有號面積為正」（裁切時內側在每條邊的同一邊）。
std::array<Point2D, 4> corners(const RectObj& r) {
    const double hw = 0.5 * r.width;
    const double hh = 0.5 * r.height;
    const Point2D local[4] = {{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}};
    std::array<Point2D, 4> out{};
    for (int k = 0; k < 4; ++k) {
        const Point2D d = rotate(local[k], r.pose.angleDeg);
        out[static_cast<std::size_t>(k)] = {r.pose.x + d.x, r.pose.y + d.y};
    }
    if (signedArea(out.data(), 4) < 0.0) std::swap(out[1], out[3]);
    return out;
}

// Sutherland–Hodgman：用凸多邊形 clipper（有號面積為正）的每條邊裁切 subject。
Polygon clip(const std::array<Point2D, 4>& subjectCorners, const std::array<Point2D, 4>& clipper) {
    Polygon subject;
    for (const Point2D& p : subjectCorners) subject.push(p);
    Polygon input;
    for (std::size_t e = 0; e < clipper.size() && subject.n > 0; ++e) {
        const Point2D a = clipper[e];
        const Point2D b = clipper[(e + 1) % clipper.size()];
        input = subject;
        subject.n = 0;
        for (int k = 0; k < input.n; ++k) {
            const Point2D p = input.v[static_cast<std::size_t>(k)];
            const Point2D q = input.v[static_cast<std::size_t>((k + 1) % input.n)];
            const double cp = crossAt(a, b, p);
            const double cq = crossAt(a, b, q);
            if (cp >= 0.0) subject.push(p);
            if ((cp >= 0.0) != (cq >= 0.0)) {
                const double t = cp / (cp - cq);
                subject.push({p.x + t * (q.x - p.x), p.y + t * (q.y - p.y)});
            }
        }
    }
    return subject;
}

double rectArea(const RectObj& r) { return r.width > 0.0 && r.height > 0.0 ? r.width * r.height : 0.0; }
double halfDiagonal(const RectObj& r) { return 0.5 * std::sqrt(r.width * r.width + r.height * r.height); }

bool finiteBox(const RectObj& r) {
    return std::isfinite(r.pose.x) && std::isfinite(r.pose.y) && std::isfinite(r.pose.angleDeg) && std::isfinite(r.width) &&
           std::isfinite(r.height);
}

}  // namespace

double overlapArea(const RectObj& a, const RectObj& b) noexcept {
    if (rectArea(a) <= 0.0 || rectArea(b) <= 0.0) return 0.0;
    // 快速排除：中心距離大於兩個半對角線的和，不可能重疊（比平方，不開根號）。
    const double reach = halfDiagonal(a) + halfDiagonal(b);
    const double dx = a.pose.x - b.pose.x;
    const double dy = a.pose.y - b.pose.y;
    if (dx * dx + dy * dy >= reach * reach) return 0.0;
    const Polygon inter = clip(corners(a), corners(b));
    if (inter.n < 3) return 0.0;
    return std::min({std::abs(signedArea(inter.v.data(), inter.n)), rectArea(a), rectArea(b)});
}

double overlapRatio(const RectObj& a, const RectObj& b, OverlapMeasure measure) noexcept {
    const double areaA = rectArea(a);
    const double areaB = rectArea(b);
    if (areaA <= 0.0 || areaB <= 0.0) return 0.0;
    const double inter = overlapArea(a, b);
    if (inter <= 0.0) return 0.0;
    const double denominator = measure == OverlapMeasure::IntersectionOverUnion ? areaA + areaB - inter : std::min(areaA, areaB);
    return std::clamp(inter / denominator, 0.0, 1.0);
}

OverlapIndex::OverlapIndex(double cellSize, double maxOverlap, OverlapMeasure measure)
    : cell_(cellSize > 0.0 && std::isfinite(cellSize) ? cellSize : 1.0), maxOverlap_(maxOverlap), measure_(measure) {}

int OverlapIndex::cellOf(double v) const noexcept {
    const double c = std::floor(v / cell_);
    return static_cast<int>(std::clamp(c, -1.0e9, 1.0e9));
}

bool OverlapIndex::overlapsAny(const RectObj& box) const {
    if (boxes_.empty() || !finiteBox(box)) return false;
    // 會重疊的框，中心距離 < 兩個半對角線的和；換成要看幾圈格子。
    const double reach = halfDiagonal(box) + maxHalfDiagonal_;
    const int rings = std::max(1, static_cast<int>(std::ceil(reach / cell_)));
    const int cx = cellOf(box.pose.x);
    const int cy = cellOf(box.pose.y);
    for (int dy = -rings; dy <= rings; ++dy) {
        for (int dx = -rings; dx <= rings; ++dx) {
            const int* first = head_.find(detail::packKey(cx + dx, cy + dy));
            if (first == nullptr) continue;
            for (int k = *first - 1; k >= 0; k = next_[static_cast<std::size_t>(k)]) {
                if (overlapRatio(box, boxes_[static_cast<std::size_t>(k)], measure_) > maxOverlap_) return true;
            }
        }
    }
    return false;
}

void OverlapIndex::add(const RectObj& box) {
    if (!finiteBox(box)) return;
    const int index = static_cast<int>(boxes_.size());
    boxes_.push_back(box);
    int& first = head_[detail::packKey(cellOf(box.pose.x), cellOf(box.pose.y))];
    next_.push_back(first - 1);
    first = index + 1;
    maxHalfDiagonal_ = std::max(maxHalfDiagonal_, halfDiagonal(box));
}

std::vector<std::size_t> NonMaxSuppression::keep(std::span<const ScoredBox> boxes) const {
    std::vector<std::size_t> order;
    order.reserve(boxes.size());
    double cell = 0.0;
    for (std::size_t k = 0; k < boxes.size(); ++k) {
        if (!finiteBox(boxes[k].rect) || !std::isfinite(boxes[k].score)) continue;
        order.push_back(k);
        cell = std::max(cell, 2.0 * halfDiagonal(boxes[k].rect));
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        const ScoredBox& x = boxes[a];
        const ScoredBox& y = boxes[b];
        if (x.score != y.score) return x.score > y.score;
        if (x.rect.pose.y != y.rect.pose.y) return x.rect.pose.y < y.rect.pose.y;
        if (x.rect.pose.x != y.rect.pose.x) return x.rect.pose.x < y.rect.pose.x;
        return a < b;
    });
    OverlapIndex kept(cell, options_.maxOverlap, options_.measure);
    std::vector<std::size_t> out;
    for (const std::size_t k : order) {
        if (kept.overlapsAny(boxes[k].rect)) continue;
        kept.add(boxes[k].rect);
        out.push_back(k);
    }
    return out;
}

}  // namespace aoi
