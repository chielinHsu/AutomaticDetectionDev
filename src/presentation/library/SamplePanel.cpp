#include "presentation/library/SamplePanel.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <algorithm>
#include <cmath>

#include "presentation/i18n/Translations.h"
#include "ui_SamplePanel.h"

namespace aoi::ui {

namespace {

constexpr int kIdRole = Qt::UserRole + 1;

// 整數就不印小數（拖出來的框可能差 1e-7，不要印成 60.0）；其餘一位小數。
QString px(double v) { return QString::number(v, 'f', std::abs(v - std::round(v)) < 1e-6 ? 0 : 1); }

}  // namespace

SamplePanel::SamplePanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::SamplePanel>()) {
    // 版面在 SamplePanel.ui：篩選欄（停用的佔位）、標題、清單（圖示在左、文字在右）、空狀態文字。
    ui_->setupUi(this);
    filter_ = ui_->libraryFilter;
    header_ = ui_->header;
    list_ = ui_->list;
    empty_ = ui_->libraryEmpty;
    empty_->setForegroundRole(QPalette::PlaceholderText);
    header_->setForegroundRole(QPalette::PlaceholderText);
    list_->setIconSize(QSize(96, 96));
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (syncing_ || item == nullptr) return;
        emit sampleClicked(item->data(kIdRole).toInt());
    });
    retranslate();
}

SamplePanel::~SamplePanel() = default;

std::vector<int> SamplePanel::selectedIds() const {
    std::vector<int> ids;
    for (QListWidgetItem* item : list_->selectedItems()) ids.push_back(item->data(kIdRole).toInt());
    return ids;
}

QString SamplePanel::summaryOf(const SampleEntry& e) const {
    const double um = samples_ ? samples_->umPerPixel : 0.0;
    const QString boxes = QString::number(e.boxCount);
    if (um > 0.0) {
        return uiText("ui.library.sample.summaryUm", {{"w", px(e.width)},
                                                       {"h", px(e.height)},
                                                       {"wum", QString::number(qRound(e.width * um))},
                                                       {"hum", QString::number(qRound(e.height * um))},
                                                       {"boxes", boxes}});
    }
    return uiText("ui.library.sample.summary", {{"w", px(e.width)}, {"h", px(e.height)}, {"boxes", boxes}});
}

void SamplePanel::rebuild() {
    syncing_ = true;
    const std::vector<int> selected = selectedIds();
    list_->clear();
    const std::size_t n = samples_ ? samples_->entries.size() : 0;
    if (samples_) {
        for (const SampleEntry& e : samples_->entries) {
            auto* item = new QListWidgetItem(list_);
            item->setData(kIdRole, e.sceneId);
            if (!e.thumbnail.isNull()) item->setIcon(QIcon(QPixmap::fromImage(e.thumbnail)));
            QString text = e.name;
            if (!e.typeLabel.isEmpty()) text += QStringLiteral(" · ") + e.typeLabel;
            item->setText(text + QLatin1Char('\n') + summaryOf(e));
            item->setToolTip(uiText("ui.library.sample.tip", {{"name", e.name}}));
            if (std::find(selected.begin(), selected.end(), e.sceneId) != selected.end()) item->setSelected(true);
        }
    }
    header_->setText(uiText("ui.library.boardSamples", {{"count", QString::number(n)}}));
    header_->setVisible(n > 0);
    list_->setVisible(n > 0);
    empty_->setVisible(n == 0);
    syncing_ = false;
}

void SamplePanel::setSamples(SampleListPtr samples) {
    if (samples == samples_) return;  // 同一份：沒有變
    samples_ = std::move(samples);
    rebuild();
}

void SamplePanel::setSelection(const std::vector<int>& sceneIds) {
    syncing_ = true;
    for (int i = 0; i < list_->count(); ++i) {
        QListWidgetItem* item = list_->item(i);
        const int id = item->data(kIdRole).toInt();
        item->setSelected(std::find(sceneIds.begin(), sceneIds.end(), id) != sceneIds.end());
    }
    syncing_ = false;
}

void SamplePanel::retranslate() {
    filter_->setPlaceholderText(uiText("ui.library.filter"));
    empty_->setText(uiText("ui.library.empty"));
    rebuild();
}

}  // namespace aoi::ui
