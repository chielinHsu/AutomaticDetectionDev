#pragma once

#include <QGraphicsItem>
#include <QHash>
#include <QImage>
#include <QPoint>
#include <QRectF>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "presentation/canvas/BoxFrames.h"
#include "presentation/canvas/ComponentSnapshot.h"
#include "presentation/canvas/OverlayStyle.h"

namespace aoi::ui {

// 板上全部元件的框，用「一個 Item」批次繪製。
//
// 為什麼不是一個框一個 QGraphicsItem：每個 Item 都有場景索引、每次繪製的樣式選項與畫筆切換成本，
// 5 000 個框時這些成本壓過真正的繪圖。這裡把同樣式的框放進同一個桶，每個桶設一次畫筆、
// 一次 drawRects；只畫露出範圍內的框；螢幕上太小的 Pin 與 Window 直接略過（層級簡化，LOD）。
//
// 單位是元件：選取、點擊判定、標籤都以元件為單位；框只是元件的一部分。
//
// 字形（S11）：同一個桶裡的框在螢幕上幾乎都是同一個大小（同一個樣本），所以每種（狀態、種類、螢幕寬高）
// 只畫一次成一張小圖（外框＋描邊，內部透明），之後每個框只做一次未轉換的 drawImage（alpha 混合的 blit）。
// 微基準（perf_paint_micro）：2 600 個框填色矩形 5.1 ms、字形 0.35 ms。像素和填色矩形完全相同（tst_items 比對）。
// 太大的框（邊超過 kGlyphMaxSide）與 devicePixelRatio ≠ 1 時照舊用填色矩形。
class RoiLayerItem : public QGraphicsItem {
public:
    explicit RoiLayerItem(OverlayStyle& style);

    void setBounds(const QRectF& bounds);  // 通常是影像矩形
    void setSnapshot(SnapshotPtr snapshot);
    void setSelection(const std::vector<int>& componentIds);
    const std::vector<int>& selection() const { return selectedIds_; }

    // 點擊判定：回傳外框包含該點、面積最小的元件編號；沒有則 -1。
    int componentAt(const QPointF& scenePos) const;
    // 這些元件的外框聯集；找不到任何一個時回傳空的矩形。
    QRectF outlineOf(const std::vector<int>& componentIds) const;

    int componentCount() const { return snapshot_ ? static_cast<int>(snapshot_->components.size()) : 0; }
    int boxCount() const { return snapshot_ ? static_cast<int>(snapshot_->boxCount()) : 0; }

    // 最近一次 paint 的統計：測試用它驗證 LOD 與裁切真的有生效。
    struct PaintStats {
        int boxesConsidered = 0;  // 桶裡的框數（含畫面外的）
        int boxesDrawn = 0;
        int boxesSkippedByLod = 0;
        int bucketsDrawn = 0;     // 實際畫了框的桶數；每個桶只設兩次畫筆（外框、描邊）
        int boxesDrawnAsGlyphs = 0;  // 其中用字形畫的（S11）
        int glyphsBuilt = 0;         // 這次 paint 新建的字形數（快取沒命中）
        int labelsDrawn = 0;
        double paintMs = 0;       // 這次 paint 在 Item 內花的時間（可觀察性；量一次計時器約 50 ns）
    };
    const PaintStats& lastPaintStats() const { return stats_; }

    // 層級簡化的門檻（螢幕像素）。Body 永遠畫。
    static constexpr double kMinBoxScreenSide = 3.0;
    // 元件外框的螢幕寬度達到這個值才顯示標籤。
    static constexpr double kLabelMinScreenWidth = 48.0;
    // 字形的上限（螢幕像素）：寬或高超過就用填色矩形（放大時框少、字形反而大）。
    static constexpr int kGlyphMaxSide = 96;
    // 快取的字形數超過就整個清掉（連續縮放時每一級都是新的大小）；在 paint 開始時檢查。
    static constexpr std::size_t kGlyphCacheLimit = 64;

    // 測試與量測用：關掉字形，全部走填色矩形（S11 之前的畫法）。
    void setGlyphsEnabled(bool enabled);
    bool glyphsEnabled() const { return glyphsEnabled_; }
    void invalidateGlyphs() { glyphs_.clear(); }
    std::size_t glyphCacheSize() const { return glyphs_.size(); }

    QRectF boundingRect() const override { return bounds_; }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    static constexpr int bucketIndex(OverlayState s, BoxKind k) { return stateIndex(s) * kBoxKindCount + kindIndex(k); }

    void paintSelection(QPainter* painter, qreal lod);
    void paintLabels(QPainter* painter, const QRectF& cull, qreal lod);
    // 這種（狀態、種類、螢幕寬高）的字形：快取有就直接給，沒有就畫一張。回傳的參考在這次 paint 內有效
    // （unordered_map 的節點不會搬家；快取只在 paint 開始時清）。
    const QImage& glyphFor(OverlayState state, BoxKind kind, int w, int h);

    struct GlyphBlit {
        QPoint origin;        // 字形左上角的螢幕位置（框的左上角再往外 1 px 的外框）
        const QImage* image;  // 指向 glyphs_ 裡的字形
    };

    OverlayStyle& style_;
    QRectF bounds_;
    SnapshotPtr snapshot_;
    // 每個（狀態，種類）一個桶；桶內是場景座標的矩形。
    std::array<std::vector<QRectF>, kOverlayStateCount * kBoxKindCount> buckets_;
    QHash<int, int> idToIndex_;
    std::vector<int> selectedIds_;
    std::vector<int> selectedIndex_;
    std::vector<QRectF> visible_;  // 每個桶重複使用的暫存，避免每幀配置記憶體
    std::array<BoxFrames::Batch, kBoxKindCount> frames_;  // 同上：每種框一組填色矩形
    std::array<std::vector<GlyphBlit>, kBoxKindCount> glyphBlits_;  // 同上：每種框這次要 blit 的字形
    std::unordered_map<std::uint64_t, QImage> glyphs_;
    bool glyphsEnabled_ = true;
    PaintStats stats_;
};

}  // namespace aoi::ui
