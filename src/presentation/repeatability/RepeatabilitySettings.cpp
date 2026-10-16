#include "presentation/repeatability/RepeatabilityWindow.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>

namespace aoi::ui {
void RepeatabilityWindow::editMetrics() {
    const auto& metrics = presenter_->state().session.metrics;
    QDialog dialog(this); dialog.setWindowTitle(qtTrId("repeat.settings")); dialog.resize(860, 560);
    auto* layout = new QVBoxLayout(&dialog);
    auto* note = new QLabel(qtTrId("repeat.settingsNote"), &dialog); note->setWordWrap(true); layout->addWidget(note);
    auto* table = new QTableWidget(static_cast<int>(metrics.size()), 5, &dialog); layout->addWidget(table);
    table->setHorizontalHeaderLabels({qtTrId("repeat.metrics"), qtTrId("repeat.unit"), qtTrId("repeat.multiplier"), QStringLiteral("SPEC"), qtTrId("repeat.ptLimit")});
    int row = 0;
    for (const auto& [id, def] : metrics) {
        auto* name = new QTableWidgetItem(QString::fromStdString(id)); name->setFlags(name->flags() & ~Qt::ItemIsEditable); table->setItem(row, 0, name);
        table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(def.unit)));
        table->setItem(row, 2, new QTableWidgetItem(QString::number(def.multiplier, 'g', 12)));
        table->setItem(row, 3, new QTableWidgetItem(def.tolerance ? QString::number(*def.tolerance, 'g', 12) : QString()));
        table->setItem(row, 4, new QTableWidgetItem(QString::number(def.ptLimit * 100, 'g', 12))); ++row;
    }
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog); layout->addWidget(buttons);
    buttons->button(QDialogButtonBox::Ok)->setText(qtTrId("repeat.apply"));
    buttons->button(QDialogButtonBox::Cancel)->setText(qtTrId("repeat.cancel"));
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        auto changed = metrics;
        for (int r = 0; r < table->rowCount(); ++r) {
            auto& def = changed.at(table->item(r, 0)->text().toStdString());
            bool factorOk = false, specOk = false, limitOk = false;
            const double factor = table->item(r, 2)->text().toDouble(&factorOk);
            const QString specText = table->item(r, 3)->text().trimmed();
            const double spec = specText.toDouble(&specOk);
            const double limit = table->item(r, 4)->text().toDouble(&limitOk);
            const QString unit = table->item(r, 1)->text().trimmed();
            if (unit.isEmpty() || !factorOk || !std::isfinite(factor) || factor <= 0 ||
                (!specText.isEmpty() && (!specOk || !std::isfinite(spec) || spec <= 0)) || !limitOk || !std::isfinite(limit) || limit <= 0) {
                QMessageBox::warning(&dialog, qtTrId("repeat.settings"), qtTrId("repeat.invalidSettings")); return;
            }
            def.unit = unit.toStdString(); def.multiplier = factor;
            def.tolerance = specText.isEmpty() ? std::optional<double>() : std::optional<double>(spec); def.ptLimit = limit / 100;
        }
        presenter_->configure(changed); dialog.accept();
    });
    dialog.exec();
}
}  // namespace aoi::ui
