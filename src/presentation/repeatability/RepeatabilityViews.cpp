#include "presentation/repeatability/RepeatabilityWindow.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include "presentation/repeatability/RepeatabilityPlot.h"
#include "ui_RepeatabilityWindow.h"

namespace aoi::ui {
using namespace repeatability;
namespace {
QString q(const std::string& s) { return QString::fromStdString(s); }
QString formatted(std::optional<double> v, Statistic statistic) {
    return v ? QString::number(*v * (statistic == Statistic::Pt ? 100 : 1), 'f', 3) + (statistic == Statistic::Pt ? QStringLiteral("%") : QString()) : QStringLiteral("—");
}
void clearLayout(QLayout* layout) { while (QLayoutItem* item = layout->takeAt(0)) { delete item->widget(); delete item; } }
}
void RepeatabilityWindow::refreshPlots() {
    clearLayout(ui_->plotsLayout);
    const auto& state = presenter_->state();
    const auto ids = selectedMetrics();
    const auto statisticValue = statistic();
    const bool multi = state.analyses.size() > 1;
    for (std::size_t g = 0; g < state.analyses.size(); ++g) {
        const Dataset& d = state.session.datasets[g];
        auto* group = new QGroupBox(q(d.name), ui_->plotsHost);
        group->setObjectName(QStringLiteral("repeatFolder%1").arg(g));
        auto* layout = new QVBoxLayout(group);
        auto* controls = new QHBoxLayout;
        const std::size_t boxes = state.analyses[g].metrics.empty() ? 0 : state.analyses[g].metrics.begin()->second.size();
        auto* source = new QLabel(q(d.folder) + QLatin1Char('\n') + qtTrId("repeat.groupInfo").arg(d.runs.size()).arg(boxes), group);
        source->setWordWrap(true); controls->addWidget(source, 1);
        auto& choice = folderMetrics_[d.folder];
        if (!ids.empty() && std::find(ids.begin(), ids.end(), choice) == ids.end()) choice = ids.front();
        if (multi) {
            auto* combo = new QComboBox(group);
            combo->setObjectName(QStringLiteral("repeatMetric%1").arg(g));
            combo->setAccessibleName(qtTrId("repeat.metrics"));
            for (const std::string& id : ids) combo->addItem(q(id));
            combo->setCurrentText(q(choice)); combo->setEnabled(!ids.empty()); controls->addWidget(combo);
            connect(combo, &QComboBox::currentTextChanged, this, [this, folder = d.folder](const QString& id) {
                folderMetrics_[folder] = id.toStdString();
                // Defer rebuilding so the signal sender survives its own currentTextChanged delivery.
                QMetaObject::invokeMethod(this, [this] { refreshPlots(); }, Qt::QueuedConnection);
            });
        }
        auto* close = new QPushButton(qtTrId("repeat.close"), group); controls->addWidget(close);
        connect(close, &QPushButton::clicked, this, [this, folder = d.folder] {
            QMetaObject::invokeMethod(this, [this, folder] {
                const auto& datasets = presenter_->state().session.datasets;
                const auto found = std::find_if(datasets.begin(), datasets.end(), [&](const Dataset& item) { return item.folder == folder; });
                if (found != datasets.end()) presenter_->removeFolder(static_cast<std::size_t>(found - datasets.begin()));
            }, Qt::QueuedConnection);
        });
        layout->addLayout(controls);
        auto* charts = new QGridLayout; layout->addLayout(charts);
        const auto shown = multi ? (ids.empty() ? std::vector<std::string>() : std::vector<std::string>{choice}) : ids;
        int chartIndex = 0;
        for (const std::string& id : shown) {
            const MetricDefinition& def = state.session.metrics.at(id);
            std::set<BoxKey> keys;
            double minimum = 0.0, maximum = 0.0;
            const double scale = statisticValue == Statistic::Pt ? 100.0 : 1.0;
            for (const DatasetAnalysis& a : state.analyses) {
                const auto m = a.metrics.find(id);
                if (m == a.metrics.end()) continue;
                for (const auto& [key, s] : m->second) {
                    keys.insert(key);
                    if (const auto v = valueOf(s, statisticValue)) { minimum = std::min(minimum, *v * scale); maximum = std::max(maximum, *v * scale); }
                }
            }
            std::optional<double> threshold;
            if (statisticValue == Statistic::Pt && def.tolerance) { threshold = def.ptLimit * 100; maximum = std::max(maximum, *threshold); }
            const double padding = std::max(0.001, (maximum - minimum) * 0.08);
            if (minimum < 0) minimum -= padding;
            maximum += padding;
            std::vector<RepeatabilityPoint> points;
            const auto m = state.analyses[g].metrics.find(id);
            for (const BoxKey& key : keys) {
                RepeatabilityPoint p{key, {}, false};
                if (m != state.analyses[g].metrics.end()) {
                    const auto found = m->second.find(key);
                    if (found != m->second.end()) {
                        p.value = valueOf(found->second, statisticValue); if (p.value) *p.value *= scale;
                        p.complete = found->second.complete();
                    }
                }
                points.push_back(std::move(p));
            }
            auto* container = new QWidget(group); auto* inner = new QVBoxLayout(container); inner->setContentsMargins(0, 0, 0, 0);
            auto* plot = new RepeatabilityPlot(container);
            plot->setSeries(std::move(points), q(id) + QStringLiteral(" · ") + ui_->statistic->currentText() + QStringLiteral(" · ") + q(d.name), statisticValue == Statistic::Pt ? QStringLiteral("%") : q(def.unit), threshold, minimum, maximum);
            inner->addWidget(plot);
            if (m == state.analyses[g].metrics.end()) inner->addWidget(new QLabel(qtTrId("repeat.noMetric"), container));
            if (statisticValue == Statistic::Pt && !def.tolerance) inner->addWidget(new QLabel(qtTrId("repeat.noSpec"), container));
            charts->addWidget(container, chartIndex / (multi ? 1 : 2), chartIndex % (multi ? 1 : 2)); ++chartIndex;
            connect(plot, &RepeatabilityPlot::pointActivated, this, [this, g, id](const QString& component, const QString& box) {
                selectedBox_ = {component.toStdString(), box.toStdString()}; hasSelectedBox_ = true;
                ui_->scope->setCurrentIndex(1); refreshTable(); showDetails(g, id, component, box);
            });
        }
        if (shown.empty()) layout->addWidget(new QLabel(qtTrId("repeat.empty"), group));
        const int columns = multi ? 2 : 1;
        ui_->plotsLayout->addWidget(group, static_cast<int>(g) / columns, static_cast<int>(g) % columns);
    }
    if (state.analyses.empty()) ui_->plotsLayout->addWidget(new QLabel(qtTrId("repeat.empty"), ui_->plotsHost), 0, 0);
}

void RepeatabilityWindow::refreshTable() {
    const auto& state = presenter_->state(); const auto ids = selectedMetrics(); const Statistic st = statistic();
    auto* table = ui_->comparisonTable; table->clear();
    table->setRowCount(static_cast<int>(ids.size())); table->setColumnCount(static_cast<int>(state.analyses.size()) + 2);
    QStringList headings{qtTrId("repeat.metrics"), qtTrId("repeat.common")};
    for (const Dataset& d : state.session.datasets) headings.push_back(q(d.name));
    table->setHorizontalHeaderLabels(headings); table->verticalHeader()->hide();
    ui_->comparisonLabel->setText(qtTrId(state.analyses.size() > 1 ? "repeat.comparison" : "repeat.summary"));
    const std::size_t baseline = static_cast<std::size_t>(std::max(0, ui_->baseline->currentIndex()));
    const bool boxScope = ui_->scope->currentIndex() == 1;
    ui_->comparisonNote->setText(boxScope ? (hasSelectedBox_ ? q(selectedBox_.component + "/" + selectedBox_.box) : qtTrId("repeat.pickBox")) : qtTrId("repeat.compareRule"));
    for (std::size_t row = 0; row < ids.size(); ++row) {
        const std::string& id = ids[row]; const auto& def = state.session.metrics.at(id);
        Comparison c = compare(state.analyses, id, st);
        if (boxScope) {
            c.commonBoxes = 0; c.values.assign(state.analyses.size(), {});
            if (hasSelectedBox_) {
                bool allComplete = true;
                for (std::size_t g = 0; g < state.analyses.size(); ++g) {
                    const auto m = state.analyses[g].metrics.find(id);
                    if (m == state.analyses[g].metrics.end()) { allComplete = false; continue; }
                    const auto s = m->second.find(selectedBox_);
                    if (s == m->second.end() || !s->second.complete()) { allComplete = false; continue; }
                    c.values[g] = valueOf(s->second, st);
                }
                if (allComplete && !state.analyses.empty()) c.commonBoxes = 1;
            }
        }
        const int rr = static_cast<int>(row);
        table->setItem(rr, 0, new QTableWidgetItem(q(id) + QStringLiteral(" (") + (st == Statistic::Pt ? QStringLiteral("%") : q(def.unit)) + QLatin1Char(')')));
        table->setItem(rr, 1, new QTableWidgetItem(QString::number(c.commonBoxes)));
        for (std::size_t g = 0; g < c.values.size(); ++g) {
            QString text = st == Statistic::Pt && !def.tolerance ? qtTrId("repeat.specMissingShort") : formatted(c.values[g], st);
            if (st == Statistic::Pt && c.values[g] && *c.values[g] > def.ptLimit) text += QStringLiteral(" · ") + qtTrId("repeat.exceeded");
            if (state.analyses.size() > 1 && c.commonBoxes && g != baseline && c.values[g] && c.values[baseline]) {
                const double delta = (*c.values[g] - *c.values[baseline]) * (st == Statistic::Pt ? 100 : 1);
                text += QStringLiteral("\nΔ ") + (delta >= 0 ? QStringLiteral("+") : QString()) + QString::number(delta, 'f', 3) +
                    (st == Statistic::Pt ? qtTrId("repeat.percentagePoints") : q(def.unit));
            }
            auto* item = new QTableWidgetItem(text); item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (st == Statistic::Pt && c.values[g] && *c.values[g] > def.ptLimit) item->setBackground(palette().alternateBase());
            table->setItem(rr, static_cast<int>(g) + 2, item);
        }
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true); table->resizeRowsToContents();
}

void RepeatabilityWindow::showDetails(std::size_t group, const std::string& metric, const QString& component, const QString& box) {
    const auto& session = presenter_->state().session;
    if (group >= session.datasets.size()) return;
    QDialog dialog(this); dialog.setWindowTitle(component + QLatin1Char('/') + box + QStringLiteral(" — ") + q(metric)); dialog.resize(900, 550);
    auto* layout = new QVBoxLayout(&dialog); layout->addWidget(new QLabel(qtTrId("repeat.raw"), &dialog));
    auto* table = new QTableWidget(&dialog); layout->addWidget(table);
    const auto ids = selectedMetrics();
    table->setColumnCount(static_cast<int>(ids.size()) + 3);
    QStringList headers{qtTrId("repeat.file"), qtTrId("repeat.time"), qtTrId("repeat.unit")};
    for (const auto& id : ids) headers.push_back(q(id)); table->setHorizontalHeaderLabels(headers);
    const auto& runs = session.datasets[group].runs; table->setRowCount(static_cast<int>(runs.size()));
    for (std::size_t i = 0; i < runs.size(); ++i) {
        const auto& run = runs[i]; const int r = static_cast<int>(i);
        table->setItem(r, 0, new QTableWidgetItem(q(run.source))); table->setItem(r, 1, new QTableWidgetItem(q(run.timestamp))); table->setItem(r, 2, new QTableWidgetItem(q(run.unit)));
        const auto found = run.boxes.find({component.toStdString(), box.toStdString()});
        for (std::size_t j = 0; j < ids.size(); ++j) {
            std::optional<double> value;
            if (found != run.boxes.end()) { const auto v = found->second.find(ids[j]); if (v != found->second.end()) value = v->second; }
            table->setItem(r, static_cast<int>(j) + 3, new QTableWidgetItem(value ? QString::number(*value, 'g', 12) : QStringLiteral("—")));
        }
    }
    table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->resizeColumnsToContents();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog); layout->addWidget(buttons);
    buttons->button(QDialogButtonBox::Close)->setText(qtTrId("repeat.close"));
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); dialog.exec();
}
}  // namespace aoi::ui
