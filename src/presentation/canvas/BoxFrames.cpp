#include "presentation/canvas/BoxFrames.h"

#include <algorithm>
#include <cmath>

namespace aoi::ui {

int BoxFrames::thickness(double widthPx) { return std::max(1, static_cast<int>(std::floor(widthPx + 0.25))); }

void BoxFrames::appendFrame(std::vector<QRect>& out, int x, int y, int w, int h, int t) {
    // 上、下兩條整寬；左、右兩條夾在中間。框比兩倍粗細還小時，上下會重疊、左右省略，結果是一塊實心。
    out.emplace_back(x, y, w, t);
    out.emplace_back(x, y + h - t, w, t);
    const int innerH = h - 2 * t;
    if (innerH > 0) {
        out.emplace_back(x, y + t, t, innerH);
        out.emplace_back(x + w - t, y + t, t, innerH);
    }
}

void BoxFrames::append(Batch& batch, const QRectF& screen, int t) {
    const int x0 = qRound(screen.left());
    const int y0 = qRound(screen.top());
    // 至少 1 px：再小的框也要看得到（Body 永遠畫）。
    const int w = std::max(1, qRound(screen.right()) - x0);
    const int h = std::max(1, qRound(screen.bottom()) - y0);
    appendFrame(batch.stroke, x0, y0, w, h, std::min(t, std::max(1, std::min(w, h))));
    appendFrame(batch.casing, x0 - 1, y0 - 1, w + 2, h + 2, std::min(t + 2, std::max(1, std::min(w + 2, h + 2))));
}

void BoxFrames::appendDashedEdge(Batch& batch, int x, int y, int length, bool horizontal, int t, const DashSpec& dash) {
    for (int offset = 0; offset < length; offset += dash.period) {
        const int run = std::min(dash.visible, length - offset);
        const QRect piece = horizontal ? QRect(x + offset, y, run, t) : QRect(x, y + offset, t, run);
        batch.stroke.push_back(piece);
        batch.casing.push_back(piece.adjusted(-1, -1, 1, 1));  // 外框：每側多 1 px，和實線的外框一致
    }
}

void BoxFrames::appendDashed(Batch& batch, const QRectF& screen, int t, const DashSpec& dash) {
    if (!dash.valid()) {
        append(batch, screen, t);
        return;
    }
    const int x0 = qRound(screen.left());
    const int y0 = qRound(screen.top());
    const int w = std::max(1, qRound(screen.right()) - x0);
    const int h = std::max(1, qRound(screen.bottom()) - y0);
    const int th = std::min(t, std::max(1, std::min(w, h)));
    appendDashedEdge(batch, x0, y0, w, true, th, dash);            // 上
    appendDashedEdge(batch, x0, y0 + h - th, w, true, th, dash);   // 下
    const int innerH = h - 2 * th;
    if (innerH > 0) {
        appendDashedEdge(batch, x0, y0 + th, innerH, false, th, dash);           // 左
        appendDashedEdge(batch, x0 + w - th, y0 + th, innerH, false, th, dash);  // 右
    }
}

void BoxFrames::draw(QPainter& painter, const Batch& batch, const QColor& casing, const QColor& stroke) {
    if (batch.empty()) return;
    painter.setPen(Qt::NoPen);
    painter.setBrush(casing);
    painter.drawRects(batch.casing.data(), static_cast<int>(batch.casing.size()));
    painter.setBrush(stroke);
    painter.drawRects(batch.stroke.data(), static_cast<int>(batch.stroke.size()));
}

}  // namespace aoi::ui
