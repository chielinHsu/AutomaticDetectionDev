#pragma once

#include <QBrush>
#include <QColor>
#include <QPen>
#include <array>

#include "presentation/canvas/OverlayTypes.h"
#include "presentation/theme/ThemeTokens.h"

namespace aoi::ui {

// 一條線畫兩次：底下較寬的「外框」（casing，對比色）加上上面的描邊，讓框在任何影像上都看得到。
struct PenPair {
    QPen stroke;
    QPen casing;
};

// 疊圖的畫筆與色彩。畫筆全部是 cosmetic（不隨縮放變粗）。
// 實測（本機 Qt 6.8.0）：cosmetic 筆的寬度單位是「裝置像素」，在 devicePixelRatio = 2 時寬 2 只有 2 個裝置像素，
// 所以裝置無關的寬度要乘 devicePixelRatio，這個類別負責做這件事。
class OverlayStyle {
public:
    explicit OverlayStyle(const ThemeTokens& tokens, qreal devicePixelRatio = 1.0);

    const ThemeTokens& tokens() const { return tokens_; }
    qreal devicePixelRatio() const { return dpr_; }
    void setDevicePixelRatio(qreal dpr);  // 不同時才重建

    const PenPair& box(OverlayState state, BoxKind kind, bool selected) const {
        return boxPens_[index(state, kind, selected)];
    }
    // 實線狀態用 BoxFrames 的整數像素填色畫法（快）；虛線與點線狀態沒辦法，仍用描邊。
    bool solid(OverlayState state) const { return tokens_.line[static_cast<size_t>(stateIndex(state))].dashPx.isEmpty(); }
    int frameThickness(OverlayState state, BoxKind kind, bool selected) const;
    const StrokeColors& colors(OverlayState state) const { return tokens_.overlay.state[static_cast<size_t>(stateIndex(state))]; }

    // 板上的選取外框、類型編輯器選取、把手：用目前主題的強調色。
    void setAccent(const QColor& accent);
    const PenPair& accentLine() const { return accentLine_; }  // 1 px
    const QColor& accent() const { return accent_; }

    // 流程 B 拖出的區域：白色細線＋淡填色，不是狀態框。
    const PenPair& region() const { return region_; }
    const QBrush& regionFill() const { return regionFill_; }
    // 類型編輯器的定義框：中性白色。
    const PenPair& definition(BoxKind kind) const { return definition_[static_cast<size_t>(kindIndex(kind))]; }
    const QBrush& definitionPinFill() const { return definitionPinFill_; }

    // 純函式：測試直接驗證。
    static double strokeWidth(const ThemeTokens& t, OverlayState state, BoxKind kind, bool selected);
    static QPen makeBoxPen(const ThemeTokens& t, OverlayState state, BoxKind kind, bool selected, bool casing,
                           qreal dpr);

private:
    static constexpr int index(OverlayState s, BoxKind k, bool selected) {
        return (stateIndex(s) * kBoxKindCount + kindIndex(k)) * 2 + (selected ? 1 : 0);
    }
    void rebuild();

    const ThemeTokens& tokens_;
    qreal dpr_;
    QColor accent_;
    std::array<PenPair, kOverlayStateCount * kBoxKindCount * 2> boxPens_;
    PenPair accentLine_;
    PenPair region_;
    QBrush regionFill_;
    std::array<PenPair, kBoxKindCount> definition_;
    QBrush definitionPinFill_;
};

}  // namespace aoi::ui
