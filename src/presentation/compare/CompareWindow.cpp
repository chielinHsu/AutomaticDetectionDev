#include "presentation/compare/CompareWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QGraphicsScene>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTableWidget>
#include <algorithm>
#include <cmath>
#include <functional>

#include "presentation/canvas/ImageCanvas.h"
#include "presentation/compare/CompareOverlayItem.h"
#include "presentation/i18n/Translations.h"
#include "ui_CompareWindow.h"

namespace aoi::ui {

namespace {

void fillCombo(QComboBox* combo, const QList<BackendChoice>& choices, int current) {
    const QSignalBlocker blocker(combo);
    combo->clear();
    for (const BackendChoice& c : choices) {
        combo->addItem(c.available ? c.name : c.name + QStringLiteral(" ✕"), c.id);
        if (!c.note.isEmpty()) combo->setItemData(combo->count() - 1, c.note, Qt::ToolTipRole);
    }
    // 不可用的項目停用（主視窗的 back end 選單同一種做法）。
    if (auto* model = qobject_cast<QStandardItemModel*>(combo->model())) {
        for (int i = 0; i < choices.size(); ++i) {
            if (QStandardItem* item = model->item(i)) item->setEnabled(choices[i].available);
        }
    }
    combo->setCurrentIndex(current);
}

// 填一張表：停止重繪、欄寬固定，填完再依內容算一次欄寬，stretchColumn 佔剩下的寬度。
void fillTable(QTableWidget* table, int rows, int stretchColumn, const std::function<QList<QTableWidgetItem*>(int)>& rowAt) {
    QHeaderView* header = table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    table->setUpdatesEnabled(false);
    table->clearContents();
    table->setRowCount(rows);
    for (int r = 0; r < rows; ++r) {
        const QList<QTableWidgetItem*> items = rowAt(r);
        for (int c = 0; c < items.size(); ++c) table->setItem(r, c, items[c]);
    }
    table->resizeColumnsToContents();
    header->setSectionResizeMode(stretchColumn, QHeaderView::Stretch);
    table->setUpdatesEnabled(true);
}

QTableWidgetItem* cell(const QString& text, bool number = false) {
    auto* item = new QTableWidgetItem(text);
    item->setToolTip(text);
    if (number) item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

}  // namespace

CompareWindow::CompareWindow(const ThemeTokens& tokens, QWidget* parent)
    : QWidget(parent, Qt::Window), ui_(std::make_unique<Ui::CompareWindow>()) {
    ui_->setupUi(this);
    setObjectName(QStringLiteral("compareWindow"));
    canvas_ = ui_->compareCanvas;
    canvas_->setToolsEnabled(false);  // 只看不改：縮放、平移照常
    overlay_ = new CompareOverlayItem(tokens);
    overlay_->setZValue(3);
    canvas_->canvasScene()->addItem(overlay_);  // 場景擁有它
    // 摘要表：整張表都看得到（不捲動）；項目那一欄佔剩下的寬度，太長的文字省略（滑鼠移上去看完整的）。
    // 差異清單：數字欄依內容，原因那一欄佔剩下的寬度；一列一行。
    // 欄寬在填完之後算一次（fillTable）：不用 ResizeToContents——它在每一個 setItem 都重新量整欄，S09 實測 70 列就要 660 ms。
    for (QTableWidget* t : {ui_->summaryTable, ui_->differencesTable}) {
        t->verticalHeader()->setDefaultSectionSize(fontMetrics().height() + 6);
        t->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        t->setWordWrap(false);
        t->horizontalHeader()->setStretchLastSection(false);
        t->horizontalHeader()->setResizeContentsPrecision(64);  // 欄寬只看前 64 列（1 000 列全量要幾十 ms）
    }
    ui_->summaryTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui_->summaryTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    ui_->summaryTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui_->summaryTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui_->differencesTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    resize(tokens.sizes.windowMin.expandedTo(QSize(1280, 800)));

    connect(ui_->runButton, &QPushButton::clicked, this, [this] {
        if (controller_) controller_->runCompare(ui_->backendA->currentData().toString(), ui_->backendB->currentData().toString());
    });
    connect(ui_->cancelButton, &QPushButton::clicked, this, [this] {
        if (controller_) controller_->cancelCompare();
    });
    connect(ui_->thresholdA, &QDoubleSpinBox::valueChanged, this, [this] { emitThresholds(); });
    connect(ui_->thresholdB, &QDoubleSpinBox::valueChanged, this, [this] { emitThresholds(); });
    const struct {
        QCheckBox* box;
        aoi::CompareKind kind;
    } legend[] = {{ui_->showBoth, aoi::CompareKind::Both}, {ui_->showOnlyA, aoi::CompareKind::OnlyA}, {ui_->showOnlyB, aoi::CompareKind::OnlyB}};
    for (const auto& l : legend) {
        const aoi::CompareKind kind = l.kind;
        connect(l.box, &QCheckBox::toggled, this, [this, kind](bool on) { overlay_->setKindVisible(kind, on); });
    }
    connect(ui_->differencesTable, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) { focusDifference(row); });
    connect(canvas_, &ImageCanvas::framePainted, this, [this](double) {
        if (!awaitingPaint_) return;
        awaitingPaint_ = false;
        emit resultPainted(static_cast<double>(resultClock_.nsecsElapsed()) / 1e6);
    });
    retranslate();
}

CompareWindow::~CompareWindow() = default;

QComboBox* CompareWindow::backendA() const { return ui_->backendA; }
QComboBox* CompareWindow::backendB() const { return ui_->backendB; }
QPushButton* CompareWindow::runButton() const { return ui_->runButton; }
QPushButton* CompareWindow::cancelButton() const { return ui_->cancelButton; }
QDoubleSpinBox* CompareWindow::thresholdA() const { return ui_->thresholdA; }
QDoubleSpinBox* CompareWindow::thresholdB() const { return ui_->thresholdB; }
QLabel* CompareWindow::statusLabel() const { return ui_->statusLabel; }
QTableWidget* CompareWindow::summaryTable() const { return ui_->summaryTable; }
QTableWidget* CompareWindow::differencesTable() const { return ui_->differencesTable; }

QCheckBox* CompareWindow::legendCheck(aoi::CompareKind kind) const {
    switch (kind) {
        case aoi::CompareKind::Both: return ui_->showBoth;
        case aoi::CompareKind::OnlyA: return ui_->showOnlyA;
        case aoi::CompareKind::OnlyB: return ui_->showOnlyB;
    }
    return nullptr;
}

void CompareWindow::applyTheme(const ThemeColors& colors) {
    canvas_->applyTheme(colors);
    overlay_->setAccent(colors.surface.accent);
}

void CompareWindow::retranslate() {
    if (result_.title.isEmpty()) setWindowTitle(uiText("ui.compare.window"));
    ui_->backendsBox->setTitle(uiText("ui.backend.label"));
    ui_->labelA->setText(uiText("ui.compare.sideA"));
    ui_->labelB->setText(uiText("ui.compare.sideB"));
    ui_->runButton->setText(uiText("ui.compare.run"));
    ui_->cancelButton->setText(uiText("action.cancel"));
    ui_->thresholdsBox->setTitle(uiText("ui.compare.thresholds"));
    ui_->thresholdLabelA->setText(uiText("ui.compare.sideA"));
    ui_->thresholdLabelB->setText(uiText("ui.compare.sideB"));
    ui_->thresholdNote->setText(uiText("ui.compare.thresholds.note"));
    ui_->legendBox->setTitle(uiText("ui.compare.show"));
    ui_->summaryTable->setHorizontalHeaderLabels(
        {uiText("ui.compare.col.item"), uiText("ui.compare.sideA"), uiText("ui.compare.sideB"), uiText("ui.compare.col.diff")});
    ui_->differencesTable->setHorizontalHeaderLabels({uiText("ui.compare.col.kind"), uiText("ui.compare.col.where"), uiText("ui.compare.sideA"),
                                                      uiText("ui.compare.sideB"), uiText("ui.compare.col.reason")});
    if (result_.legend.size() != 3) {
        ui_->showBoth->setText(uiText("ui.compare.kind.both", {{"n", QStringLiteral("0")}}));
        ui_->showOnlyA->setText(uiText("ui.compare.kind.onlyA", {{"n", QStringLiteral("0")}}));
        ui_->showOnlyB->setText(uiText("ui.compare.kind.onlyB", {{"n", QStringLiteral("0")}}));
        ui_->differencesLabel->setText(uiText("ui.compare.differences", {{"n", QStringLiteral("0")}}));
    }
    // 結果的文字（標題、圖例的數量、表格）由 Presenter 在語言切換後重新交進來（ComparePresenter::refreshTexts）。
}

void CompareWindow::setChoices(const QList<BackendChoice>& choices, int a, int b) {
    fillCombo(ui_->backendA, choices, a);
    fillCombo(ui_->backendB, choices, b);
}

void CompareWindow::showRequest(const aoi::ImageView& image, double thresholdA, double thresholdB) {
    if (image.data != imageData_) {
        imageData_ = image.data;
        canvas_->showImage(image);
        overlay_->setBounds(canvas_->imageRect());
        overlay_->setOverlay(nullptr);
    }
    syncing_ = true;
    ui_->thresholdA->setValue(thresholdA);
    ui_->thresholdB->setValue(thresholdB);
    syncing_ = false;
    if (isMinimized()) showNormal();
    show();
    raise();
    activateWindow();
}

void CompareWindow::showResult(const CompareResultView& result) {
    result_ = result;
    setWindowTitle(result.title);
    ui_->statusLabel->setText(result.status);
    if (result.legend.size() == 3) {
        ui_->showBoth->setText(result.legend[0]);
        ui_->showOnlyA->setText(result.legend[1]);
        ui_->showOnlyB->setText(result.legend[2]);
    }

    fillTable(ui_->summaryTable, static_cast<int>(result.table.size()), 0, [&](int r) {
        const CompareTableRow& row = result.table[r];
        return QList<QTableWidgetItem*>{cell(row.label), cell(row.a, true), cell(row.b, true), cell(row.diff, true)};
    });
    ui_->differencesLabel->setText(result.differencesTitle);
    {
        const QSignalBlocker blocker(ui_->differencesTable);  // 清單換了：不是使用者選了一列
        fillTable(ui_->differencesTable, static_cast<int>(result.differences.size()), 4, [&](int r) {
            const CompareDifferenceRow& row = result.differences[r];
            return QList<QTableWidgetItem*>{cell(row.kind), cell(row.where), cell(row.scoreA, true), cell(row.scoreB, true), cell(row.reason)};
        });
    }

    const bool sameOverlay = overlay_->overlay() == result.overlay;
    const int highlight = overlay_->highlight();
    overlay_->setOverlay(result.overlay);
    if (sameOverlay) overlay_->setHighlight(highlight);  // 只換了文字（語言）：選的那一個留著
    awaitingPaint_ = true;
    resultClock_.start();
    canvas_->viewport()->update();
}

void CompareWindow::showBusy(bool busy, const QString& text) {
    busy_ = busy;
    ui_->runButton->setEnabled(!busy);
    ui_->cancelButton->setEnabled(busy);
    ui_->backendA->setEnabled(!busy);
    ui_->backendB->setEnabled(!busy);
    ui_->statusLabel->setText(text);
    setCursor(busy ? Qt::BusyCursor : Qt::ArrowCursor);
}

void CompareWindow::focusDifference(int row) {
    if (row < 0 || row >= result_.differences.size() || !result_.overlay) {
        overlay_->setHighlight(-1);
        return;
    }
    const int index = result_.differences[row].markIndex;
    if (index < 0 || index >= static_cast<int>(result_.overlay->marks.size())) return;
    overlay_->setHighlight(index);
    if (canvas_->viewScale() < 1.0) {
        canvas_->zoomAt(QPointF(canvas_->viewport()->rect().center()), 1.0 / canvas_->viewScale());
    }
    canvas_->centerOn(result_.overlay->marks[static_cast<std::size_t>(index)].center);
}

void CompareWindow::emitThresholds() {
    if (syncing_ || controller_ == nullptr) return;
    controller_->setThresholds(ui_->thresholdA->value(), ui_->thresholdB->value());
}

void CompareWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) retranslate();
    QWidget::changeEvent(event);
}

}  // namespace aoi::ui
