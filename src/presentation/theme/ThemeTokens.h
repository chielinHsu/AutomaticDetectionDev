#pragma once

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QSize>
#include <QString>
#include <Qt>

#include "presentation/canvas/OverlayTypes.h"

namespace aoi::ui {

// docs/ux/tokens.json 的型別化版本。數值只有一個來源（tokens.json）；
// 程式裡不要再寫一份顏色或尺寸。

struct StrokeColors {
    QColor stroke;
    QColor casing;
};

struct SurfaceColors {
    QColor window, panel, canvas, text, textMuted, border, accent, onAccent, focusRing;
};

struct ThemeColors {
    SurfaceColors surface;
    PerState<StrokeColors> state;  // dock 的圖例與文字用；畫布上的框用 OverlayTokens
};

// 單一狀態的線型。dashPx 的單位是「沿路徑的像素」；QPen::setDashPattern 以線寬為單位，要除以線寬。
struct LineSpec {
    double width = 0;
    QList<qreal> dashPx;
    Qt::PenCapStyle cap = Qt::FlatCap;
    QString marker;
};

struct OverlayTokens {
    PerState<StrokeColors> state;  // 兩個主題共用的亮色＋深色外框
    QColor boardGreen, boardBlue, boardBlack;
    QColor regionStroke, regionCasing;
    double regionFillOpacity = 0;
    double regionWidth = 0;
    QColor defStroke, defCasing;
    double defWidthBody = 0, defWidthPin = 0, defPinFillOpacity = 0;
};

struct SizeTokens {
    double handleVisual = 0;
    double handleHit = 0;
    double handlesMoveOutsideBelow = 0;
    int toolbarButton = 0;
    int statusBarHeight = 0;
    int dockMinWidth = 0;
    int messageBarHeight = 0;
    QSize windowMin;
};

struct InteractionTokens {
    double zoomStepPerNotch = 1.1;
    double zoomMinPercent = 5;
    double zoomMaxPercent = 3200;
    double fitMarginPx = 24;
    double minSampleSidePx = 8;
};

struct TimingTokens {
    int visualFeedbackMax = 0, busyAfter = 0, busyMinShow = 0, progressAfter = 0;
    int cancelToIdleMax = 0, paramDebounce = 0, toast = 0;
};

struct ThemeTokens {
    bool valid = false;
    QString error;  // valid == false 時才有值

    ThemeColors light;
    ThemeColors dark;
    OverlayTokens overlay;
    PerState<LineSpec> line;  // stateStyle.{sample,found,inferred,rejected}
    double selectedWidthDelta = 1;
    double casingExtraWidth = 2;
    SizeTokens sizes;
    InteractionTokens interaction;
    TimingTokens timing;
    double fontBasePt = 9;
    QList<double> fontScaleOptions;

    static ThemeTokens parse(const QByteArray& json);
    // 內嵌在執行檔裡的 tokens.json（資源 :/aoi/ux/tokens.json）。解析失敗是建置錯誤，直接中止。
    static const ThemeTokens& builtin();
};

}  // namespace aoi::ui
