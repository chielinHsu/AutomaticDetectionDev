#pragma once

#include <QWidget>
#include <QStringList>
#include "application/repeatability/RepeatabilityAnalysis.h"

namespace aoi::ui {
struct RepeatabilityPoint {
    repeatability::BoxKey key;
    std::optional<double> value;
    bool complete = false;
};
class RepeatabilityPlot : public QWidget {
    Q_OBJECT
public:
    explicit RepeatabilityPlot(QWidget* parent = nullptr);
    void setSeries(std::vector<RepeatabilityPoint> points, const QString& title, const QString& unit,
                   std::optional<double> threshold, double minimum, double maximum);
    const std::vector<RepeatabilityPoint>& points() const { return points_; }
    void setXAxis(const QString& title, const QStringList& labels);
signals:
    void pointActivated(const QString& component, const QString& box);
protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
private:
    int pointAt(const QPointF& position) const;
    std::vector<RepeatabilityPoint> points_;
    QString title_, unit_;
    QString axisTitle_ = QStringLiteral("Box");
    QStringList axisLabels_;
    std::optional<double> threshold_;
    double minimum_ = 0, maximum_ = 1;
};
}  // namespace aoi::ui
