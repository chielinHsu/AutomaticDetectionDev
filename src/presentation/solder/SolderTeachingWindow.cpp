#include "presentation/solder/SolderTeachingWindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QHeaderView>
#include <QFont>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTableWidget>
#include <algorithm>
#include "domain/box_expander.h"
#include "presentation/canvas/ImageCanvas.h"
#include "presentation/theme/ThemeManager.h"
#include "ui_SolderTeachingWindow.h"

namespace aoi::ui {
SolderTeachingWindow::SolderTeachingWindow(IImageSource& images, IRecipeStore& recipes,
    std::shared_ptr<const ISolderRegionDetector> detector, IExecutor& executor, ThemeManager& theme, QWidget* parent)
    : QMainWindow(parent), ui_(std::make_unique<Ui::SolderTeachingWindow>()), images_(images), recipes_(recipes),
      detector_(std::move(detector)), poster_(std::make_shared<UiPoster>(this)), runner_(executor), saver_(recipes, executor) {
    ui_->setupUi(this);
    setFont(QFont(QStringLiteral("Microsoft JhengHei UI"),10));
    ui_->mainLayout->setStretch(1,1);
    ui_->cadGroup->hide();
    ui_->detectionGroup->hide();  // 舊亮塊入口只留相容 API，主要流程改為樣本搜尋。
    buildConnections(theme);
    refresh();
    showMessage(qtTrId("socket.instructions"));
}

SolderTeachingWindow::~SolderTeachingWindow() {
    poster_->detach();
    runner_.cancel();
    ui_->canvas->setController(nullptr);
    ui_->canvas->setSelectionController(nullptr);
    ui_->canvas->setKeyboardController(nullptr);
}

ImageCanvas* SolderTeachingWindow::canvas() const { return ui_->canvas; }

void SolderTeachingWindow::buildConnections(ThemeManager& theme) {
    auto* c = ui_->canvas;
    c->setController(this); c->setSelectionController(this); c->setKeyboardController(this);
    c->applyTheme(theme.colors());
    connect(&theme, &ThemeManager::changed, this, [this, &theme] { ui_->canvas->applyTheme(theme.colors()); });
    const struct { QAction* action; const char* key; QKeySequence shortcut; } actions[] = {
        {ui_->actionOpenImage, "socket.openImage", QKeySequence::Open},
        {ui_->actionOpenRecipe, "socket.openRecipe", QKeySequence(QStringLiteral("Ctrl+Shift+O"))},
        {ui_->actionSave, "socket.save", QKeySequence::Save},
        {ui_->actionRegion, "socket.teach", QKeySequence(Qt::Key_R)},
        {ui_->actionSearch, "socket.searchAll", QKeySequence(Qt::Key_F5)},
        {ui_->actionSelect, "socket.select", QKeySequence(Qt::Key_V)},
        {ui_->actionManual, "socket.manual", QKeySequence(Qt::Key_S)},
        {ui_->actionUndo, "action.undo", QKeySequence::Undo},
        {ui_->actionRedo, "action.redo", QKeySequence::Redo},
        {ui_->actionDelete, "socket.delete", QKeySequence::Delete},
        {ui_->actionFit, "action.fit", QKeySequence(Qt::Key_F)},
        {ui_->actionCancel, "socket.cancel", QKeySequence(Qt::Key_Escape)}
    };
    for (const auto& a : actions) { a.action->setText(qtTrId(a.key)); a.action->setShortcut(a.shortcut); }
    connect(ui_->actionOpenImage, &QAction::triggered, this, &SolderTeachingWindow::promptImage);
    connect(ui_->actionOpenRecipe, &QAction::triggered, this, &SolderTeachingWindow::promptRecipe);
    connect(ui_->actionSave, &QAction::triggered, this, &SolderTeachingWindow::promptSave);
    connect(ui_->actionSearch, &QAction::triggered, this, &SolderTeachingWindow::searchWholeImage);
    connect(ui_->searchButton, &QPushButton::clicked, this, &SolderTeachingWindow::searchWholeImage);
    connect(ui_->similarity, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (!busy() && session_.setMatchScore(value/100.0)) { refresh(); showMessage(qtTrId("socket.searchAgain")); }
    });
    connect(ui_->actionRegion, &QAction::triggered, this, [c] { c->setInteraction(ToolId::Region); });
    connect(ui_->actionSelect, &QAction::triggered, this, [c] { c->setInteraction(ToolId::Select); });
    connect(ui_->actionManual, &QAction::triggered, this, [c] { c->setInteraction(ToolId::Sample); });
    connect(ui_->actionFit, &QAction::triggered, c, &ImageCanvas::fit);
    connect(ui_->actionCancel, &QAction::triggered, this, &SolderTeachingWindow::cancelDetection);
    connect(ui_->actionDelete, &QAction::triggered, this, &SolderTeachingWindow::deleteSelected);
    connect(ui_->actionUndo, &QAction::triggered, this, [this] { if (!busy() && session_.undo()) refresh(); });
    connect(ui_->actionRedo, &QAction::triggered, this, [this] { if (!busy() && session_.redo()) refresh(); });
    connect(ui_->repeatButton, &QPushButton::clicked, this, [this] { if (lastRegion_) detectRegion(*lastRegion_); });
    connect(ui_->applyButton, &QPushButton::clicked, this, &SolderTeachingWindow::applyGeometry);
    connect(ui_->table, &QTableWidget::itemSelectionChanged, this, [this] {
        if (busy()) return;
        std::vector<int> ids;
        for (const auto& index : ui_->table->selectionModel()->selectedRows())
            ids.push_back(ui_->table->item(index.row(), 0)->data(Qt::UserRole).toInt());
        select(ids);
    });
    ui_->cadGroup->setTitle(qtTrId("socket.cadSettings"));
    ui_->cadAuto->setText(qtTrId("socket.cadAuto"));
    ui_->cadSnap->setText(qtTrId("socket.cadSnap"));
    ui_->cadWholeButton->setText(qtTrId("socket.cadWhole"));
    connect(ui_->cadWholeButton,&QPushButton::clicked,this,&SolderTeachingWindow::searchWholeImage);
    connect(ui_->cadAuto,&QCheckBox::toggled,this,[this] { refresh(); });
    ui_->sampleGroup->setTitle(qtTrId("socket.sampleSettings"));
    ui_->similarityLabel->setText(qtTrId("socket.similarity"));
    ui_->searchButton->setText(qtTrId("socket.searchAll"));
    ui_->detectionGroup->setTitle(qtTrId("socket.detection"));
    ui_->geometryGroup->setTitle(qtTrId("socket.geometry"));
    ui_->thresholdLabel->setText(qtTrId("socket.threshold"));
    ui_->threshold->setSpecialValueText(qtTrId("socket.auto"));
    ui_->minAreaLabel->setText(qtTrId("socket.minArea"));
    ui_->maxAreaLabel->setText(qtTrId("socket.maxArea"));
    ui_->paddingLabel->setText(qtTrId("socket.padding"));
    ui_->darkForeground->setText(qtTrId("socket.dark"));
    ui_->repeatButton->setText(qtTrId("socket.repeat"));
    ui_->applyButton->setText(qtTrId("socket.apply"));
    ui_->hint->setText(qtTrId("socket.instructions"));
    ui_->hint->setWordWrap(true);
    ui_->table->setHorizontalHeaderLabels({qtTrId("socket.kind"),qtTrId("socket.box"),qtTrId("socket.similarity"),
        QStringLiteral("X"),QStringLiteral("Y"),QStringLiteral("W"),QStringLiteral("H")});
    // 持續 ResizeToContents 會在逐格替換時反覆掃描整張表，拖移放開可阻塞數秒。
    // 保留手動調寬，資料批次更新後只量一次內容。
    ui_->table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui_->table->horizontalHeader()->setStretchLastSection(true);
    ui_->splitter->setStretchFactor(0, 1);
}

void SolderTeachingWindow::showMessage(const QString& message) { ui_->message->setText(message); }

void SolderTeachingWindow::refresh() {
    auto snapshot = std::make_shared<ComponentsSnapshot>();
    const Board& board = session_.recipe().board;
    QSignalBlocker blocker(ui_->table);
    ui_->table->setRowCount(static_cast<int>(board.components.size()));
    int row = 0;
    for (const auto& component : board.components) {
        const auto r = session_.box(component.id);
        if (!r) continue;
        ComponentView view;
        view.id = component.id; view.text = QString::fromStdString(component.name);
        view.state = component.state == ObjectState::Sample ? OverlayState::Sample : OverlayState::Found;
        view.outline = QRectF(r->x, r->y, r->w, r->h);
        const auto type=std::find_if(board.types.begin(),board.types.end(),[&](const auto& t) { return t.id==component.typeId; });
        if (type!=board.types.end()) for (const auto& box : expandBoxes(*type,component.pose,board.umPerPixel)) {
            const Rect b=boundingRect(box.rect);
            view.boxes.push_back({QRectF(b.x,b.y,b.w,b.h),box.kind==BoxGroupKind::Window?BoxKind::Window:BoxKind::Body});
        }
        else view.boxes.push_back({view.outline,BoxKind::Body});
        snapshot->components.push_back(std::move(view));
        QString kind = qtTrId(component.state == ObjectState::Sample ? "socket.kindSample" :
            (component.origin == ObjectOrigin::Manual ? "socket.kindManual" : "socket.kindResult"));
        if (type!=board.types.end() && type->libId.starts_with("socket-cad."))
            kind=type->libId.ends_with("left")?QStringLiteral("CAD-L"):QStringLiteral("CAD-R");
        const QStringList values{kind,QString::fromStdString(component.name),
                                 component.score ? QString::number(*component.score*100,'f',1)+QLatin1String("%") : QStringLiteral("—"), QString::number(r->x, 'f', 1),
                                 QString::number(r->y, 'f', 1), QString::number(r->w, 'f', 1), QString::number(r->h, 'f', 1)};
        for (int col = 0; col < values.size(); ++col) {
            auto* item = ui_->table->item(row, col);
            if (!item) {
                item = new QTableWidgetItem;
                ui_->table->setItem(row, col, item);
            }
            if (item->data(Qt::UserRole).toInt() != component.id) item->setData(Qt::UserRole, component.id);
            if (item->text() != values[col]) item->setText(values[col]);
        }
        ++row;
    }
    ui_->table->resizeColumnsToContents();
    const auto samples = session_.samples();
    ui_->sampleSummary->setText(qtTrId("socket.sampleCount").arg(samples.size()));
    ui_->sizeSummary->setText(samples.empty() ? qtTrId("socket.needSamples") :
        qtTrId("socket.fixedSize").arg(samples.front().w,0,'f',1).arg(samples.front().h,0,'f',1));
    { QSignalBlocker scoreBlocker(ui_->similarity); ui_->similarity->setValue(session_.recipe().params.minScore*100); }
    const auto cadSize=ui_->cadAuto->isChecked()?session_.cadFrameSize():std::optional<Size2D>{{ui_->cadWidth->value(),ui_->cadHeight->value()}};
    ui_->cadInfo->setText(cadSize?qtTrId("socket.cadSpec").arg(cadSize->w,0,'f',1).arg(cadSize->h,0,'f',1):qtTrId("socket.cadInitial"));
    if (cadSize && ui_->cadAuto->isChecked()) { ui_->cadWidth->setValue(cadSize->w); ui_->cadHeight->setValue(cadSize->h); }
    ui_->canvas->showComponents(std::move(snapshot));
    std::erase_if(selected_, [this](int id) { return !session_.box(id); });
    refreshSelection();
    setBusyUi();
    setWindowTitle(qtTrId(cadMode_?"socket.cadTitle":"socket.title") + (session_.dirty() ? QStringLiteral(" *") : QString()) +
                   qtTrId("socket.titleCounts").arg(samples.size()).arg(board.components.size()-samples.size()));
}

void SolderTeachingWindow::refreshSelection() {
    QSignalBlocker blocker(ui_->table);
    ui_->table->clearSelection();
    for (int row = 0; row < ui_->table->rowCount(); ++row) {
        if (std::find(selected_.begin(), selected_.end(), ui_->table->item(row, 0)->data(Qt::UserRole).toInt()) != selected_.end())
            for (int col = 0; col < ui_->table->columnCount(); ++col) ui_->table->item(row, col)->setSelected(true);
    }
    ui_->canvas->setSelection(selected_);
    const auto r = selected_.size() == 1 ? session_.box(selected_.front()) : std::nullopt;
    ui_->geometryGroup->setEnabled(r.has_value() && !busy());
    if (r) { ui_->x->setValue(r->x); ui_->y->setValue(r->y); ui_->w->setValue(r->w); ui_->h->setValue(r->h); }
    ui_->actionDelete->setEnabled(!selected_.empty() && !busy());
}

void SolderTeachingWindow::setBusyUi() {
    const bool loaded = image_.valid(), editable = loaded && !busy();
    ui_->canvas->setToolsEnabled(editable); ui_->canvas->setBusyCursor(busy());
    for (auto* action : {ui_->actionOpenImage, ui_->actionOpenRecipe}) action->setEnabled(!busy());
    for (auto* action : {ui_->actionSelect, ui_->actionManual, ui_->actionSave}) action->setEnabled(editable);
    ui_->actionRegion->setEnabled(editable && (!cadMode_ || cadGenerator_));
    const bool searchable = editable && (cadMode_ ? cadGenerator_!=nullptr : sampleSearcher_ && !session_.samples().empty());
    ui_->actionSearch->setEnabled(searchable);
    ui_->searchButton->setEnabled(searchable);
    ui_->sampleGroup->setEnabled(editable);
    ui_->cadGroup->setEnabled(editable);
    ui_->cadWholeButton->setEnabled(searchable);
    ui_->cadWidth->setEnabled(editable && !ui_->cadAuto->isChecked());
    ui_->cadHeight->setEnabled(editable && !ui_->cadAuto->isChecked());
    ui_->actionFit->setEnabled(loaded);
    ui_->actionUndo->setEnabled(!busy() && session_.canUndo());
    ui_->actionRedo->setEnabled(!busy() && session_.canRedo());
    ui_->actionCancel->setEnabled(detecting_);
    ui_->detectionGroup->setEnabled(editable && detector_ != nullptr);
    ui_->repeatButton->setEnabled(editable && detector_ && lastRegion_.has_value());
    ui_->table->setEnabled(!busy()); refreshSelection();
}

void SolderTeachingWindow::select(const std::vector<int>& ids) {
    if (busy()) return;
    selected_.clear();
    for (int id : ids) if (session_.box(id)) selected_.push_back(id);
    refreshSelection();
}
void SolderTeachingWindow::deleteSelected() { if (!busy() && session_.remove(selected_)) { selected_.clear(); refresh(); } }
void SolderTeachingWindow::selectAll() {
    std::vector<int> ids; for (const auto& c : session_.recipe().board.components) ids.push_back(c.id); select(ids);
}
void SolderTeachingWindow::selectSameType() { refreshSelection(); }  // 每個框有自己的類型。
void SolderTeachingWindow::onComponentsMoved(const std::vector<int>& ids, const QPointF& delta) {
    if (!busy()) { if (!session_.move(ids, {delta.x(), delta.y()})) showMessage(qtTrId("socket.invalidBox")); refresh(); }
}
void SolderTeachingWindow::onComponentResized(int id, const QRectF& rect) {
    if (!busy()) {
        if (!session_.resize(id, {rect.x(), rect.y(), rect.width(), rect.height()})) showMessage(qtTrId("socket.invalidBox"));
        refresh();
    }
}
void SolderTeachingWindow::applyGeometry() {
    if (selected_.size() == 1) onComponentResized(selected_.front(), QRectF(ui_->x->value(),ui_->y->value(),ui_->w->value(),ui_->h->value()));
}
void SolderTeachingWindow::onOpenTypeEditor(int, const QPointF&) { ui_->x->setFocus(); }
void SolderTeachingWindow::onAddRequested(const QPointF&) { showMessage(qtTrId("socket.useManual")); }
void SolderTeachingWindow::onRegionAborted() { ui_->canvas->showRegionDraft(std::nullopt, DraftMode::Region); }
void SolderTeachingWindow::onRegionDrawn(const QRectF& rect, DraftMode mode) {
    if (busy()) return;
    const Rect region{rect.x(),rect.y(),rect.width(),rect.height()};
    if (mode == DraftMode::Sample) {
        if (!session_.addBox(region)) showMessage(qtTrId("socket.invalidBox"));
        ui_->canvas->showRegionDraft(std::nullopt, mode); ui_->canvas->setInteraction(ToolId::Select); refresh();
    } else if (cadMode_) registerCadRegion(region);
    else {
        if (!session_.addSample(region)) showMessage(qtTrId("socket.sampleInvalid"));
        else { selected_ = {session_.recipe().board.components.back().id}; showMessage(qtTrId("socket.sampleAdded").arg(session_.samples().size())); }
        ui_->canvas->showRegionDraft(std::nullopt,mode);
        refresh();
    }
}
bool SolderTeachingWindow::onKeyPressed(int key, Qt::KeyboardModifiers modifiers) {
    if (key == Qt::Key_Escape) { cancelDetection(); return true; }
    if (busy()) return false;
    if (key == Qt::Key_F5) { searchWholeImage(); return true; }
    if (key == Qt::Key_Delete) { deleteSelected(); return true; }
    if (key == Qt::Key_A && modifiers == Qt::ControlModifier) { selectAll(); return true; }
    Point2D delta;
    if (key == Qt::Key_Left) delta.x=-1; else if (key == Qt::Key_Right) delta.x=1;
    else if (key == Qt::Key_Up) delta.y=-1; else if (key == Qt::Key_Down) delta.y=1; else return false;
    if (session_.move(selected_,delta)) refresh();
    return true;
}

bool SolderTeachingWindow::confirmDiscard() {
    if (!session_.dirty()) return true;
    return QMessageBox::question(this, qtTrId("socket.title"), qtTrId("socket.discard"),
        QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Discard;
}
void SolderTeachingWindow::closeEvent(QCloseEvent* event) {
    if (saving_) { showMessage(qtTrId("socket.saving")); event->ignore(); return; }
    cancelDetection();
    if (confirmDiscard()) event->accept(); else event->ignore();
}
}  // namespace aoi::ui
