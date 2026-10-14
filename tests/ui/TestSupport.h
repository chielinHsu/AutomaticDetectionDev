#pragma once

#include <QApplication>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QPointF>
#include <QTest>
#include <QWheelEvent>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <vector>

#include "presentation/canvas/ICanvasController.h"
#include "presentation/canvas/ImageCanvas.h"

// 測試共用的輔助：注入滑鼠、滾輪、鍵盤事件；記錄 controller 收到的呼叫。
namespace aoi::test {

inline void sendWheel(QWidget* w, const QPointF& pos, int notches) {
    QWheelEvent e(pos, w->mapToGlobal(pos), QPoint(0, 0), QPoint(0, 120 * notches), Qt::NoButton, Qt::NoModifier,
                  Qt::NoScrollPhase, false);
    QApplication::sendEvent(w, &e);
}

inline void sendMouse(QWidget* w, QEvent::Type type, const QPointF& pos, Qt::MouseButton button,
                      Qt::MouseButtons buttons, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QMouseEvent e(type, pos, w->mapToGlobal(pos), button, buttons, mods);
    QApplication::sendEvent(w, &e);
}
inline void press(QWidget* w, const QPointF& p, Qt::MouseButton b = Qt::LeftButton, Qt::KeyboardModifiers m = {}) {
    sendMouse(w, QEvent::MouseButtonPress, p, b, b, m);
}
inline void move(QWidget* w, const QPointF& p, Qt::MouseButtons held = Qt::LeftButton) {
    sendMouse(w, QEvent::MouseMove, p, Qt::NoButton, held);
}
inline void release(QWidget* w, const QPointF& p, Qt::MouseButton b = Qt::LeftButton) {
    sendMouse(w, QEvent::MouseButtonRelease, p, b, Qt::NoButton);
}
inline void doubleClick(QWidget* w, const QPointF& p) {
    sendMouse(w, QEvent::MouseButtonDblClick, p, Qt::LeftButton, Qt::LeftButton);
}

inline void sendKey(QWidget* w, int key, QEvent::Type type = QEvent::KeyPress, Qt::KeyboardModifiers mods = {}) {
    QKeyEvent e(type, key, mods);
    QApplication::sendEvent(w, &e);
}
inline void tapKey(QWidget* w, int key) {
    sendKey(w, key, QEvent::KeyPress);
    sendKey(w, key, QEvent::KeyRelease);
}

// 視埠座標 ↔ 影像座標（浮點，不經過 Qt 的整數版本）。
inline QPointF toView(aoi::ui::ImageCanvas& c, const QPointF& scenePos) { return c.viewportTransform().map(scenePos); }
inline QPointF toScene(aoi::ui::ImageCanvas& c, const QPointF& viewPos) {
    return c.viewportTransform().inverted().map(viewPos);
}

// 一次畫完所有待處理的更新（和使用者看到的「這一幀畫完」同義）。
inline void paintNow(QWidget* top) {
    QCoreApplication::sendPostedEvents(top, QEvent::UpdateRequest);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
}

inline double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    const double idx = p * (static_cast<double>(v.size()) - 1);
    const size_t lo = static_cast<size_t>(std::floor(idx));
    const size_t hi = static_cast<size_t>(std::ceil(idx));
    return v[lo] + (v[hi] - v[lo]) * (idx - static_cast<double>(lo));
}

// 記錄 controller 呼叫的假 controller。
class RecordingController final : public aoi::ui::ICanvasController, public aoi::ui::ISelectionController {
public:
    struct Region {
        QRectF rect;
        aoi::ui::DraftMode mode;
    };
    std::vector<Region> regions;
    int aborted = 0;
    std::vector<QPointF> adds;
    std::vector<std::pair<int, QPointF>> editors;
    std::vector<std::pair<std::vector<int>, QPointF>> moves;
    std::vector<std::vector<int>> selections;
    std::vector<std::pair<int, QRectF>> resizes;
    int deletes = 0;
    int selectAlls = 0;
    int selectSameTypes = 0;

    void onRegionDrawn(const QRectF& rect, aoi::ui::DraftMode mode) override { regions.push_back({rect, mode}); }
    void onRegionAborted() override { ++aborted; }
    void onAddRequested(const QPointF& p) override { adds.push_back(p); }
    void onComponentsMoved(const std::vector<int>& ids, const QPointF& delta) override { moves.push_back({ids, delta}); }
    void onOpenTypeEditor(int id, const QPointF& p) override { editors.push_back({id, p}); }
    void onComponentResized(int id, const QRectF& rect) override { resizes.push_back({id, rect}); }
    void select(const std::vector<int>& ids) override { selections.push_back(ids); }
    void deleteSelected() override { ++deletes; }
    void selectAll() override { ++selectAlls; }
    void selectSameType() override { ++selectSameTypes; }
};

}  // namespace aoi::test
