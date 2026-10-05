#include "presentation/objects/ObjectsPanel.h"

#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QLabel>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <algorithm>

#include "presentation/i18n/Translations.h"

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

ObjectsPanel::ObjectsPanel(QWidget* parent) : QWidget(parent) {
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(8, 8, 8, 8);
    column->setSpacing(6);

    // 狀態篩選晶片：文字（狀態名稱＋數量）而不是只靠顏色。
    auto* chipRow = new QHBoxLayout;
    chipRow->setSpacing(4);
    for (OverlayState s : {OverlayState::Sample, OverlayState::Found, OverlayState::Inferred, OverlayState::Rejected}) {
        auto* b = new QToolButton(this);
        b->setCheckable(true);
        b->setChecked(true);
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        b->setAutoRaise(false);
        b->setObjectName(QStringLiteral("stateChip%1").arg(stateIndex(s)));
        connect(b, &QToolButton::toggled, this, [this] { onChipToggled(); });
        chips_[static_cast<std::size_t>(stateIndex(s))] = b;
        chipRow->addWidget(b);
    }
    chipRow->addStretch(1);
    column->addLayout(chipRow);

    tree_ = new QTreeView(this);
    tree_->setModel(&model_);
    tree_->setUniformRowHeights(true);  // 5 000 列時 View 不必逐列量高度
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree_->setAlternatingRowColors(false);
    tree_->setRootIsDecorated(true);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(ObjectsModel::NameColumn, QHeaderView::Stretch);
    // 狀態與分數欄用固定寬度（由字型量出來，resizeColumns），不用 ResizeToContents：後者每次資料變動都要掃描上千列的文字寬度，
    // 5 000 列時光這一項就花掉整個重繪預算（實測，見 docs/steps/S04-presenter/perf.md）。
    tree_->header()->setSectionResizeMode(ObjectsModel::StateColumn, QHeaderView::Fixed);
    tree_->header()->setSectionResizeMode(ObjectsModel::ScoreColumn, QHeaderView::Fixed);
    column->addWidget(tree_, 1);

    empty_ = new QLabel(this);
    empty_->setWordWrap(true);
    empty_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    empty_->setForegroundRole(QPalette::PlaceholderText);
    empty_->setMargin(4);
    column->addWidget(empty_);

    connect(tree_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        if (syncing_) return;
        emit selectionChangedByUser(selectedIds());
    });
    retranslate();
    updateChips();
}

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
    model_.retranslate();
    if (model_.rows()) tree_->expand(model_.boardIndex());
    resizeColumns();
    updateChips();
}

}  // namespace aoi::ui
