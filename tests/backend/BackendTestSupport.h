#pragma once

#include <QString>
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "domain/geometry.h"
#include "domain/match.h"
#include "synthetic_board.h"

// tests/backend 共用（S07）：合成板子的樣本框、與標準答案比對（召回率 T6a、位置誤差 T6b）。
namespace aoi::test {

// 使用者框樣本的樣子：元件主體外加 margin 像素（範本含一圈背景）。用第 index 個「有元件」的格子。
inline Rect sampleBoxAround(const SyntheticBoard& board, std::size_t index = 0, double margin = 4.0) {
    std::size_t seen = 0;
    for (const SyntheticPart& p : board.parts) {
        if (!p.present) continue;
        if (seen++ != index) continue;
        const RectObj body{p.pose, p.widthPx + 2.0 * margin, p.heightPx + 2.0 * margin};
        const Rect r = boundingRect(body);
        return {std::round(r.x), std::round(r.y), std::round(r.w), std::round(r.h)};
    }
    return {};
}

// 板子中間那個有元件的格子（樣本離四邊都遠：晶格與視窗驗證的典型用法）。
inline std::size_t middlePresentIndex(const SyntheticBoard& board) {
    std::size_t index = 0;
    std::size_t best = 0;
    double bestDistance = std::numeric_limits<double>::max();
    const Point2D center{board.spec.width * 0.5, board.spec.height * 0.5};
    for (const SyntheticPart& p : board.parts) {
        if (!p.present) continue;
        const double d = distance(p.pose.position(), center);
        if (d < bestDistance) {
            bestDistance = d;
            best = index;
        }
        ++index;
    }
    return best;
}

struct Accuracy {
    std::size_t present = 0;       // 標準答案裡有元件的格子
    std::size_t matched = 0;       // 其中被找到的（最近的匹配在 tolerance 內）
    std::size_t falsePositives = 0;  // 匹配附近沒有任何有元件的格子
    double meanErrorPx = 0.0;      // 找到的元件，中心的平均誤差
    double maxErrorPx = 0.0;
    double maxAngleErrorDeg = 0.0; // 和 expectedAngle 比
    double recall() const { return present == 0 ? 1.0 : static_cast<double>(matched) / static_cast<double>(present); }
    QString describe() const {
        return QStringLiteral("present %1, matched %2, false %3, mean %4 px, max %5 px, angle max %6 deg")
            .arg(present).arg(matched).arg(falsePositives).arg(meanErrorPx, 0, 'f', 3).arg(maxErrorPx, 0, 'f', 3)
            .arg(maxAngleErrorDeg, 0, 'f', 3);
    }
};

// 每個有元件的格子找最近的位姿；距離 ≤ tolerance 算找到。expectedAngle：匹配的角度應該是多少（相對樣本）。
inline Accuracy compareWithTruth(const SyntheticBoard& board, const std::vector<Pose2D>& found, double expectedAngle,
                                 double tolerance = 3.0) {
    Accuracy a;
    double sum = 0.0;
    for (const SyntheticPart& p : board.parts) {
        if (!p.present) continue;
        ++a.present;
        double best = std::numeric_limits<double>::max();
        const Pose2D* nearest = nullptr;
        for (const Pose2D& f : found) {
            const double d = distance(f.position(), p.pose.position());
            if (d < best) {
                best = d;
                nearest = &f;
            }
        }
        if (nearest == nullptr || best > tolerance) continue;
        ++a.matched;
        sum += best;
        a.maxErrorPx = std::max(a.maxErrorPx, best);
        a.maxAngleErrorDeg = std::max(a.maxAngleErrorDeg, std::abs(nearest->angleDeg - expectedAngle));
    }
    for (const Pose2D& f : found) {
        const bool nearPart = std::any_of(board.parts.begin(), board.parts.end(), [&](const SyntheticPart& p) {
            return p.present && distance(f.position(), p.pose.position()) <= tolerance;
        });
        if (!nearPart) ++a.falsePositives;
    }
    a.meanErrorPx = a.matched == 0 ? 0.0 : sum / static_cast<double>(a.matched);
    return a;
}

inline std::vector<Pose2D> posesOf(const std::vector<Match>& matches) {
    std::vector<Pose2D> out;
    out.reserve(matches.size());
    for (const Match& m : matches) out.push_back(m.pose);
    return out;
}

// 契約的排序：分數由高到低，同分依 y、再依 x。
inline bool sortedByContract(const std::vector<Match>& m) {
    for (std::size_t i = 1; i < m.size(); ++i) {
        const Match& a = m[i - 1];
        const Match& b = m[i];
        if (a.score < b.score) return false;
        if (a.score == b.score && (a.pose.y > b.pose.y || (a.pose.y == b.pose.y && a.pose.x > b.pose.x))) return false;
    }
    return true;
}

}  // namespace aoi::test
