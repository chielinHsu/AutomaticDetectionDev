#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/geometry.h"
#include "domain/image_view.h"
#include "domain/match_region.h"
#include "domain/object_state.h"

// domain 層：只用 C++ 標準庫。
//
// 資料模型：Board › Component › Box（docs/adr/006-sample-library-and-hierarchy.md）
//
//   Board            影像、名稱、元件實例清單，以及它用到的每個類型的「複本」
//    └─ ComponentInstance   一筆 CAD 記錄：名稱、位姿、正反面、類型參照、狀態與分數、來源
//        └─ Box             不單獨儲存；由「類型的框群組」與「元件位姿」推導出來（S04 的 BoxExpander）
//   ComponentType    樣本庫條目：料號、別名、類別、參考影像、框群組
//    └─ BoxGroup     Body（每個類型恰好一個）｜Pin（矩陣）｜Window（連結到父群組，用父框的比例）
//
// 單位：ComponentType 與 BoxGroup 的長度一律是 µm（欄位名稱以 Um 結尾），座標相對於**主體中心**；
// ComponentInstance 的位姿是**影像像素**（板子影像帶校正值 ImageView::umPerPixel，換算由使用者端負責）。
// 角度與軸向的慣例見 geometry.h。
namespace aoi {

enum class ComponentCategory { Chip, IC, Other };
enum class ComponentSide { Top, Bottom };
enum class BoxGroupKind { Body, Pin, Window };

// Pin 矩陣中的一格（略過的格子不產生框）。
struct MatrixCell {
    int row = 0;
    int col = 0;
};

struct PinMatrix {
    int rows = 1;
    int cols = 1;
    double pitchXUm = 0.0;  // 欄與欄之間（水平）的中心距
    double pitchYUm = 0.0;  // 列與列之間（垂直）的中心距
    std::vector<MatrixCell> skipped;
};

// 檢測窗：連結到父群組，位置與大小用「父框」的比例表示（父框改，窗跟著變）。
struct WindowSpec {
    int parentGroupId = 0;
    double offsetXRatio = 0.0;  // 窗中心相對父框中心，以父框寬為單位
    double offsetYRatio = 0.0;  // 同上，以父框高為單位
    double widthRatio = 0.8;
    double heightRatio = 0.6;
    // 演算法標籤（Void、Solder、Color…）。先是標籤；要不要加參數，交給使用者決定（ADR-006 未決 #6）。
    std::string algo;
};

struct BoxGroup {
    int id = 0;  // 在同一個類型內唯一
    BoxGroupKind kind = BoxGroupKind::Body;
    // Body：主體大小。Pin：單一腳位框的大小。Window：不用（由父框的比例推導）。
    double widthUm = 0.0;
    double heightUm = 0.0;
    // 群組中心相對主體中心。Body 固定在 (0, 0)；Pin 是整個矩陣的中心。
    double centerXUm = 0.0;
    double centerYUm = 0.0;
    PinMatrix matrix;   // 只有 Pin 用
    WindowSpec window;  // 只有 Window 用
};

// 元件類型。樣本庫裡是母版；板子保存自己的複本，並以 libId 記得它從哪個庫條目來（ADR-006 §5）。
struct ComponentType {
    std::string id;     // 在所屬的庫或板子內唯一
    std::string libId;  // 來源的庫條目；空字串 ＝ 只在這塊板子上
    std::string partNumber;
    std::vector<std::string> aliases;
    std::string name;
    ComponentCategory category = ComponentCategory::Other;
    std::vector<BoxGroup> groups;
    ImageView referenceImage;  // 空的（無效）＝ 沒有參考影像
    // 怎麼認出這個類型（ADR-028）：建範本用的比對區與特徵。預設 ＝ 整個主體框、灰階（和之前相同）。
    // 不是框群組：它不產生檢測框（expandBoxes 不看它）。樣本庫的比對器（流程 B）還不讀它。
    MatchSettings match;
};

// 板上的一個元件（一筆 CAD 記錄）。
struct ComponentInstance {
    int id = 0;
    std::string name;  // R12、U1…；接受提案時由「開頭字元＋流水號」指派（ux-spec §12.1 第 8 項）
    Pose2D pose;       // 影像像素
    ComponentSide side = ComponentSide::Top;
    std::string typeId;  // 指向 Board::types
    ObjectState state = ObjectState::Found;
    std::optional<double> score;  // 手動補的沒有
    ObjectOrigin origin = ObjectOrigin::Auto;
};

struct Board {
    std::string name;
    int imageWidth = 0;
    int imageHeight = 0;
    double umPerPixel = 0.0;  // 0 ＝ 未校正
    // 影像的有效位元數（S07，給 16 bit 灰階）：0 ＝ 看最大值推測；8..16 ＝ 使用者或配方指定（相機的設定，例如 12 bit 相機）。
    // 推測對暗的影像會錯（domain/image_depth.h 的 decideSignificantBits）。換影像時沿用（使用者 2026-10-06 決定）。
    int significantBits = 0;
    std::vector<ComponentType> types;  // 板子自己的複本
    std::vector<ComponentInstance> components;
};

}  // namespace aoi
