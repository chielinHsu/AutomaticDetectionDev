#include "infrastructure/backend_fake/FakeGrid.h"

#include <algorithm>
#include <cmath>

namespace aoi::fake {

namespace {

constexpr std::size_t kMaxCells = 1u << 20;

// splitmix64：沒有狀態的雜湊，同樣的輸入永遠得到同樣的輸出（不依賴 std 的分佈，各平台結果相同）。
std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

// (seed, i, j, k) → [0, 1)
double unit(std::uint64_t seed, int i, int j, int k) {
    std::uint64_t key = seed;
    key = splitmix64(key ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(i) + (1 << 20)));
    key = splitmix64(key ^ (static_cast<std::uint64_t>(static_cast<std::int64_t>(j) + (1 << 20)) << 21));
    key = splitmix64(key + static_cast<std::uint64_t>(k));
    return static_cast<double>(key >> 11) * (1.0 / 9007199254740992.0);
}

double lerp(double a, double b, double t) { return a + (b - a) * t; }

}  // namespace

std::vector<FakeCell> makeFakeGrid(const FakeSpec& spec, const Rect& sample, int imageWidth, int imageHeight) {
    std::vector<FakeCell> cells;
    if (sample.empty() || imageWidth <= 0 || imageHeight <= 0) return cells;

    const double pitchX = std::max(1.0, spec.pitchFactorX * sample.w);
    const double pitchY = std::max(1.0, spec.pitchFactorY * sample.h);
    const Point2D center = sample.center();
    const Rect imageRect{0.0, 0.0, static_cast<double>(imageWidth), static_cast<double>(imageHeight)};

    // 排滿影像時，索引範圍取影像對角線除以間距再多一格（旋轉時格子會往對角線方向延伸）。
    const double diagonal = std::hypot(static_cast<double>(imageWidth), static_cast<double>(imageHeight));
    const int spanI = spec.halfCols > 0 ? spec.halfCols : static_cast<int>(std::ceil(diagonal / pitchX)) + 1;
    const int spanJ = spec.halfRows > 0 ? spec.halfRows : static_cast<int>(std::ceil(diagonal / pitchY)) + 1;

    for (int i = -spanI; i <= spanI; ++i) {
        for (int j = -spanJ; j <= spanJ; ++j) {
            const Point2D offset = rotate({i * pitchX, j * pitchY}, spec.gridAngleDeg);
            const Point2D ideal{center.x + offset.x, center.y + offset.y};
            if (!imageRect.contains(ideal)) continue;
            if (cells.size() >= kMaxCells) return cells;

            FakeCell cell;
            cell.i = i;
            cell.j = j;
            cell.pose = {ideal.x, ideal.y, normalizeAngleDeg(spec.gridAngleDeg)};
            const bool isSample = (i == 0 && j == 0);
            if (isSample) {
                cell.kind = FakeCellKind::Strong;
                cell.score = spec.strongScoreMax;
            } else {
                const double roll = unit(spec.seed, i, j, 0);
                if (roll < spec.missingFraction) {
                    cell.kind = FakeCellKind::Missing;
                } else if (roll < spec.missingFraction + spec.weakFraction) {
                    cell.kind = FakeCellKind::Weak;
                    cell.score = lerp(spec.weakScoreMin, spec.weakScoreMax, unit(spec.seed, i, j, 1));
                } else {
                    cell.kind = FakeCellKind::Strong;
                    cell.score = lerp(spec.strongScoreMin, spec.strongScoreMax, unit(spec.seed, i, j, 1));
                }
                if (cell.kind != FakeCellKind::Missing && spec.jitterPx > 0.0) {
                    cell.pose.x += (unit(spec.seed, i, j, 2) * 2.0 - 1.0) * spec.jitterPx;
                    cell.pose.y += (unit(spec.seed, i, j, 3) * 2.0 - 1.0) * spec.jitterPx;
                }
            }
            cells.push_back(cell);
        }
    }
    return cells;
}

}  // namespace aoi::fake
