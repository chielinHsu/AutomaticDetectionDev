//! 座標與角度慣例（和 `src/domain/geometry.h` 相同，ADR-010）：影像像素、y 向下、角度為度、螢幕上逆時針為正。

/// 點或向量（像素）。`#[repr(C)]`：和 C++ 的 `Point2D`、C 的 `AoiRsPoint` 相同，直接當成 FFI 的結構。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Point {
    pub x: f64,
    pub y: f64,
}

/// 旋轉矩形（`RectObj`）：中心 (x, y)、角度（度）、未旋轉時的寬（沿 x）與高（沿 y）。
/// `#[repr(C)]`：欄位順序和 C++ 的 `RectObj`（`Pose2D{x, y, angleDeg}`、`width`、`height`）相同，直接當成 FFI 的結構。
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct RectObj {
    pub x: f64,
    pub y: f64,
    pub angle_deg: f64,
    pub width: f64,
    pub height: f64,
}

/// `degreesToRadians`：運算順序和 C++ 相同（先乘 π 再除 180），結果才會一位不差。
pub fn degrees_to_radians(degrees: f64) -> f64 {
    degrees * std::f64::consts::PI / 180.0
}

/// `rotate`：把向量轉 `degrees` 度（螢幕上逆時針為正）。
pub fn rotate(v: Point, degrees: f64) -> Point {
    let r = degrees_to_radians(degrees);
    let c = r.cos();
    let s = r.sin();
    Point {
        x: v.x * c + v.y * s,
        y: -v.x * s + v.y * c,
    }
}
