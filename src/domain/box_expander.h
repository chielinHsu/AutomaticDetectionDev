#pragma once

#include <cstddef>
#include <map>
#include <vector>

#include "domain/component.h"
#include "domain/geometry.h"

// domain 層：只用 C++ 標準庫。純幾何，可用精確數字測試。
//
// 框不單獨儲存，而是從「元件類型的框群組」與「元件位姿」推導出來（docs/adr/006-sample-library-and-hierarchy.md）：
//   Body    一個框，中心在類型座標的原點（主體中心）。
//   Pin     rows × cols 的矩陣，以群組中心為中心、水平間距 pitchX、垂直間距 pitchY；skipped 的格子不產生框。
//   Window  連結到父群組（Body 或 Pin）：父群組的每一個框各有一個窗，中心相對父框中心偏移（以父框寬、高為單位），
//           大小是父框的比例。父框改，窗跟著變。
// 類型的座標是 µm、未旋轉、相對主體中心（component.h）；元件的位姿是影像像素。座標與角度慣例見 geometry.h。
namespace aoi {

// µm → 像素：影像帶校正值 umPerPixel（ImageView::umPerPixel，0 ＝ 未校正）。
// **暫定**：未校正時視為 1 µm ＝ 1 像素，只是為了讓沒有校正值的影像也能運作（S04 的暫時類型就是用這個換算的）。
// 真正的換算規則（要不要拒絕未校正的影像、怎麼提示）與校正值一起在 S06 定案。
constexpr double effectiveUmPerPixel(double umPerPixel) noexcept { return umPerPixel > 0.0 ? umPerPixel : 1.0; }

struct ExpandedBox {
    RectObj rect;  // 影像像素；寬高是未旋轉時的寬（沿 x）與高（沿 y），角度與元件相同
    BoxGroupKind kind = BoxGroupKind::Body;
    int groupId = 0;
};

namespace detail {

// 一個框在「類型座標」的位置：中心 (µm) 與大小 (µm)。
struct LocalBox {
    double cx = 0.0;
    double cy = 0.0;
    double w = 0.0;
    double h = 0.0;
};

inline std::vector<LocalBox> localBoxes(const BoxGroup& group) {
    std::vector<LocalBox> out;
    switch (group.kind) {
        case BoxGroupKind::Body:
            out.push_back({group.centerXUm, group.centerYUm, group.widthUm, group.heightUm});
            break;
        case BoxGroupKind::Pin: {
            const PinMatrix& m = group.matrix;
            const double x0 = -(m.cols - 1) * 0.5 * m.pitchXUm;
            const double y0 = -(m.rows - 1) * 0.5 * m.pitchYUm;
            for (int row = 0; row < m.rows; ++row) {
                for (int col = 0; col < m.cols; ++col) {
                    bool skipped = false;
                    for (const MatrixCell& cell : m.skipped) skipped = skipped || (cell.row == row && cell.col == col);
                    if (skipped) continue;
                    out.push_back({group.centerXUm + x0 + col * m.pitchXUm, group.centerYUm + y0 + row * m.pitchYUm,
                                   group.widthUm, group.heightUm});
                }
            }
            break;
        }
        case BoxGroupKind::Window:
            break;  // 由父群組推導，見 expandBoxes
    }
    return out;
}

}  // namespace detail

// 把類型的全部框放到 pose 上。順序：依 type.groups 的順序，群組內 Pin 依列再依欄、Window 依父框的順序。
// 找不到父群組、或父群組也是 Window 的 Window 不產生框（資料不合法時不丟例外，也不亂畫）。
inline std::vector<ExpandedBox> expandBoxes(const ComponentType& type, const Pose2D& pose, double umPerPixel) {
    const double pxPerUm = 1.0 / effectiveUmPerPixel(umPerPixel);

    // 先算非 Window 群組的框（Window 要用它們當父框）。
    std::map<int, std::vector<detail::LocalBox>> parents;
    for (const BoxGroup& g : type.groups) {
        if (g.kind != BoxGroupKind::Window) parents[g.id] = detail::localBoxes(g);
    }

    std::vector<ExpandedBox> out;
    auto place = [&](const detail::LocalBox& b, BoxGroupKind kind, int groupId) {
        const Point2D offset = rotate({b.cx * pxPerUm, b.cy * pxPerUm}, pose.angleDeg);
        ExpandedBox box;
        box.rect = RectObj{{pose.x + offset.x, pose.y + offset.y, pose.angleDeg}, b.w * pxPerUm, b.h * pxPerUm};
        box.kind = kind;
        box.groupId = groupId;
        out.push_back(box);
    };

    for (const BoxGroup& g : type.groups) {
        if (g.kind != BoxGroupKind::Window) {
            for (const detail::LocalBox& b : parents[g.id]) place(b, g.kind, g.id);
            continue;
        }
        const auto parent = parents.find(g.window.parentGroupId);
        if (parent == parents.end()) continue;
        for (const detail::LocalBox& p : parent->second) {
            const detail::LocalBox w{p.cx + g.window.offsetXRatio * p.w, p.cy + g.window.offsetYRatio * p.h,
                                     p.w * g.window.widthRatio, p.h * g.window.heightRatio};
            place(w, BoxGroupKind::Window, g.id);
        }
    }
    return out;
}

// 只要數量時用，不算幾何。
inline std::size_t countBoxes(const ComponentType& type) {
    std::map<int, std::size_t> parentCounts;
    for (const BoxGroup& g : type.groups) {
        if (g.kind != BoxGroupKind::Window) parentCounts[g.id] = detail::localBoxes(g).size();
    }
    std::size_t total = 0;
    for (const BoxGroup& g : type.groups) {
        if (g.kind != BoxGroupKind::Window) {
            total += parentCounts[g.id];
        } else {
            const auto parent = parentCounts.find(g.window.parentGroupId);
            if (parent != parentCounts.end()) total += parent->second;
        }
    }
    return total;
}

}  // namespace aoi
