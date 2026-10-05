#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

// domain 層：只用 C++ 標準庫（docs/architecture.md）。
//
// ===== 座標慣例（整個程式只有這一種；兩個 back end 的轉換由各自的 adapter 負責）=====
//
// 影像空間（單位：影像像素）
//   - 原點在影像「左上角像素的左上角」。像素 (i, j) 佔據 [i, i+1) × [j, j+1)，它的中心在 (i + 0.5, j + 0.5)。
//     這和 Qt 的場景座標一致（S02 的畫布：場景座標＝影像像素）。
//     OpenCV 把像素中心放在整數座標，HALCON 的 (Row, Column) 也是中心在整數、且 y 在前：adapter 要做 ±0.5 與軸順序的轉換。
//   - x 向右，y 向下。
//   - Rect 以「左上角＋寬高」表示；Pose2D 與 RectObj 以「中心」表示。
//
// 角度
//   - 單位是「度」，因為 UI、CAD 清單與搜尋參數都用度。
//   - 正方向是「在螢幕上看起來逆時針」（和 cv::getRotationMatrix2D 的正方向相同）。
//     因為 y 向下，把點 (x, y) 繞原點轉 θ 度：x' = x·cosθ + y·sinθ，y' = −x·sinθ + y·cosθ。
//     例：(1, 0) 轉 +90° 到 (0, −1)，也就是畫面的正上方。
//   - 正規化到 (−180, 180]。0° 的元件是「畫面上沒有旋轉」。
//   - HALCON 以弧度、並以自己的軸順序回報角度：S08 的 adapter 轉換，S09 的契約測試用旋轉過的合成影像鎖住這個慣例。
//     在那之前，HALCON 端的正負號**未驗證**。
//
// 物理單位
//   - 樣本庫存 µm，影像帶「µm／像素」的校正值（ImageView::umPerPixel，0 ＝ 未校正；ADR-006 §6）。
//     本檔的型別沒有單位；型別所在的檔案說明它是像素還是 µm（component.h 的欄位名稱以 Um 結尾）。
namespace aoi {

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct Size2D {
    double w = 0.0;
    double h = 0.0;
};

// 軸對齊矩形：左上角 (x, y)，寬 w、高 h。右邊與下邊是「不含」（半開區間）。
struct Rect {
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;

    double right() const noexcept { return x + w; }
    double bottom() const noexcept { return y + h; }
    Point2D center() const noexcept { return {x + w * 0.5, y + h * 0.5}; }
    Size2D size() const noexcept { return {w, h}; }
    double area() const noexcept { return w > 0.0 && h > 0.0 ? w * h : 0.0; }
    bool empty() const noexcept { return !(w > 0.0 && h > 0.0); }
    bool contains(Point2D p) const noexcept { return p.x >= x && p.x < right() && p.y >= y && p.y < bottom(); }
};

inline Rect rectFromCenter(Point2D center, Size2D size) noexcept {
    return {center.x - size.w * 0.5, center.y - size.h * 0.5, size.w, size.h};
}

inline Rect intersection(const Rect& a, const Rect& b) noexcept {
    const double left = std::max(a.x, b.x);
    const double top = std::max(a.y, b.y);
    const double right = std::min(a.right(), b.right());
    const double bottom = std::min(a.bottom(), b.bottom());
    if (right <= left || bottom <= top) return {};
    return {left, top, right - left, bottom - top};
}

inline double intersectionArea(const Rect& a, const Rect& b) noexcept { return intersection(a, b).area(); }

// 元件位姿：中心與角度。
struct Pose2D {
    double x = 0.0;
    double y = 0.0;
    double angleDeg = 0.0;

    Point2D position() const noexcept { return {x, y}; }
};

// 旋轉矩形（一個檢測框）：中心在 pose，寬高是「未旋轉時」的寬（沿 x）與高（沿 y）。
struct RectObj {
    Pose2D pose;
    double width = 0.0;
    double height = 0.0;
};

inline double distance(Point2D a, Point2D b) noexcept { return std::hypot(a.x - b.x, a.y - b.y); }

inline double degreesToRadians(double degrees) noexcept { return degrees * std::numbers::pi / 180.0; }
inline double radiansToDegrees(double radians) noexcept { return radians * 180.0 / std::numbers::pi; }

// 把角度正規化到 (−180, 180]。
inline double normalizeAngleDeg(double degrees) noexcept {
    double a = std::fmod(degrees, 360.0);
    if (a <= -180.0) a += 360.0;
    if (a > 180.0) a -= 360.0;
    return a;
}

// 依本檔的角度慣例，把向量 (x, y) 轉 degrees 度（螢幕上逆時針為正）。
inline Point2D rotate(Point2D v, double degrees) noexcept {
    const double r = degreesToRadians(degrees);
    const double c = std::cos(r);
    const double s = std::sin(r);
    return {v.x * c + v.y * s, -v.x * s + v.y * c};
}

// 旋轉矩形的外接軸對齊矩形。
inline Rect boundingRect(const RectObj& r) noexcept {
    const double rad = degreesToRadians(r.pose.angleDeg);
    const double c = std::abs(std::cos(rad));
    const double s = std::abs(std::sin(rad));
    const double w = r.width * c + r.height * s;
    const double h = r.width * s + r.height * c;
    return rectFromCenter(r.pose.position(), {w, h});
}

}  // namespace aoi
