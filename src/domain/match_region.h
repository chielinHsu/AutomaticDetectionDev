#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "domain/geometry.h"

// domain 層：只用標準庫。樣本的比對區（ADR-026）。
//
// 比對區 ＝ 建範本用的區域（主體框內的矩形或遮罩）；CAD 框（主體框與它推導出的檢測框）不變。範本記住比對區與主體中心的
// 相對位置（origin），搜尋器回報的位姿一律是**主體中心**（ITemplateBuilder.h 的契約），所以 use case 之後的每一層都不必知道
// 比對區。沒有設定比對區 ＝ 整個主體框（和 ADR-026 之前完全相同）。
//
// 兩種座標：
//  - 存在類型上（MatchSettings）：µm、相對主體中心、未旋轉，和 BoxGroup 同一套慣例（component.h）。
//  - 交給 back end（MatchSpec）：影像像素（絕對座標），遮罩已取樣到像素格，adapter 不必自己換算。
// 限制（P1）：樣本的角度一律是 0（InspectionDocument::addSample），所以換算不處理旋轉。
namespace aoi {

// 比對的特徵。Intensity：灰階（現在的做法）。Edges：梯度大小（OpenCV 在 Sobel 的大小上做 NCC；
// HALCON 的 shape model 本來就是邊緣，兩種模式一樣，ADR-004 D7）。
enum class MatchMode { Intensity, Edges };

// 影像像素格上的遮罩：左上角 (x, y)、寬 width、高 height；bits 逐列排，1 ＝ 參與比對。
struct PixelMask {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bits;

    bool empty() const noexcept {
        return width <= 0 || height <= 0 || bits.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }
    std::size_t count() const noexcept;
    // (px, py) 是影像像素的整數座標；在遮罩外回 false。
    bool at(int px, int py) const noexcept {
        if (empty() || px < x || py < y || px >= x + width || py >= y + height) return false;
        return bits[static_cast<std::size_t>(py - y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(px - x)] != 0;
    }
    Rect bounds() const noexcept {
        return {static_cast<double>(x), static_cast<double>(y), static_cast<double>(width), static_cast<double>(height)};
    }
};

// 交給 ITemplateBuilder 的比對規格（影像像素）。
struct MatchSpec {
    std::optional<Rect> rect;  // nullopt ＝ 整個主體框
    PixelMask mask;            // 空 ＝ rect 整塊；不空時 rect 等於遮罩的外接矩形（mask.bounds()）
    MatchMode mode = MatchMode::Intensity;

    bool isDefault() const noexcept { return !rect && mask.empty() && mode == MatchMode::Intensity; }
};

// 一個樣本：主體框（影像像素，CAD 框）＋比對規格。
struct SampleSpec {
    Rect body;
    MatchSpec match;
};

// ---- 存在類型上的設定（µm，相對主體中心）----

// 遮罩的一段：第 row 列的 [colBegin, colEnd)。
struct MaskRun {
    int row = 0;
    int colBegin = 0;
    int colEnd = 0;
};

// 遮罩：width × height 的格子均勻蓋住 MatchRegion 的矩形；runs 依 (row, colBegin) 排序、同一列不重疊。
struct MatchMask {
    int width = 0;
    int height = 0;
    std::vector<MaskRun> runs;

    bool empty() const noexcept { return width <= 0 || height <= 0 || runs.empty(); }
};

struct MatchRegion {
    double centerXUm = 0.0;  // 比對區中心相對主體中心
    double centerYUm = 0.0;
    double widthUm = 0.0;
    double heightUm = 0.0;
    MatchMask mask;  // 空 ＝ 矩形整塊
};

struct MatchSettings {
    std::optional<MatchRegion> region;  // nullopt ＝ 整個主體框
    MatchMode mode = MatchMode::Intensity;

    bool isDefault() const noexcept { return !region && mode == MatchMode::Intensity; }
};

bool operator==(const MaskRun& a, const MaskRun& b) noexcept;
bool operator==(const MatchMask& a, const MatchMask& b) noexcept;
bool operator==(const MatchRegion& a, const MatchRegion& b) noexcept;
bool operator==(const MatchSettings& a, const MatchSettings& b) noexcept;

// 類型上的設定 → 影像像素的規格（主體中心在 bodyCenterPx；umPerPixel 要是有效值，見 box_expander.h 的 effectiveUmPerPixel）。
// 遮罩用最近鄰取樣到像素格（格子的外接矩形四捨五入到整數像素）。
MatchSpec matchSpecFor(const MatchSettings& settings, Point2D bodyCenterPx, double umPerPixel);

// 影像像素的比對區 → 類型上的設定。mask 不是空指標而且不空時，比對區 ＝ 遮罩的外接矩形（regionPx 不用）。
MatchSettings matchSettingsFromPixels(const Rect& regionPx, const PixelMask* mask, MatchMode mode, Point2D bodyCenterPx,
                                      double umPerPixel);

// 遮罩 ⇄ runs（格子的大小就是遮罩的寬高）。
MatchMask runsFromMask(const PixelMask& mask);
// runs 不合法（超出格子、重疊）時忽略那一段。
PixelMask maskFromRuns(const MatchMask& runs, int x, int y);

}  // namespace aoi
