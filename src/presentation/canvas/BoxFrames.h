#pragma once

#include <QColor>
#include <QPainter>
#include <QRect>
#include <QRectF>
#include <vector>

namespace aoi::ui {

// 把框畫成「整數像素的填色細矩形」（上、下、左、右各一條），不用 cosmetic 描邊。
//
// 為什麼：本機實測（純 QPainter，2 600 個框，各畫外框與描邊）
//   cosmetic 描邊 drawRects      27.6 ms
//   一個 QPainterPath 描邊        68.2 ms
//   整數像素填色細矩形 drawRects   5.9 ms
// 描邊要走筆畫轉路徑（stroker）再光柵化，每個框約 10 µs；填色矩形走快速的掃描線填充。
// 代價：粗細必須是整數像素（1.5 取 1、2.5 取 2；見 thickness）。
//
// 虛線與點線（S04 起）也用填色矩形：沿著每條邊切成「畫 visible 個像素、每 period 個像素重複」的小矩形。
// 為什麼：S04 的預覽會讓使用者把分數門檻拉高，一次有上千個框變成點線；cosmetic 的點線筆（圓端點）每個框要 68 µs，
// 5 832 個框一個畫格 394 ms；同樣的點用填色矩形畫只要 3.9 µs（微基準：docs/steps/S04-presenter/perf.md）。
// 代價：每條邊各自從頭起算相位（描邊筆則是沿著整個周長連續），所以角落的虛線排列和舊畫法不完全一樣。
//
// 座標都是「裝置無關像素」（畫筆 identity 轉換時的座標）。呼叫端負責把場景矩形換算成螢幕矩形，
// 並在 draw 之前 resetTransform。在高 DPI 下，整數的裝置無關像素就是整數倍的裝置像素，所以不必乘 devicePixelRatio。
class BoxFrames {
public:
    struct Batch {
        std::vector<QRect> casing;  // 外框：比描邊每一側多 1 px，對比色
        std::vector<QRect> stroke;
        void clear() {
            casing.clear();
            stroke.clear();
        }
        bool empty() const { return stroke.empty(); }
    };

    // 虛線／點線的整數像素描述：每 period 個像素重複，每次畫 visible 個像素（從邊的起點開始）。
    struct DashSpec {
        int visible = 0;
        int period = 0;
        bool valid() const { return visible > 0 && period > visible; }
    };

    // 螢幕矩形 → 整數像素的框：左上角四捨五入，寬高至少 1 px（再小的框也要看得到，Body 永遠畫）。
    // append／appendDashed 都用它；RoiLayerItem 的字形（S11）也用同一個規則取大小，所以兩種畫法的像素一樣。
    static QRect snap(const QRectF& screen);

    // 描邊粗細（整數像素）：max(1, floor(寬度 + 0.25))。
    // tokens 的寬度有 0.5 的差（Pin ＝ 狀態寬度 − 0.5），整數像素表示不了：Found 的主體 2、Pin 1、窗 1。
    static int thickness(double widthPx);

    // screen：這個框在螢幕上的矩形（裝置無關像素）。粗細向內長，外框向外多 1 px、向內多 1 px。
    static void append(Batch& batch, const QRectF& screen, int thickness);

    // 同 append，但描邊與外框都切成 dash 指定的小段。dash 無效時等同 append。
    static void appendDashed(Batch& batch, const QRectF& screen, int thickness, const DashSpec& dash);

    // painter 必須是 identity 轉換、關閉抗鋸齒。先畫外框、再畫描邊。
    static void draw(QPainter& painter, const Batch& batch, const QColor& casing, const QColor& stroke);

private:
    static void appendFrame(std::vector<QRect>& out, int x, int y, int w, int h, int t);
    static void appendDashedEdge(Batch& batch, int x, int y, int length, bool horizontal, int t, const DashSpec& dash);
};

}  // namespace aoi::ui
