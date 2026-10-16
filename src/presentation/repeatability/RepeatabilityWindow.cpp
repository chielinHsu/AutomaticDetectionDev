#include "presentation/repeatability/RepeatabilityWindow.h"

#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QHeaderView>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <algorithm>
#include "presentation/repeatability/RepeatabilityPlot.h"
#include "presentation/repeatability/RepeatabilityComparisonDialog.h"
#include "presentation/theme/ThemeManager.h"
#include "ui_RepeatabilityWindow.h"

namespace aoi::ui {
using namespace repeatability;
RepeatabilityWindow::RepeatabilityWindow(std::shared_ptr<IRepeatabilitySource> source, ThemeManager& theme, QWidget* parent)
    : QWidget(parent, Qt::Window), ui_(std::make_unique<Ui::RepeatabilityWindow>()),
      presenter_(std::make_unique<RepeatabilityPresenter>(std::move(source))) {
    ui_->setupUi(this);
    resize(1400, 920); setMinimumSize(940, 620);
    ui_->mainSplitter->setSizes({190, 1100}); ui_->resultsSplitter->setSizes({600, 230});
    connect(&theme, &ThemeManager::changed, this, [this] { refreshPlots(); refreshTable(); });
    connect(presenter_.get(), &RepeatabilityPresenter::changed, this, &RepeatabilityWindow::refresh);
    connect(presenter_.get(), &RepeatabilityPresenter::busyChanged, this, &RepeatabilityWindow::updateBusy);
    connect(presenter_.get(), &RepeatabilityPresenter::failed, this, [this](const QString& message) {
        QMessageBox::warning(this, qtTrId("repeat.title"), qtTrId("repeat.error") + QStringLiteral("\n") + message);
    });
    connect(presenter_.get(), &RepeatabilityPresenter::exported, this, &RepeatabilityWindow::exportCharts, Qt::QueuedConnection);
    connect(ui_->addFolder, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getExistingDirectory(this, qtTrId("repeat.add"));
        if (!path.isEmpty()) loadFolder(path);
    });
    connect(ui_->openSession, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, qtTrId("repeat.open"), {}, QStringLiteral("JSON (*.json)"));
        if (!path.isEmpty()) presenter_->loadSession(path);
    });
    connect(ui_->saveSession, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this, qtTrId("repeat.save"), QStringLiteral("repeatability.json"), QStringLiteral("JSON (*.json)"));
        if (!path.isEmpty()) presenter_->saveSession(path);
    });
    connect(ui_->exportResults, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, qtTrId("repeat.export"));
        if (folder.isEmpty()) return;
        const QDir dir(folder);
        if ((!dir.entryList({QStringLiteral("chart-*.png"), QStringLiteral("summary.csv"), QStringLiteral("details.csv")}, QDir::Files).isEmpty()) &&
            QMessageBox::question(this, qtTrId("repeat.export"), qtTrId("repeat.overwrite")) != QMessageBox::Yes) return;
        presenter_->exportTables(folder, selectedMetrics(), statistic(), static_cast<std::size_t>(std::max(0, ui_->baseline->currentIndex())));
    });
    connect(ui_->cancel, &QPushButton::clicked, presenter_.get(), &RepeatabilityPresenter::cancel);
    connect(ui_->metricSettings, &QPushButton::clicked, this, &RepeatabilityWindow::editMetrics);
    connect(ui_->compareLines, &QPushButton::clicked, this, [this] {
        RepeatabilityComparisonDialog dialog(presenter_->snapshot(), selectedMetrics(), this);
        dialog.exec();
    });
    connect(ui_->metricsList, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        const std::string id = item->data(Qt::UserRole).toString().toStdString();
        if (item->checkState() == Qt::Checked) checked_.insert(id); else checked_.erase(id);
        refreshPlots(); refreshTable(); updateBusy();
    });
    connect(ui_->statistic, &QComboBox::currentIndexChanged, this, [this] { refreshPlots(); refreshTable(); });
    connect(ui_->baseline, &QComboBox::currentIndexChanged, this, &RepeatabilityWindow::refreshTable);
    connect(ui_->scope, &QComboBox::currentIndexChanged, this, &RepeatabilityWindow::refreshTable);
    retranslate(); refresh();
}
RepeatabilityWindow::~RepeatabilityWindow() = default;

Statistic RepeatabilityWindow::statistic() const { return static_cast<Statistic>(ui_->statistic->currentIndex()); }
std::vector<std::string> RepeatabilityWindow::selectedMetrics() const {
    std::vector<std::string> out;
    for (int i = 0; i < ui_->metricsList->count(); ++i) {
        const auto* item = ui_->metricsList->item(i);
        if (item->checkState() == Qt::Checked) out.push_back(item->data(Qt::UserRole).toString().toStdString());
    }
    return out;
}
void RepeatabilityWindow::retranslate() {
    setWindowTitle(qtTrId("repeat.title"));
    for (const auto& [button, id] : std::vector<std::pair<QPushButton*, const char*>>{{ui_->addFolder,"repeat.add"}, {ui_->openSession,"repeat.open"},
         {ui_->saveSession,"repeat.save"}, {ui_->exportResults,"repeat.export"}, {ui_->compareLines,"repeat.overlay"}, {ui_->cancel,"repeat.cancel"}, {ui_->metricSettings,"repeat.settings"}})
        button->setText(qtTrId(id));
    ui_->metricsLabel->setText(qtTrId("repeat.metrics")); ui_->statisticLabel->setText(qtTrId("repeat.statistic"));
    ui_->rules->setText(qtTrId("repeat.rules")); ui_->baselineLabel->setText(qtTrId("repeat.baseline"));
    {
        QSignalBlocker block(ui_->statistic); const int old = ui_->statistic->currentIndex();
        ui_->statistic->clear(); ui_->statistic->addItems({QStringLiteral("Avg"), QStringLiteral("Range"), QStringLiteral("STDEV"), QStringLiteral("P/T")});
        ui_->statistic->setCurrentIndex(old < 0 ? 3 : old);
    }
    {
        QSignalBlocker block(ui_->scope); const int old = ui_->scope->currentIndex();
        ui_->scope->clear(); ui_->scope->addItems({qtTrId("repeat.summary"), qtTrId("repeat.selectedBox")}); ui_->scope->setCurrentIndex(std::max(0, old));
    }
}
void RepeatabilityWindow::refresh() {
    const auto& s = presenter_->state();
    std::set<std::string> available;
    for (const DatasetAnalysis& a : s.analyses) for (const auto& [id, values] : a.metrics) { (void)values; available.insert(id); }
    QSignalBlocker block(ui_->metricsList);
    ui_->metricsList->clear();
    std::vector<std::string> ordered{"CO", "XO", "YO", "RO"};
    for (const std::string& id : available)
        if (std::find(ordered.begin(), ordered.end(), id) == ordered.end()) ordered.push_back(id);
    for (const std::string& id : ordered) {
        if (!available.contains(id)) continue;
        if (seen_.insert(id).second) checked_.insert(id);
        auto* item = new QListWidgetItem(QString::fromStdString(id + " (" + s.session.metrics.at(id).unit + ")"), ui_->metricsList);
        item->setData(Qt::UserRole, QString::fromStdString(id));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable); item->setCheckState(checked_.contains(id) ? Qt::Checked : Qt::Unchecked);
    }
    {
        QSignalBlocker baselineBlock(ui_->baseline); const QVariant old = ui_->baseline->currentData(); ui_->baseline->clear();
        for (const Dataset& d : s.session.datasets) ui_->baseline->addItem(QString::fromStdString(d.name), QString::fromStdString(d.folder));
        if (ui_->baseline->count()) ui_->baseline->setCurrentIndex(std::max(0, ui_->baseline->findData(old)));
    }
    refreshPlots(); refreshTable(); updateBusy();
}
void RepeatabilityWindow::updateBusy() {
    const bool busy = presenter_->busy(), empty = presenter_->state().session.datasets.empty();
    ui_->addFolder->setEnabled(!busy && presenter_->state().session.datasets.size() < 4);
    ui_->openSession->setEnabled(!busy); ui_->saveSession->setEnabled(!busy && !empty);
    ui_->exportResults->setEnabled(!busy && !empty && !selectedMetrics().empty());
    ui_->compareLines->setEnabled(!busy && presenter_->state().analyses.size() >= 2 && !selectedMetrics().empty());
    ui_->metricSettings->setEnabled(!busy); ui_->cancel->setVisible(busy);
    ui_->metricsList->setEnabled(!busy); ui_->statistic->setEnabled(!busy);
    if (busy) ui_->status->setText(qtTrId("repeat.loading"));
    else {
        std::size_t incomplete = 0, runs = 0;
        for (const auto& a : presenter_->state().analyses) incomplete += a.incomplete;
        for (const auto& d : presenter_->state().session.datasets) runs += d.runs.size();
        ui_->status->setText(qtTrId("repeat.status").arg(presenter_->state().session.datasets.size()).arg(runs).arg(incomplete));
    }
}
void RepeatabilityWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) { retranslate(); refresh(); }
    QWidget::changeEvent(event);
}
void RepeatabilityWindow::exportCharts(const QString& folder) {
    int i = 0;
    for (RepeatabilityPlot* plot : ui_->plotsHost->findChildren<RepeatabilityPlot*>()) {
        const QString path = QDir(folder).filePath(QStringLiteral("chart-%1.png").arg(++i));
        if (!plot->grab().save(path)) { QMessageBox::warning(this, qtTrId("repeat.export"), qtTrId("repeat.error") + path); return; }
    }
    QMessageBox::information(this, qtTrId("repeat.export"), qtTrId("repeat.exported"));
}
}  // namespace aoi::ui
