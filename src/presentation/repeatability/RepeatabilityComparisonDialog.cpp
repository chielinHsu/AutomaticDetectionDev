#include "presentation/repeatability/RepeatabilityComparisonDialog.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <algorithm>
#include <set>
#include "presentation/repeatability/RepeatabilityPlot.h"
#include "ui_RepeatabilityComparisonDialog.h"

namespace aoi::ui {
using namespace repeatability;
RepeatabilityComparisonDialog::RepeatabilityComparisonDialog(std::shared_ptr<const RepeatabilityState> snapshot,
        const std::vector<std::string>& metrics, QWidget* parent)
    : QDialog(parent), snapshot_(std::move(snapshot)), ui_(std::make_unique<Ui::RepeatabilityComparisonDialog>()) {
    ui_->setupUi(this); setWindowTitle(qtTrId("repeat.overlay")); resize(1380, 720); setMinimumSize(840, 480);
    ui_->metricLabel->setText(qtTrId("repeat.metrics"));
    ui_->comparisonMetric->setAccessibleName(qtTrId("repeat.metrics"));
    for (const auto& id : metrics) ui_->comparisonMetric->addItem(QString::fromStdString(id));
    ui_->boxLabel->setText(qtTrId("repeat.boxFilter"));
    ui_->boxName->setPlaceholderText(qtTrId("repeat.boxFilterHint"));
    ui_->boxName->setAccessibleName(qtTrId("repeat.boxFilter"));
    ui_->buttons->button(QDialogButtonBox::Close)->setText(qtTrId("repeat.close"));
    connect(ui_->buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(ui_->comparisonMetric, &QComboBox::currentIndexChanged, this, &RepeatabilityComparisonDialog::refreshCharts);
    connect(ui_->boxName, &QLineEdit::textChanged, this, &RepeatabilityComparisonDialog::refreshCharts);
    ui_->comparisonRule->setText(qtTrId("repeat.folderLineRule"));
    refreshCharts();
}
RepeatabilityComparisonDialog::~RepeatabilityComparisonDialog() = default;

void RepeatabilityComparisonDialog::refreshCharts() {
    const std::string id = ui_->comparisonMetric->currentText().toStdString();
    const auto def = snapshot_->session.metrics.find(id);
    if (def == snapshot_->session.metrics.end() || snapshot_->analyses.size() < 2) return;
    const QString filter = ui_->boxName->text().trimmed();
    std::set<BoxKey> matches;
    if (!filter.isEmpty()) {
        for (const auto& a : snapshot_->analyses) {
            const auto metric = a.metrics.find(id);
            if (metric == a.metrics.end()) continue;
            for (const auto& [key, stats] : metric->second) {
                (void)stats;
                if (QString::fromStdString(key.box) == filter || QString::fromStdString(key.component + "/" + key.box) == filter) matches.insert(key);
            }
        }
    }
    const bool summary = filter.isEmpty();
    const bool uniqueBox = matches.size() == 1;
    std::size_t completeGroups = 0;
    if (uniqueBox) for (const auto& a : snapshot_->analyses) {
        const auto metric = a.metrics.find(id);
        if (metric == a.metrics.end()) continue;
        const auto value = metric->second.find(*matches.begin());
        if (value != metric->second.end() && value->second.complete()) ++completeGroups;
    }
    QStringList labels;
    for (std::size_t g = 0; g < snapshot_->session.datasets.size(); ++g)
        labels << QString::number(g + 1) + QStringLiteral(". ") + QString::fromStdString(snapshot_->session.datasets[g].name);
    const auto draw = [&](RepeatabilityPlot* plot, Statistic statistic) {
        const auto comparison = compare(snapshot_->analyses, id, statistic);
        std::vector<RepeatabilityPoint> points;
        double maximum = 0;
        const double scale = statistic == Statistic::Pt ? 100.0 : 1.0;
        for (std::size_t g = 0; g < snapshot_->analyses.size(); ++g) {
            std::optional<double> value;
            bool complete = false;
            if (summary) { value = comparison.values[g]; complete = comparison.commonBoxes > 0; }
            else if (uniqueBox) {
                const auto metric = snapshot_->analyses[g].metrics.find(id);
                if (metric != snapshot_->analyses[g].metrics.end()) {
                    const auto s = metric->second.find(*matches.begin());
                    if (s != metric->second.end() && s->second.complete()) { value = valueOf(s->second, statistic); complete = true; }
                }
            }
            if (value) { *value *= scale; maximum = std::max(maximum, *value); }
            const auto& d = snapshot_->session.datasets[g];
            points.push_back({{d.folder, d.name}, value, complete});
        }
        std::optional<double> threshold;
        if (statistic == Statistic::Pt && def->second.tolerance) { threshold = def->second.ptLimit * 100; maximum = std::max(maximum, *threshold); }
        const QString title = QString::fromStdString(id) + (statistic == Statistic::Pt ? QStringLiteral(" · P/T") : QStringLiteral(" · Range"));
        plot->setSeries(std::move(points), title, statistic == Statistic::Pt ? QStringLiteral("%") : QString::fromStdString(def->second.unit), threshold, 0, std::max(0.001, maximum * 1.08));
        plot->setXAxis(qtTrId("repeat.foldersAxis"), labels);
    };
    draw(ui_->ptComparison, Statistic::Pt); draw(ui_->rangeComparison, Statistic::Range);
    QString status;
    if (summary) status = qtTrId("repeat.folderLineSummary").arg(compare(snapshot_->analyses, id, Statistic::Range).commonBoxes);
    else if (matches.empty()) status = qtTrId("repeat.boxNotFound");
    else if (!uniqueBox) status = qtTrId("repeat.boxAmbiguous");
    else status = QString::fromStdString(matches.begin()->component + "/" + matches.begin()->box) + QStringLiteral(" · ") + qtTrId("repeat.boxCompleteGroups").arg(completeGroups).arg(snapshot_->analyses.size());
    if (!def->second.tolerance) status += QStringLiteral(" ") + qtTrId("repeat.noSpec");
    ui_->comparisonStatus->setText(status);
}
}  // namespace aoi::ui
