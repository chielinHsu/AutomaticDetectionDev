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
// 代價：粗細必須是整數像素（1.5 取 1、2.5 取 2；見 thickness），虛線畫不了（虛線狀態仍用描邊）。
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

    // 描邊粗細（整數像素）：max(1, floor(寬度 + 0.25))。
    // tokens 的寬度有 0.5 的差（Pin ＝ 狀態寬度 − 0.5），整數像素表示不了：Found 的主體 2、Pin 1、窗 1。
    static int thickness(double widthPx);

    // screen：這個框在螢幕上的矩形（裝置無關像素）。粗細向內長，外框向外多 1 px、向內多 1 px。
    static void append(Batch& batch, const QRectF& screen, int thickness);

    // painter 必須是 identity 轉換、關閉抗鋸齒。先畫外框、再畫描邊。
    static void draw(QPainter& painter, const Batch& batch, const QColor& casing, const QColor& stroke);

private:
    static void appendFrame(std::vector<QRect>& out, int x, int y, int w, int h, int t);
};

}  // namespace aoi::ui
