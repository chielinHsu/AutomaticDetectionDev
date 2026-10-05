#pragma once

#include <QString>
#include <array>
#include <memory>
#include <vector>

#include "domain/component.h"
#include "presentation/canvas/OverlayTypes.h"

namespace aoi::ui {

// 提案物件在畫布與物件樹裡的編號 ＝ 提案編號 ＋ 這個值。文件裡的元件編號從 1 起，兩者不會相撞，
// 所以畫布的選取、物件樹的列、工具回報的點擊都只用一個整數，Presenter 再依編號範圍轉回提案或文件。
inline constexpr int kProposalIdBase = 1'000'000;

constexpr bool isProposalId(int sceneId) noexcept { return sceneId >= kProposalIdBase; }

// 物件樹的一列＝一個元件（Board › Component）。框群組是它的子節點，由 ObjectTypeRow 提供。
struct ObjectRow {
    int id = 0;                    // 畫布編號（提案物件已加上 kProposalIdBase）
    QString name;                  // R12、X3…；提案物件還沒有名稱，顯示 #編號
    OverlayState state = OverlayState::Found;
    double score = -1.0;           // < 0 ＝ 沒有分數（手動補的、推算的）
    bool manual = false;
    int typeIndex = -1;            // 指向 ObjectRows::types；-1 ＝ 沒有框群組資訊
};

struct ObjectGroupRow {
    BoxGroupKind kind = BoxGroupKind::Body;
    int boxCount = 0;
};

struct ObjectTypeRow {
    QString label;                      // 料號；暫時類型沒有料號時是空字串
    std::vector<ObjectGroupRow> groups;
};

// Presenter 交給物件樹的資料：不可變的快照，由 UI 執行緒建立、UI 執行緒讀。
struct ObjectRows {
    QString boardTitle;
    bool proposal = false;  // true ＝ 這些是尚未接受的提案物件
    std::vector<ObjectRow> rows;
    std::vector<ObjectTypeRow> types;
    std::array<int, kOverlayStateCount> stateCounts{};  // 依 OverlayState 的順序
};

using ObjectRowsPtr = std::shared_ptr<const ObjectRows>;

// 物件樹的狀態篩選：位元遮罩，第 stateIndex(s) 位 ＝ 顯示該狀態。
using StateFilter = unsigned;
inline constexpr StateFilter kAllStates = (1u << kOverlayStateCount) - 1u;
constexpr StateFilter stateBit(OverlayState s) noexcept { return 1u << stateIndex(s); }

}  // namespace aoi::ui
