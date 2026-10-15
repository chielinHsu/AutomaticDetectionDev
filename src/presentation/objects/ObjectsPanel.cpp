#include "presentation/objects/ObjectsPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFontMetrics>
#include <QGroupBox>
#include <QHeaderView>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QTreeView>
#include <algorithm>
#include <cmath>

#include "presentation/i18n/Translations.h"
#include "ui_ObjectsPanel.h"

namespace aoi::ui {

namespace {

const char* stateKey(OverlayState s) {
    switch (s) {
        case OverlayState::Sample: return "state.sample";
        case OverlayState::Found: return "state.found";
        case OverlayState::Inferred: return "state.inferred";
        case OverlayState::Rejected: return "state.rejected";
    }
    return "state.found";
}

}  // namespace

ObjectsPanel::ObjectsPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::ObjectsPanel>()) {
    // 版面在 ObjectsPanel.ui。晶片 stateChip0..3 依 OverlayState 的順序（Sample、Found、Inferred、Rejected），
    // 可勾選、預設勾選、只顯示文字（狀態名稱＋數量，不只靠顏色）。樹：uniformRowHeights（5 000 列時 View 不必逐列量高度）、
    // 多選、整列選取、不能編輯。
    ui_->setupUi(this);
    chips_ = {ui_->stateChip0, ui_->stateChip1, ui_->stateChip2, ui_->stateChip3};
    for (QToolButton* b : chips_) connect(b, &QToolButton::toggled, this, [this] { onChipToggled(); });

    tree_ = ui_->tree;
    tree_->setModel(&model_);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(ObjectsModel::NameColumn, QHeaderView::Stretch);
    // 狀態與分數欄用固定寬度（由字型量出來，resizeColumns），不用 ResizeToContents：後者每次資料變動都要掃描上千列的文字寬度，
    // 5 000 列時光這一項就花掉整個重繪預算（實測，見 docs/steps/S04-presenter/perf.md）。
    tree_->header()->setSectionResizeMode(ObjectsModel::StateColumn, QHeaderView::Fixed);
    tree_->header()->setSectionResizeMode(ObjectsModel::ScoreColumn, QHeaderView::Fixed);

    empty_ = ui_->empty;
    empty_->setForegroundRole(QPalette::PlaceholderText);

    connect(tree_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        if (syncing_) return;
        emit selectionChangedByUser(selectedIds());
    });

    // 選取資訊表單（2026-10-14）。spin box 的 keyboardTracking 關（.ui）：打字時不觸發，按 Enter 或離開欄位（editingFinished）才送出一次。
    selectionBox_ = ui_->selectionBox;
    selectionSummary_ = ui_->selectionSummary;
    centerLabel_ = ui_->centerLabel;
    centerX_ = ui_->centerX;
    centerY_ = ui_->centerY;
    sizeLabel_ = ui_->sizeLabel;
    sizeW_ = ui_->sizeW;
    sizeH_ = ui_->sizeH;
    sizeUm_ = ui_->sizeUm;
    lockRatio_ = ui_->lockRatio;
    angleLabel_ = ui_->angleLabel;
    angleValue_ = ui_->angleValue;
    boxesLabel_ = ui_->boxesLabel;
    boxesValue_ = ui_->boxesValue;
    applyToAll_ = ui_->applyToAll;
    matchLabel_ = ui_->matchLabel;
    matchValue_ = ui_->matchValue;
    matchModeLabel_ = ui_->matchModeLabel;
    matchMode_ = ui_->matchMode;
    matchMode_->addItems({QString(), QString()});  // 0 ＝ 灰階、1 ＝ 邊緣（文字在 retranslate）
    matchGrowLabel_ = ui_->matchGrowLabel;
    matchGrow_ = ui_->matchGrow;
    matchClear_ = ui_->matchClear;
    selectionNote_ = ui_->selectionNote;
    selectionNote_->setForegroundRole(QPalette::PlaceholderText);
    sizeUm_->setForegroundRole(QPalette::PlaceholderText);
    connect(centerX_, &QDoubleSpinBox::editingFinished, this, &ObjectsPanel::commitCenter);
    connect(centerY_, &QDoubleSpinBox::editingFinished, this, &ObjectsPanel::commitCenter);
    connect(sizeW_, &QDoubleSpinBox::editingFinished, this, &ObjectsPanel::commitSize);
    connect(sizeH_, &QDoubleSpinBox::editingFinished, this, &ObjectsPanel::commitSize);
    // 〔鎖定寬高比〕：改一邊、另一邊跟著（只是表單的行為；valueChanged 在 keyboardTracking 關時只在步進或 Enter／離開時發出）。
    connect(sizeW_, &QDoubleSpinBox::valueChanged, this, [this] { followRatio(true); });
    connect(sizeH_, &QDoubleSpinBox::valueChanged, this, [this] { followRatio(false); });
    connect(applyToAll_, &QPushButton::clicked, this, [this] {
        if (info_.count == 1 && info_.canResize) emit applySizeToAllRequested();
    });
    // 比對區（ADR-026）：特徵選了就送出；外擴是 keyboardTracking 關的 spin box，按 Enter、離開欄位或按上下箭頭才送出一次。
    connect(matchMode_, &QComboBox::activated, this, [this](int index) {
        if (!infoSyncing_ && info_.count == 1 && info_.canEditMatch && index != info_.matchMode) emit matchModeEdited(index);
    });
    connect(matchGrow_, &QSpinBox::valueChanged, this, [this](int value) {
        if (!infoSyncing_ && info_.count == 1 && info_.matchGrow >= 0 && value != info_.matchGrow) emit matchGrowEdited(value);
    });
    connect(matchClear_, &QPushButton::clicked, this, [this] {
        if (info_.count == 1 && info_.canEditMatch && info_.hasMatchRegion) emit matchClearRequested();
    });
    setSelectionInfo(SelectionInfoView{});

    retranslate();
    updateChips();
}

// ---- 選取資訊表單 ----------------------------------------------------------------------------------------

void ObjectsPanel::followRatio(bool widthChanged) {
    if (infoSyncing_ || !lockRatio_->isChecked() || info_.width <= 0.0 || info_.height <= 0.0) return;
    const double ratio = info_.width / info_.height;
    infoSyncing_ = true;
    if (widthChanged) sizeH_->setValue(sizeW_->value() / ratio);
    else sizeW_->setValue(sizeH_->value() * ratio);
    infoSyncing_ = false;
    updateSelectionTexts();
}

void ObjectsPanel::commitCenter() {
    if (infoSyncing_ || info_.count != 1 || !info_.canMove) return;
    const double x = centerX_->value();
    const double y = centerY_->value();
    if (std::abs(x - info_.x) < 1e-6 && std::abs(y - info_.y) < 1e-6) return;
    emit centerEdited(x, y);
}

void ObjectsPanel::commitSize() {
    if (infoSyncing_ || info_.count != 1 || !info_.canResize) return;
    const double w = sizeW_->value();
    const double h = sizeH_->value();
    if (std::abs(w - info_.width) < 1e-6 && std::abs(h - info_.height) < 1e-6) return;
    emit sizeEdited(w, h);
}

void ObjectsPanel::updateSelectionTexts() {
    if (info_.count != 1) {
        selectionSummary_->setText(info_.count == 0 ? uiText("ui.selection.none")
                                                    : uiText("ui.selection.multi", {{"count", QString::number(info_.count)}}));
        sizeUm_->clear();
        selectionNote_->clear();
        selectionNote_->setVisible(false);
        return;
    }
    selectionSummary_->setText(info_.typeLabel.isEmpty()
                                   ? info_.name
                                   : uiText("ui.selection.summary", {{"name", info_.name}, {"type", info_.typeLabel}}));
    if (info_.umPerPixel > 0.0) {
        sizeUm_->setText(uiText("ui.selection.um", {{"w", QString::number(qRound(sizeW_->value() * info_.umPerPixel))},
                                                     {"h", QString::number(qRound(sizeH_->value() * info_.umPerPixel))}}));
    } else {
        sizeUm_->clear();
    }
    sizeUm_->setVisible(info_.umPerPixel > 0.0);
    angleValue_->setText(QStringLiteral("%1°").arg(QString::number(info_.angleDeg, 'f', 1)));
    boxesValue_->setText(QString::number(info_.boxCount));
    selectionNote_->setText(info_.canResize ? QString() : uiText("ui.selection.previewOnlyMove"));
    selectionNote_->setVisible(!info_.canResize);
}

void ObjectsPanel::setSelectionInfo(const SelectionInfoView& info) {
    infoSyncing_ = true;
    info_ = info;
    const bool one = info.count == 1;
    {
        const QSignalBlocker bx(centerX_);
        const QSignalBlocker by(centerY_);
        const QSignalBlocker bw(sizeW_);
        const QSignalBlocker bh(sizeH_);
        centerX_->setValue(one ? info.x : 0.0);
        centerY_->setValue(one ? info.y : 0.0);
        sizeW_->setValue(one ? info.width : 0.0);
        sizeH_->setValue(one ? info.height : 0.0);
        const QSignalBlocker bm(matchMode_);
        const QSignalBlocker bg(matchGrow_);
        matchMode_->setCurrentIndex(one ? info.matchMode : 0);
        matchGrow_->setValue(one && info.matchGrow >= 0 ? info.matchGrow : 0);
    }
    matchValue_->setText(one ? info.matchText : QString());
    matchMode_->setEnabled(one && info.canEditMatch);
    matchGrow_->setEnabled(one && info.canEditMatch && info.matchGrow >= 0);
    matchClear_->setEnabled(one && info.canEditMatch && info.hasMatchRegion);
    for (QWidget* w : {static_cast<QWidget*>(matchLabel_), static_cast<QWidget*>(matchValue_), static_cast<QWidget*>(matchModeLabel_),
                       static_cast<QWidget*>(matchGrowLabel_)})
        w->setEnabled(one && info.canEditMatch);
    for (QWidget* w : {static_cast<QWidget*>(centerX_), static_cast<QWidget*>(centerY_)}) w->setEnabled(one && info.canMove);
    for (QWidget* w : {static_cast<QWidget*>(sizeW_), static_cast<QWidget*>(sizeH_), static_cast<QWidget*>(lockRatio_),
                       static_cast<QWidget*>(applyToAll_)})
        w->setEnabled(one && info.canResize);
    for (QWidget* w : {static_cast<QWidget*>(centerLabel_), static_cast<QWidget*>(sizeLabel_), static_cast<QWidget*>(angleLabel_),
                       static_cast<QWidget*>(boxesLabel_), static_cast<QWidget*>(angleValue_), static_cast<QWidget*>(boxesValue_)})
        w->setEnabled(one);
    if (!one) {
        angleValue_->clear();
        boxesValue_->clear();
    }
    updateSelectionTexts();
    infoSyncing_ = false;
}

ObjectsPanel::~ObjectsPanel() = default;

void ObjectsPanel::resizeColumns() {
    const QFontMetrics fm(tree_->font());
    constexpr int kPadding = 16;
    int stateWidth = fm.horizontalAdvance(model_.headerData(ObjectsModel::StateColumn, Qt::Horizontal).toString());
    for (const char* key : {"state.sample", "state.found", "state.inferred", "state.rejected"})
        stateWidth = std::max(stateWidth, fm.horizontalAdvance(uiText(key)));
    int scoreWidth = fm.horizontalAdvance(model_.headerData(ObjectsModel::ScoreColumn, Qt::Horizontal).toString());
    for (const QString& s : {QStringLiteral("0.00"), QStringLiteral("×000"), uiText("origin.manual")})
        scoreWidth = std::max(scoreWidth, fm.horizontalAdvance(s));
    tree_->header()->resizeSection(ObjectsModel::StateColumn, stateWidth + kPadding);
    tree_->header()->resizeSection(ObjectsModel::ScoreColumn, scoreWidth + kPadding);
}

void ObjectsPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) resizeColumns();
    QWidget::changeEvent(event);
}

std::vector<int> ObjectsPanel::selectedIds() const {
    std::vector<int> ids;
    for (const QModelIndex& index : tree_->selectionModel()->selectedRows(ObjectsModel::NameColumn)) {
        const int id = model_.idAt(index);
        if (id >= 0 && std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    }
    return ids;
}

void ObjectsPanel::setRows(ObjectRowsPtr rows) {
    syncing_ = true;
    model_.setRows(std::move(rows));
    tree_->expand(model_.boardIndex());  // Board 展開；元件下的框群組預設收起
    syncing_ = false;
    updateChips();
}

void ObjectsPanel::setSelection(const std::vector<int>& sceneIds) {
    syncing_ = true;
    QItemSelection selection;
    QModelIndex first;
    for (int id : sceneIds) {
        const QModelIndex a = model_.indexForId(id, 0);
        if (!a.isValid()) continue;
        const QModelIndex b = model_.indexForId(id, ObjectsModel::ColumnCount - 1);
        selection.select(a, b);
        if (!first.isValid()) first = a;
    }
    tree_->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    if (first.isValid()) tree_->scrollTo(first);
    syncing_ = false;
}

void ObjectsPanel::setFilter(StateFilter filter) {
    for (OverlayState s : {OverlayState::Sample, OverlayState::Found, OverlayState::Inferred, OverlayState::Rejected}) {
        QToolButton* b = chips_[static_cast<std::size_t>(stateIndex(s))];
        const QSignalBlocker blocker(b);
        b->setChecked((filter & stateBit(s)) != 0);
    }
    model_.setFilter(filter);
    updateChips();
}

void ObjectsPanel::onChipToggled() {
    StateFilter mask = 0;
    for (OverlayState s : {OverlayState::Sample, OverlayState::Found, OverlayState::Inferred, OverlayState::Rejected}) {
        if (chips_[static_cast<std::size_t>(stateIndex(s))]->isChecked()) mask |= stateBit(s);
    }
    syncing_ = true;
    model_.setFilter(mask);
    tree_->expand(model_.boardIndex());
    syncing_ = false;
    updateChips();
}

void ObjectsPanel::updateChips() {
    const ObjectRowsPtr rows = model_.rows();
    for (OverlayState s : {OverlayState::Sample, OverlayState::Found, OverlayState::Inferred, OverlayState::Rejected}) {
        const int n = rows ? rows->stateCounts[static_cast<std::size_t>(stateIndex(s))] : 0;
        QToolButton* b = chips_[static_cast<std::size_t>(stateIndex(s))];
        b->setText(QStringLiteral("%1 %2").arg(uiText(stateKey(s))).arg(n));
        b->setEnabled(rows != nullptr);
    }
    const bool none = !rows || rows->rows.empty();
    empty_->setVisible(none);
    tree_->setVisible(!none);
}

void ObjectsPanel::retranslate() {
    empty_->setText(uiText("ui.objects.empty"));
    selectionBox_->setTitle(uiText("ui.selection.title"));
    centerLabel_->setText(uiText("ui.selection.center"));
    sizeLabel_->setText(uiText("ui.selection.size"));
    angleLabel_->setText(uiText("ui.selection.angle"));
    boxesLabel_->setText(uiText("ui.selection.boxes"));
    lockRatio_->setText(uiText("ui.selection.lockRatio"));
    applyToAll_->setText(uiText("ui.selection.applyToAll"));
    applyToAll_->setToolTip(uiText("ui.selection.applyToAll.tip"));
    matchLabel_->setText(uiText("ui.selection.match.title"));
    matchLabel_->setToolTip(uiText("ui.selection.match.tip"));
    matchModeLabel_->setText(uiText("ui.selection.match.mode"));
    matchMode_->setItemText(0, uiText("ui.selection.match.mode.intensity"));
    matchMode_->setItemText(1, uiText("ui.selection.match.mode.edges"));
    matchMode_->setToolTip(uiText("ui.selection.match.mode.tip"));
    matchGrowLabel_->setText(uiText("ui.selection.match.grow"));
    matchGrow_->setToolTip(uiText("ui.selection.match.grow.tip"));
    matchClear_->setText(uiText("ui.selection.match.clear"));
    const QString px = QStringLiteral(" px");
    for (QDoubleSpinBox* s : {centerX_, centerY_, sizeW_, sizeH_}) s->setSuffix(px);
    updateSelectionTexts();
    model_.retranslate();
    if (model_.rows()) tree_->expand(model_.boardIndex());
    resizeColumns();
    updateChips();
}

}  // namespace aoi::ui
