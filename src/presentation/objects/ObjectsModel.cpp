#include "presentation/objects/ObjectsModel.h"

#include <QFont>

#include "presentation/i18n/Translations.h"

namespace aoi::ui {

namespace {

const char* roleKey(BoxGroupKind kind) {
    switch (kind) {
        case BoxGroupKind::Body: return "role.body";
        case BoxGroupKind::Pin: return "role.pin";
        case BoxGroupKind::Window: return "role.window";
    }
    return "role.other";
}

}  // namespace

ObjectsModel::ObjectsModel(QObject* parent) : QAbstractItemModel(parent) { rebuildTexts(); }

void ObjectsModel::rebuildTexts() {
    stateNames_[static_cast<std::size_t>(stateIndex(OverlayState::Sample))] = uiText("state.sample");
    stateNames_[static_cast<std::size_t>(stateIndex(OverlayState::Found))] = uiText("state.found");
    stateNames_[static_cast<std::size_t>(stateIndex(OverlayState::Inferred))] = uiText("state.inferred");
    stateNames_[static_cast<std::size_t>(stateIndex(OverlayState::Rejected))] = uiText("state.rejected");
    manualText_ = uiText("origin.manual");
    boardTitle_ = rows_ ? rows_->boardTitle : QString();
}

std::vector<int> ObjectsModel::computeVisible(const ObjectRowsPtr& rows, StateFilter filter) {
    std::vector<int> visible;
    if (!rows) return visible;
    visible.reserve(rows->rows.size());
    for (std::size_t i = 0; i < rows->rows.size(); ++i) {
        if (filter & stateBit(rows->rows[i].state)) visible.push_back(static_cast<int>(i));
    }
    return visible;
}

// 新舊兩份資料的「形狀」一樣：同樣的列（編號與順序）、同樣的類型與群組個數。只有狀態、分數、名稱可能不同。
bool ObjectsModel::sameShape(const ObjectRowsPtr& a, const ObjectRowsPtr& b) {
    if (!a || !b || a->proposal != b->proposal || a->rows.size() != b->rows.size() || a->types.size() != b->types.size())
        return false;
    for (std::size_t i = 0; i < a->types.size(); ++i) {
        if (a->types[i].groups.size() != b->types[i].groups.size()) return false;
    }
    for (std::size_t i = 0; i < a->rows.size(); ++i) {
        if (a->rows[i].id != b->rows[i].id || a->rows[i].typeIndex != b->rows[i].typeIndex) return false;
    }
    return true;
}

void ObjectsModel::setRows(ObjectRowsPtr rows) {
    std::vector<int> newVisible = computeVisible(rows, filter_);
    // 分數門檻拖動時，列的編號與順序都不變，只有狀態與分數變：用 dataChanged 就好，
    // 不要 reset（reset 會讓樹丟掉捲動位置、展開狀態與選取，拖一下門檻整棵樹跳回最上面）。
    if (sameShape(rows_, rows) && newVisible == visible_) {
        rows_ = std::move(rows);
        rebuildTexts();
        if (!visible_.empty()) {
            const QModelIndex board = boardIndex();
            emit dataChanged(index(0, 0, board), index(static_cast<int>(visible_.size()) - 1, ColumnCount - 1, board));
        }
        emit dataChanged(boardIndex(), boardIndex());
        return;
    }
    beginResetModel();
    rows_ = std::move(rows);
    visible_ = std::move(newVisible);
    idToVisible_.clear();
    idIndexBuilt_ = false;
    rebuildTexts();
    endResetModel();
}

void ObjectsModel::setFilter(StateFilter filter) {
    filter &= kAllStates;
    if (filter == filter_) return;
    beginResetModel();
    filter_ = filter;
    visible_ = computeVisible(rows_, filter_);
    idToVisible_.clear();
    idIndexBuilt_ = false;
    endResetModel();
}

void ObjectsModel::retranslate() {
    beginResetModel();
    rebuildTexts();
    endResetModel();
}

int ObjectsModel::groupCount(int visibleRow) const {
    const ObjectRow& row = rowAt(visibleRow);
    if (row.typeIndex < 0 || static_cast<std::size_t>(row.typeIndex) >= rows_->types.size()) return 0;
    return static_cast<int>(rows_->types[static_cast<std::size_t>(row.typeIndex)].groups.size());
}

int ObjectsModel::idAt(const QModelIndex& index) const {
    if (!index.isValid() || !rows_) return -1;
    const Kind kind = kindOf(index.internalId());
    if (kind != Kind::Component && kind != Kind::Group) return -1;
    return rowAt(visibleRowOf(index.internalId())).id;
}

QModelIndex ObjectsModel::boardIndex() const { return rows_ ? createIndex(0, 0, boardId()) : QModelIndex(); }

QModelIndex ObjectsModel::indexForId(int sceneId, int column) const {
    if (!rows_) return {};
    if (!idIndexBuilt_) {
        idToVisible_.reserve(static_cast<qsizetype>(visible_.size()));
        for (std::size_t v = 0; v < visible_.size(); ++v) idToVisible_.insert(rowAt(static_cast<int>(v)).id, static_cast<int>(v));
        idIndexBuilt_ = true;
    }
    const auto it = idToVisible_.constFind(sceneId);
    if (it == idToVisible_.constEnd()) return {};
    return createIndex(it.value(), column, componentId(it.value()));
}

QModelIndex ObjectsModel::index(int row, int column, const QModelIndex& parent) const {
    if (!rows_ || column < 0 || column >= ColumnCount || row < 0) return {};
    if (!parent.isValid()) return row == 0 ? createIndex(0, column, boardId()) : QModelIndex();
    switch (kindOf(parent.internalId())) {
        case Kind::Board:
            return row < visibleCount() ? createIndex(row, column, componentId(row)) : QModelIndex();
        case Kind::Component: {
            const int vr = visibleRowOf(parent.internalId());
            return row < groupCount(vr) ? createIndex(row, column, groupId(vr, row)) : QModelIndex();
        }
        default: return {};
    }
}

QModelIndex ObjectsModel::parent(const QModelIndex& child) const {
    if (!child.isValid() || !rows_) return {};
    switch (kindOf(child.internalId())) {
        case Kind::Component: return createIndex(0, 0, boardId());
        case Kind::Group: {
            const int vr = visibleRowOf(child.internalId());
            return createIndex(vr, 0, componentId(vr));
        }
        default: return {};
    }
}

int ObjectsModel::rowCount(const QModelIndex& parent) const {
    if (!rows_ || parent.column() > 0) return 0;
    if (!parent.isValid()) return 1;
    switch (kindOf(parent.internalId())) {
        case Kind::Board: return visibleCount();
        case Kind::Component: return groupCount(visibleRowOf(parent.internalId()));
        default: return 0;
    }
}

int ObjectsModel::columnCount(const QModelIndex&) const { return ColumnCount; }

Qt::ItemFlags ObjectsModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    const Kind kind = kindOf(index.internalId());
    if (kind == Kind::Board) return Qt::ItemIsEnabled;  // Board 列只是標題，不能選
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant ObjectsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !rows_) return {};
    const quintptr id = index.internalId();
    const Kind kind = kindOf(id);

    if (kind == Kind::Board) {
        if (index.column() != NameColumn) return {};
        if (role == Qt::DisplayRole || role == Qt::ToolTipRole) return boardTitle_;
        if (role == Qt::FontRole) {
            QFont f;
            f.setBold(true);
            return f;
        }
        return {};
    }

    const int vr = visibleRowOf(id);
    const ObjectRow& row = rowAt(vr);

    if (kind == Kind::Group) {
        const ObjectTypeRow& type = rows_->types[static_cast<std::size_t>(row.typeIndex)];
        const ObjectGroupRow& group = type.groups[static_cast<std::size_t>(groupOf(id))];
        if (role == Qt::DisplayRole) {
            if (index.column() == NameColumn) return uiText(roleKey(group.kind));
            if (index.column() == ScoreColumn) return QStringLiteral("×%1").arg(group.boxCount);
        }
        if (role == Qt::TextAlignmentRole && index.column() == ScoreColumn) return int(Qt::AlignRight | Qt::AlignVCenter);
        return {};
    }

    // 元件列
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case NameColumn: return displayName(row);
            case StateColumn: return stateNames_[static_cast<std::size_t>(stateIndex(row.state))];
            case ScoreColumn:
                if (row.manual) return manualText_;
                return row.score >= 0.0 ? QVariant(QString::number(row.score, 'f', 2)) : QVariant(QStringLiteral("—"));
        }
    } else if (role == Qt::TextAlignmentRole && index.column() == ScoreColumn) {
        return int(Qt::AlignRight | Qt::AlignVCenter);
    } else if (role == Qt::ToolTipRole && index.column() == NameColumn) {
        if (row.typeIndex >= 0 && static_cast<std::size_t>(row.typeIndex) < rows_->types.size()) {
            const QString& label = rows_->types[static_cast<std::size_t>(row.typeIndex)].label;
            if (!label.isEmpty()) return QStringLiteral("%1 · %2").arg(displayName(row), label);
        }
        return displayName(row);
    }
    return {};
}

QVariant ObjectsModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case NameColumn: return uiText("ui.objects.col.name");
        case StateColumn: return uiText("ui.objects.col.state");
        case ScoreColumn: return uiText("ui.objects.col.score");
    }
    return {};
}

}  // namespace aoi::ui
