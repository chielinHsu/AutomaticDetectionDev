//! 格子的連通（`cell_components.h`）：兩個格子的 Chebyshev 距離 ≤ link 就相連，只留成員最多的那一塊。

use crate::cpp::by_less;
use crate::fallible::{Res, vec_filled, vec_with_capacity};
use crate::flat_map::{FlatMap, pack_key};
use crate::lattice::basis::Cell;

/// 並查集。根是編號小的：結果與合併的順序無關。
struct DisjointSets {
    parent: Vec<usize>,
}

impl DisjointSets {
    fn new(n: usize) -> Res<Self> {
        let mut parent = vec_with_capacity(n)?;
        parent.extend(0..n);
        Ok(DisjointSets { parent })
    }

    fn find(&mut self, mut x: usize) -> usize {
        let mut root = x;
        while self.parent[root] != root {
            root = self.parent[root];
        }
        while self.parent[x] != root {
            let next = self.parent[x];
            self.parent[x] = root;
            x = next;
        }
        root
    }

    fn unite(&mut self, a: usize, b: usize) {
        let a = self.find(a);
        let b = self.find(b);
        if a != b {
            self.parent[a.max(b)] = a.min(b);
        }
    }
}

/// 成員最多的那一塊在 cells 裡的索引（由小到大）；一樣大時取逐列掃描先遇到的那一塊。
/// two_dimensional ＝ false 時只看同一列。空輸入回空。
///
/// 和 C++ 的差別只在「同一個格子出現兩次以上」的輸入：雜湊表那一條路依 (j, i) 排序時，C++ 的 `std::sort`
/// 不保證相等元素的順序，Rust 這裡用穩定排序（C++ 在 32 個以內用插入排序，也是穩定的）。估計器的候選每格只有一個點。
pub fn largest_component(cells: &[Cell], link: i32, two_dimensional: bool) -> Res<Vec<usize>> {
    let m = cells.len();
    if m == 0 {
        return Ok(Vec::new());
    }
    let link = link.max(1);
    let link_j = if two_dimensional { link } else { 0 };
    let (mut min_i, mut max_i, mut min_j, mut max_j) = (cells[0].i, cells[0].i, cells[0].j, cells[0].j);
    for c in cells {
        min_i = min_i.min(c.i);
        max_i = max_i.max(c.i);
        min_j = min_j.min(c.j);
        max_j = max_j.max(c.j);
    }
    let mut sets = DisjointSets::new(m)?;
    let mut label: Vec<i32> = vec_filled(m, -1)?; // 每個格子的標號（並查集的元素）
    let w = f64::from(max_i) - f64::from(min_i) + 1.0 + 2.0 * f64::from(link);
    let h = f64::from(max_j) - f64::from(min_j) + 1.0 + 2.0 * f64::from(link_j);
    if w * h <= 64.0 * m as f64 + 4096.0 {
        let width = w as isize;
        let height = h as isize;
        let mut grid: Vec<i32> = vec_filled((w * h) as usize, -1)?;
        for (s, c) in cells.iter().enumerate() {
            let row = c.j.wrapping_sub(min_j).wrapping_add(link_j) as isize;
            let col = c.i.wrapping_sub(min_i).wrapping_add(link) as isize;
            grid[(row * width + col) as usize] = s as i32;
        }
        let mut next: i32 = 0;
        let (link_x, link_y) = (link as isize, link_j as isize);
        for y in link_y..height - link_y {
            for x in link_x..width - link_x {
                let s = grid[(y * width + x) as usize];
                if s < 0 {
                    continue;
                }
                let mut mine: i32 = -1;
                for dj in -link_y..=0 {
                    let row = (y + dj) * width + x;
                    let last = if dj == 0 { -1 } else { link_x }; // 同一列只看左邊
                    for di in -link_x..=last {
                        let other = grid[(row + di) as usize];
                        if other < 0 {
                            continue;
                        }
                        let theirs = label[other as usize];
                        if mine < 0 {
                            mine = theirs;
                        } else if theirs != mine {
                            sets.unite(theirs as usize, mine as usize);
                        }
                    }
                }
                label[s as usize] = if mine >= 0 {
                    mine
                } else {
                    next += 1;
                    next - 1
                };
            }
        }
    } else {
        let mut occupied: FlatMap<i32> = FlatMap::new(m)?;
        for (s, c) in cells.iter().enumerate() {
            *occupied.entry(pack_key(c.i, c.j))? = s as i32 + 1;
        }
        // 標號 ＝ 依 (j, i) 排序後的名次：一樣大時「先掃到的」與密集陣列的做法一致。
        let mut order: Vec<usize> = vec_with_capacity(m)?;
        order.extend(0..m);
        order.sort_by(by_less(|&a: &usize, &b: &usize| {
            if cells[a].j != cells[b].j {
                cells[a].j < cells[b].j
            } else {
                cells[a].i < cells[b].i
            }
        }));
        for (rank, &s) in order.iter().enumerate() {
            label[s] = rank as i32;
        }
        for (s, c) in cells.iter().enumerate() {
            for dj in 0..=link_j {
                let first = if dj == 0 { 1 } else { -link };
                for di in first..=link {
                    if let Some(&other) = occupied.find(pack_key(c.i.wrapping_add(di), c.j.wrapping_add(dj))) {
                        sets.unite(label[s] as usize, label[(other - 1) as usize] as usize);
                    }
                }
            }
        }
    }
    let mut size: Vec<usize> = vec_filled(m, 0)?;
    for s in 0..m {
        let root = sets.find(label[s] as usize);
        size[root] += 1;
    }
    // std::max_element：第一個最大的。
    let mut keep = 0usize;
    for (k, &v) in size.iter().enumerate() {
        if v > size[keep] {
            keep = k;
        }
    }
    let mut out: Vec<usize> = vec_with_capacity(size[keep])?;
    for s in 0..m {
        if sets.find(label[s] as usize) == keep {
            out.push(s);
        }
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn c(i: i32, j: i32) -> Cell {
        Cell { i, j }
    }

    #[test]
    fn splits_at_gaps_wider_than_link() {
        let cells = [c(0, 0), c(1, 0), c(2, 0), c(7, 0), c(8, 0)];
        assert_eq!(largest_component(&cells, 3, false).unwrap(), vec![0, 1, 2]);
        assert_eq!(largest_component(&[c(0, 0), c(4, -4)], 3, true).unwrap().len(), 1);
        let far = [c(0, 0), c(1, 0), c(1_000_000, 5), c(1_000_001, 5), c(1_000_002, 5)];
        assert_eq!(largest_component(&far, 3, true).unwrap(), vec![2, 3, 4]);
    }
}
