#include "domain/lattice/cell_components.h"

#include <algorithm>
#include <cstdint>
#include <numeric>

#include "domain/lattice/flat_map.h"

namespace aoi::detail {

namespace {

// 並查集。根是編號小的：結果與合併的順序無關。
class DisjointSets {
public:
    explicit DisjointSets(std::size_t n) : parent_(n) { std::iota(parent_.begin(), parent_.end(), std::size_t{0}); }
    std::size_t find(std::size_t x) {
        std::size_t root = x;
        while (parent_[root] != root) root = parent_[root];
        while (parent_[x] != root) {
            const std::size_t next = parent_[x];
            parent_[x] = root;
            x = next;
        }
        return root;
    }
    void unite(std::size_t a, std::size_t b) {
        a = find(a);
        b = find(b);
        if (a != b) parent_[std::max(a, b)] = std::min(a, b);
    }

private:
    std::vector<std::size_t> parent_;
};

}  // namespace

// 做法：索引範圍不大時（通常如此），在加了邊框的密集陣列上逐列掃描：每個格子只看「前面半個」鄰域裡已經有標號的
// 格子，沿用其中一個標號；不同的標號相鄰時記到並查集（密集的陣列很少發生）。每格看 (2·link＋1)·link＋link 個鄰居。
// 範圍很大時（有離得很遠的點）用雜湊表：每個格子看「往後半個」鄰域，相鄰就合併。
std::vector<std::size_t> largestComponent(const std::vector<LatticeIndex>& cells, int link, bool twoDimensional) {
    const std::size_t m = cells.size();
    std::vector<std::size_t> out;
    if (m == 0) return out;
    link = std::max(1, link);
    const int linkJ = twoDimensional ? link : 0;
    int minI = cells[0].i, maxI = minI, minJ = cells[0].j, maxJ = minJ;
    for (const LatticeIndex& cell : cells) {
        minI = std::min(minI, cell.i);
        maxI = std::max(maxI, cell.i);
        minJ = std::min(minJ, cell.j);
        maxJ = std::max(maxJ, cell.j);
    }
    DisjointSets sets(m);
    std::vector<std::int32_t> label(m, -1);  // 每個格子的標號（並查集的元素）
    const double w = static_cast<double>(maxI) - minI + 1.0 + 2.0 * link;
    const double h = static_cast<double>(maxJ) - minJ + 1.0 + 2.0 * linkJ;
    if (w * h <= 64.0 * static_cast<double>(m) + 4096.0) {
        const auto width = static_cast<std::ptrdiff_t>(w);
        const auto height = static_cast<std::ptrdiff_t>(h);
        std::vector<std::int32_t> grid(static_cast<std::size_t>(w * h), -1);
        for (std::size_t s = 0; s < m; ++s) {
            const std::ptrdiff_t at = static_cast<std::ptrdiff_t>(cells[s].j - minJ + linkJ) * width + (cells[s].i - minI + link);
            grid[static_cast<std::size_t>(at)] = static_cast<std::int32_t>(s);
        }
        std::int32_t next = 0;
        for (std::ptrdiff_t y = linkJ; y < height - linkJ; ++y) {
            for (std::ptrdiff_t x = link; x < width - link; ++x) {
                const std::int32_t s = grid[static_cast<std::size_t>(y * width + x)];
                if (s < 0) continue;
                std::int32_t mine = -1;
                for (int dj = -linkJ; dj <= 0; ++dj) {
                    const std::ptrdiff_t row = (y + dj) * width + x;
                    const int last = dj == 0 ? -1 : link;  // 同一列只看左邊
                    for (int di = -link; di <= last; ++di) {
                        const std::int32_t other = grid[static_cast<std::size_t>(row + di)];
                        if (other < 0) continue;
                        const std::int32_t theirs = label[static_cast<std::size_t>(other)];
                        if (mine < 0) mine = theirs;
                        else if (theirs != mine) sets.unite(static_cast<std::size_t>(theirs), static_cast<std::size_t>(mine));
                    }
                }
                label[static_cast<std::size_t>(s)] = mine >= 0 ? mine : next++;
            }
        }
    } else {
        FlatMap<std::int32_t> occupied(m);
        for (std::size_t s = 0; s < m; ++s) occupied[packKey(cells[s].i, cells[s].j)] = static_cast<std::int32_t>(s) + 1;
        // 標號 ＝ 依 (j, i) 排序後的名次：一樣大時「先掃到的」與密集陣列的做法一致。
        std::vector<std::size_t> order(m);
        std::iota(order.begin(), order.end(), std::size_t{0});
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return cells[a].j != cells[b].j ? cells[a].j < cells[b].j : cells[a].i < cells[b].i;
        });
        for (std::size_t rank = 0; rank < m; ++rank) label[order[rank]] = static_cast<std::int32_t>(rank);
        for (std::size_t s = 0; s < m; ++s) {
            for (int dj = 0; dj <= linkJ; ++dj) {
                for (int di = (dj == 0 ? 1 : -link); di <= link; ++di) {
                    const std::int32_t* other = occupied.find(packKey(cells[s].i + di, cells[s].j + dj));
                    if (other != nullptr)
                        sets.unite(static_cast<std::size_t>(label[s]), static_cast<std::size_t>(label[static_cast<std::size_t>(*other - 1)]));
                }
            }
        }
    }
    std::vector<std::size_t> size(m, 0);
    for (std::size_t s = 0; s < m; ++s) ++size[sets.find(static_cast<std::size_t>(label[s]))];
    const std::size_t keep = static_cast<std::size_t>(std::max_element(size.begin(), size.end()) - size.begin());
    out.reserve(size[keep]);
    for (std::size_t s = 0; s < m; ++s) {
        if (sets.find(static_cast<std::size_t>(label[s])) == keep) out.push_back(s);
    }
    return out;
}

}  // namespace aoi::detail
