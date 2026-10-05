#include "presentation/canvas/OverlayStyle.h"

#include "presentation/canvas/BoxFrames.h"

#include <QList>
#include <algorithm>
#include <cmath>

namespace aoi::ui {
namespace {

QPen cosmeticPen(const QColor& color, double widthPx, qreal dpr) {
    QPen pen(color);
    pen.setCosmetic(true);
    pen.setWidthF(widthPx * dpr);
    pen.setJoinStyle(Qt::MiterJoin);
    return pen;
}

PenPair solidPair(const QColor& stroke, const QColor& casing, double widthPx, double casingExtra, qreal dpr) {
    return {cosmeticPen(stroke, widthPx, dpr), cosmeticPen(casing, widthPx + casingExtra, dpr)};
}

}  // namespace

OverlayStyle::OverlayStyle(const ThemeTokens& tokens, qreal devicePixelRatio)
    : tokens_(tokens), dpr_(devicePixelRatio), accent_(tokens.dark.surface.accent) {
    rebuild();
}

void OverlayStyle::setDevicePixelRatio(qreal dpr) {
    if (dpr == dpr_) return;
    dpr_ = dpr;
    rebuild();
}

void OverlayStyle::setAccent(const QColor& accent) {
    if (accent == accent_) return;
    accent_ = accent;
    rebuild();
}

double OverlayStyle::strokeWidth(const ThemeTokens& t, OverlayState state, BoxKind kind, bool selected) {
    const double stateWidth = t.line[static_cast<size_t>(stateIndex(state))].width;
    double w = stateWidth;
    switch (kind) {
        case BoxKind::Body: w = stateWidth; break;
        case BoxKind::Pin: w = stateWidth - 0.5; break;
        case BoxKind::Window: w = 1.0; break;
    }
    return selected ? w + t.selectedWidthDelta : w;
}

int OverlayStyle::frameThickness(OverlayState state, BoxKind kind, bool selected) const {
    return BoxFrames::thickness(strokeWidth(tokens_, state, kind, selected));
}

bool OverlayStyle::frameable(OverlayState state) const {
    const LineSpec& spec = tokens_.line[static_cast<size_t>(stateIndex(state))];
    return spec.dashPx.isEmpty() || spec.dashPx.size() == 2;
}

BoxFrames::DashSpec OverlayStyle::dashSpec(OverlayState state, BoxKind kind, bool selected) const {
    const LineSpec& spec = tokens_.line[static_cast<size_t>(stateIndex(state))];
    if (spec.dashPx.size() != 2) return {};
    // tokens 的 dash 單位是像素。圓端點與方端點會讓每一段在兩端各多出半個線寬（Qt 的 cap 規則），
    // 零長度的「點」因此是一個線寬大的圓點：visible ＝ dash ＋ 線寬。
    const int t = frameThickness(state, kind, selected);
    const int dash = static_cast<int>(std::lround(spec.dashPx.at(0)));
    const int gap = static_cast<int>(std::lround(spec.dashPx.at(1)));
    BoxFrames::DashSpec d;
    d.visible = std::max(1, dash + (spec.cap == Qt::FlatCap ? 0 : t));
    d.period = std::max(d.visible + 1, dash + gap);
    return d;
}

QPen OverlayStyle::makeBoxPen(const ThemeTokens& t, OverlayState state, BoxKind kind, bool selected, bool casing,
                              qreal dpr) {
    const LineSpec& spec = t.line[static_cast<size_t>(stateIndex(state))];
    const StrokeColors& colors = t.overlay.state[static_cast<size_t>(stateIndex(state))];
    const double w = strokeWidth(t, state, kind, selected);
    const double drawn = casing ? w + t.casingExtraWidth : w;
    QPen pen = cosmeticPen(casing ? colors.casing : colors.stroke, drawn, dpr);
    pen.setCapStyle(spec.cap);
    if (!spec.dashPx.isEmpty()) {
        // tokens 的虛線長度單位是像素；QPen 的單位是線寬，所以除以「實際畫的線寬」。
        // 零長度的虛線段 Qt 不會畫（實測：[0, 2] 畫出 0 個像素，抗鋸齒開關都一樣）。點線（Rejected）要的是
        // 「長度 0 ＋ 圓端點 ＝ 一個圓點」，所以用極小的正值取代 0：[0.001, 2] 畫出圓點，約一半覆蓋。
        constexpr qreal kMinDashLengthInWidths = 0.001;
        QList<qreal> pattern;
        for (qreal px : spec.dashPx) pattern.append(std::max(px / drawn, kMinDashLengthInWidths));
        pen.setDashPattern(pattern);
    }
    return pen;
}

void OverlayStyle::rebuild() {
    for (int s = 0; s < kOverlayStateCount; ++s) {
        for (int k = 0; k < kBoxKindCount; ++k) {
            for (int sel = 0; sel < 2; ++sel) {
                const auto state = static_cast<OverlayState>(s);
                const auto kind = static_cast<BoxKind>(k);
                boxPens_[static_cast<size_t>(index(state, kind, sel == 1))] = {
                    makeBoxPen(tokens_, state, kind, sel == 1, false, dpr_),
                    makeBoxPen(tokens_, state, kind, sel == 1, true, dpr_)};
            }
        }
    }

    const OverlayTokens& ov = tokens_.overlay;
    accentLine_ = solidPair(accent_, ov.defCasing, 1.0, tokens_.casingExtraWidth, dpr_);
    region_ = solidPair(ov.regionStroke, ov.regionCasing, ov.regionWidth, tokens_.casingExtraWidth, dpr_);
    QColor fill = ov.regionStroke;
    fill.setAlphaF(ov.regionFillOpacity);
    regionFill_ = QBrush(fill);

    definition_[static_cast<size_t>(kindIndex(BoxKind::Body))] =
        solidPair(ov.defStroke, ov.defCasing, ov.defWidthBody, tokens_.casingExtraWidth, dpr_);
    definition_[static_cast<size_t>(kindIndex(BoxKind::Pin))] =
        solidPair(ov.defStroke, ov.defCasing, ov.defWidthPin, tokens_.casingExtraWidth, dpr_);
    definition_[static_cast<size_t>(kindIndex(BoxKind::Window))] =
        solidPair(ov.defStroke, ov.defCasing, 1.0, tokens_.casingExtraWidth, dpr_);
    QColor pinFill = ov.defStroke;
    pinFill.setAlphaF(ov.defPinFillOpacity);
    definitionPinFill_ = QBrush(pinFill);
}

}  // namespace aoi::ui
