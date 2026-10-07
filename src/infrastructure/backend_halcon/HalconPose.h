#pragma once

#include <cmath>

#include "domain/geometry.h"

// infrastructure/backend_halcon：HALCON 的位姿 ⇄ ADR-010（S08）。純函式，沒有 HALCON 的型別。
//
// 座標：HALCON 的 (Row, Column) 是 y 在前、像素中心在整數；ADR-010 是 (x, y)、像素中心在 +0.5。
//
// find_shape_model 的 Row、Column（HALCON 的說明，operators_en_US.ref）：它們「不完全是」模型參考點的位置，
// 而是調過的值，讓 vector_angle_to_rigid(0, 0, 0, Row, Column, Angle) 配 affine_trans_pixel 能把模型放對。
// affine_trans_pixel(H, r, c) ＝ affine_trans_point_2d(H, r ＋ 0.5, c ＋ 0.5) − 0.5（說明裡的公式），
// 所以參考點（模型座標的 (0, 0)）在搜尋影像裡真正的位置是
//     (Row, Column) ＋ R(Angle)·(0.5, 0.5) − (0.5, 0.5)            （HALCON 的像素座標）
// R 是 hom_mat2d_rotate 的旋轉矩陣 [[cos, −sin], [sin, cos]]，作用在 (row, column) 上。換成 ADR-010：
//     x ＝ Column ＋ 0.5·(sin ＋ cos)，  y ＝ Row ＋ 0.5·(cos − sin)
// 角度 0 時就是單純的 ＋0.5；30° 時和單純 ＋0.5 差 0.37 像素（tst_halcon_backend 的寬角度測試量得出來）。
//
// 角度：hom_mat2d_rotate 的 R 作用在 (row, column) 上，展開是 y' ＝ cos·y − sin·x、x' ＝ sin·y ＋ cos·x，
// 和 geometry.h 的 rotate()（螢幕上逆時針為正）完全一樣，所以只換單位（弧度 → 度），不改正負號。
// 這兩件事在 ADR-010 是「未驗證」；S08 用傾斜的合成板子（±3°、±4.5°、±30°）實測並以測試鎖住。
namespace aoi::infra {

// find_shape_model 的一個結果 → 參考點的位姿（ADR-010）。originX、originY：HALCON 影像的像素 (0, 0) 在原影像的位置。
inline Pose2D poseFromHalcon(double row, double column, double angleRad, double originX = 0.0, double originY = 0.0) noexcept {
    const double c = std::cos(angleRad);
    const double s = std::sin(angleRad);
    return {column + 0.5 * (s + c) + originX, row + 0.5 * (c - s) + originY, radiansToDegrees(angleRad)};
}

// 沒有考慮旋轉的換算（只有 ＋0.5）。只給測試用：證明上面的修正是必要的。
inline Pose2D naivePoseFromHalcon(double row, double column, double angleRad, double originX = 0.0, double originY = 0.0) noexcept {
    return {column + 0.5 + originX, row + 0.5 + originY, radiansToDegrees(angleRad)};
}

// 一個整數像素矩形（左上角 x0、y0，寬 w、高 h）在 HALCON 裡的 gen_rectangle1 參數（包含兩端的列與行）。
struct HalconRect1 {
    double row1 = 0.0;
    double column1 = 0.0;
    double row2 = 0.0;
    double column2 = 0.0;
};
inline HalconRect1 halconRect1(int x0, int y0, int w, int h) noexcept {
    return {static_cast<double>(y0), static_cast<double>(x0), static_cast<double>(y0 + h - 1), static_cast<double>(x0 + w - 1)};
}

}  // namespace aoi::infra
