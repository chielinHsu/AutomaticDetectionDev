#pragma once

#include <QColor>
#include <QGraphicsItem>
#include <QPixmap>
#include <QRectF>
#include <array>

#include "presentation/compare/CompareInterfaces.h"
#include "presentation/theme/ThemeTokens.h"

namespace aoi::ui {

// A/B 比較的疊圖（S09）：每個比較過的位置一個標記，畫在比較視窗的畫布上（一個 Item 批次繪製，同 RoiLayerItem 的理由）。
//
//  - 兩邊都找到＝綠色的圓、只有 A＝藍色的方塊、只有 B＝橘色的三角形：顏色之外形狀也不同（色覺障礙、灰階列印也分得出來）。
//    三個顏色取自 tokens.json 的 overlay（Found、Sample、Inferred 的描邊）：同一組已經驗過對比的亮色＋深色外框，
//    不另外定義顏色（tokens.json 不改）。
//  - 標記的大小固定在螢幕上（kMarkerScreenPx），縮小到看全板時也看得到；元件的框（範本大小、依角度轉）只在放大到看得清楚時才畫。
//  - 標記畫成「小圖」：每一類先畫一次（外框＋描邊、反鋸齒），每個位置貼一次。S09 實測 5 600 個標記：每個標記描兩次邊 96 ms，
//    貼小圖 3.9 ms（perf.md 的 AB-U1b）。
//  - 差異（只有 A、只有 B）畫在兩邊都找到的上面；選了的那一個加一圈強調。
class CompareOverlayItem : public QGraphicsItem {
public:
    explicit CompareOverlayItem(const ThemeTokens& tokens);

    void setBounds(const QRectF& bounds);  // 通常是影像矩形
    void setOverlay(CompareOverlayPtr overlay);
    const CompareOverlayPtr& overlay() const { return overlay_; }
    void setKindVisible(aoi::CompareKind kind, bool visible);
    bool isKindVisible(aoi::CompareKind kind) const { return visible_[index(kind)]; }
    void setHighlight(int markIndex);  // -1 ＝ 沒有
    int highlight() const { return highlight_; }
    void setAccent(const QColor& accent);

    QColor color(aoi::CompareKind kind) const { return colors_[index(kind)]; }

    struct PaintStats {
        int marksDrawn = 0;
        int outlinesDrawn = 0;
        double paintMs = 0.0;
    };
    const PaintStats& lastPaintStats() const { return stats_; }

    static constexpr double kMarkerScreenPx = 11.0;      // 標記的邊長（裝置無關的螢幕像素）
    static constexpr double kOutlineMinScreenPx = 12.0;  // 元件短邊在螢幕上至少這麼大才畫框

    QRectF boundingRect() const override { return bounds_; }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    static constexpr int index(aoi::CompareKind k) { return static_cast<int>(k); }
    void rebuildSprites(qreal dpr);

    QRectF bounds_;
    CompareOverlayPtr overlay_;
    std::array<bool, 3> visible_{true, true, true};
    std::array<QColor, 3> colors_;
    QColor casing_;
    QColor accent_;
    int highlight_ = -1;
    std::array<QPixmap, 3> sprites_;  // 每一類一張標記的小圖（依 devicePixelRatio）
    qreal spriteDpr_ = 0.0;
    PaintStats stats_;
};

}  // namespace aoi::ui
