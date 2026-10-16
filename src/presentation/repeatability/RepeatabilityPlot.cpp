#include "presentation/repeatability/RepeatabilityPlot.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <algorithm>
#include <cmath>
#include <set>

namespace aoi::ui {
RepeatabilityPlot::RepeatabilityPlot(QWidget* parent) : QWidget(parent) {
    setMinimumSize(260, 190);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setAccessibleName(QStringLiteral("Repeatability chart"));
}
void RepeatabilityPlot::setSeries(std::vector<RepeatabilityPoint> points, const QString& title, const QString& unit,
                                 std::optional<double> threshold, double minimum, double maximum) {
    points_ = std::move(points); title_ = title; unit_ = unit; threshold_ = threshold;
    minimum_ = minimum; maximum_ = maximum > minimum ? maximum : minimum + 1.0;
    update();
}
void RepeatabilityPlot::setXAxis(const QString& title, const QStringList& labels) {
    axisTitle_ = title; axisLabels_ = labels; update();
}
void RepeatabilityPlot::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), palette().base());
    painter.setRenderHint(QPainter::Antialiasing);
    const QFontMetrics fm(font());
    const int left = std::max(56, fm.horizontalAdvance(QString::number(minimum_, 'g', 5)) + 14);
    const QRectF plot(left, fm.height() * 2, std::max(1, width() - left - 20), std::max(1, height() - fm.height() * 4 - 10));
    const auto y = [&](double value) { return plot.bottom() - (value - minimum_) / (maximum_ - minimum_) * plot.height(); };
    painter.setPen(palette().text().color());
    painter.drawText(QRect(8, 0, width() - 16, fm.height() + 6), Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(title_ + QStringLiteral("  ") + unit_, Qt::ElideMiddle, width() - 16));
    for (int i = 0; i <= 3; ++i) {
        const double v = minimum_ + (maximum_ - minimum_) * i / 3;
        const double yy = y(v);
        painter.setPen(palette().mid().color()); painter.drawLine(QPointF(plot.left(), yy), QPointF(plot.right(), yy));
        painter.setPen(palette().text().color());
        painter.drawText(QRectF(0, yy - fm.height() / 2.0, left - 7, fm.height()), Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'g', 4));
    }
    painter.drawText(QRectF(plot.left(), plot.bottom() + fm.height() + 3, plot.width(), fm.height()), Qt::AlignCenter, axisTitle_);
    if (points_.empty()) return;
    const auto pointX = [&](std::size_t i) {
        return plot.left() + plot.width() * (axisLabels_.empty() ? static_cast<double>(i) / static_cast<double>(std::max(std::size_t(1), points_.size() - 1)) :
            (static_cast<double>(i) + 0.5) / static_cast<double>(points_.size()));
    };
    std::set<std::size_t> ticks{0, (points_.size() - 1) / 2, points_.size() - 1};
    if (!axisLabels_.empty()) for (std::size_t i = 0; i < points_.size(); ++i) ticks.insert(i);
    for (const std::size_t i : ticks) {
        const double span = axisLabels_.empty() ? 50 : plot.width() / static_cast<double>(points_.size());
        const QString label = i < static_cast<std::size_t>(axisLabels_.size()) ? fm.elidedText(axisLabels_[static_cast<qsizetype>(i)], Qt::ElideMiddle, static_cast<int>(span) - 6) : QString::number(i + 1);
        painter.drawText(QRectF(pointX(i) - span / 2, plot.bottom() + 2, span, fm.height()), Qt::AlignCenter, label);
    }
    painter.save(); painter.setClipRect(plot.adjusted(-1, -1, 1, 1));
    if (threshold_) {
        QPen limit(palette().link().color(), 1, Qt::DashLine); painter.setPen(limit);
        painter.drawLine(QPointF(plot.left(), y(*threshold_)), QPointF(plot.right(), y(*threshold_)));
        painter.drawText(QRectF(plot.left(), y(*threshold_) - fm.height() - 2, plot.width() - 3, fm.height()),
                         Qt::AlignRight | Qt::AlignVCenter, QString::number(*threshold_, 'g', 4) + unit_);
    }
    QPainterPath path;
    bool continuous = false;
    painter.setPen(QPen(palette().highlight().color(), 1.5));
    for (std::size_t i = 0; i < points_.size(); ++i) {
        const auto& point = points_[i];
        if (!point.value) { continuous = false; continue; }
        const QPointF p(pointX(i), y(*point.value));
        if (continuous) path.lineTo(p); else path.moveTo(p);
        continuous = point.complete;
        if (!point.complete || points_.size() == 1 || !axisLabels_.empty()) painter.drawEllipse(p, 3, 3);
        if (!axisLabels_.empty())
            painter.drawText(QRectF(p.x() - 55, p.y() - fm.height() - 5, 110, fm.height()), Qt::AlignCenter, QString::number(*point.value, 'g', 5));
        if (!axisLabels_.empty())
            painter.drawText(QRectF(p.x() - 55, p.y() - fm.height() - 5, 110, fm.height()), Qt::AlignCenter, QString::number(*point.value, 'g', 5));
    }
    painter.drawPath(path);
    painter.restore();
}
int RepeatabilityPlot::pointAt(const QPointF& p) const {
    if (points_.empty()) return -1;
    const int left = std::max(56, QFontMetrics(font()).horizontalAdvance(QString::number(minimum_, 'g', 5)) + 14);
    const double position = (p.x() - left) / std::max(1, width() - left - 20);
    const int index = axisLabels_.empty() ? qRound(position * static_cast<double>(points_.size() - 1)) : static_cast<int>(std::floor(position * static_cast<double>(points_.size())));
    return std::clamp(index, 0, static_cast<int>(points_.size() - 1));
}
void RepeatabilityPlot::mouseMoveEvent(QMouseEvent* event) {
    const int i = pointAt(event->position());
    if (i < 0) return;
    const auto& p = points_[static_cast<std::size_t>(i)];
    QString text = QString::fromStdString(axisLabels_.empty() ? p.key.component + "/" + p.key.box : p.key.box);
    text += QStringLiteral("\n") + (p.value ? QString::number(*p.value, 'g', 7) + unit_ : QStringLiteral("—")) + (p.complete ? QString() : QStringLiteral(" [n < expected]"));
    QToolTip::showText(event->globalPosition().toPoint(), text, this);
}
void RepeatabilityPlot::mousePressEvent(QMouseEvent* event) {
    const int i = pointAt(event->position());
    if (event->button() == Qt::LeftButton && i >= 0) {
        const auto& key = points_[static_cast<std::size_t>(i)].key;
        emit pointActivated(QString::fromStdString(key.component), QString::fromStdString(key.box));
    }
}
}  // namespace aoi::ui
