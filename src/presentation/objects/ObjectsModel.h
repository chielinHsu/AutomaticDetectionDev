#pragma once

#include <QAbstractItemModel>
#include <QHash>
#include <QString>
#include <array>
#include <vector>

#include "presentation/objects/ObjectRows.h"

namespace aoi::ui {

// 物件樹的 model：Board › Component › 框群組（ux-spec 第 4 節）。
//
// 為什麼自己寫而不是 QStandardItemModel：5 000 個元件，每個一個 QStandardItem 要配置 5 000 個物件和它們的資料，
// 填表就花掉預算（U2：5 000 列 ≤ 50 ms）。這裡只持有 Presenter 交來的不可變快照（一個 shared_ptr），
// 每一格的文字在 data() 被問到時才算（延遲載入）：View 只會問畫面上看得到的那幾十列。
// 狀態篩選也在 model 裡：只是重建一個「看得到的列」索引，不複製資料。
//
// 索引的 internalId 編碼（不配置節點物件）：低 3 位是種類（1 ＝ Board、2 ＝ 元件、3 ＝ 框群組），
// 元件與群組再帶「看得到的第幾列」，群組另外帶群組序號。
class ObjectsModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column { NameColumn = 0, StateColumn, ScoreColumn, ColumnCount };

    explicit ObjectsModel(QObject* parent = nullptr);

    // 換掉整份資料。形狀沒變（同樣的列與順序）時只發 dataChanged，不 reset：門檻拖動時樹的捲動、展開、選取都留著。
    void setRows(ObjectRowsPtr rows);
    // 只顯示 filter 裡的狀態。
    void setFilter(StateFilter filter);
    StateFilter filter() const { return filter_; }
    ObjectRowsPtr rows() const { return rows_; }
    int visibleCount() const { return static_cast<int>(visible_.size()); }
    // 語言換了：重新取狀態名稱等文字。
    void retranslate();

    // 元件列（或框群組列，回它的元件）的畫布編號；Board 列與無效索引回 -1。
    int idAt(const QModelIndex& index) const;
    // 畫布編號 → 該元件列的索引（目前被篩掉的回無效索引）。
    QModelIndex indexForId(int sceneId, int column = NameColumn) const;
    QModelIndex boardIndex() const;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    enum class Kind { None, Board, Component, Group };
    static Kind kindOf(quintptr id) { return static_cast<Kind>(id & 7u); }
    static quintptr boardId() { return 1u; }
    static quintptr componentId(int visibleRow) { return (static_cast<quintptr>(visibleRow) << 3) | 2u; }
    static quintptr groupId(int visibleRow, int group) {
        return (static_cast<quintptr>(group + 1) << 40) | (static_cast<quintptr>(visibleRow) << 3) | 3u;
    }
    static int visibleRowOf(quintptr id) { return static_cast<int>((id >> 3) & 0xFFFFFFFFu); }
    static int groupOf(quintptr id) { return static_cast<int>(id >> 40) - 1; }

    const ObjectRow& rowAt(int visibleRow) const { return rows_->rows[static_cast<std::size_t>(visible_[static_cast<std::size_t>(visibleRow)])]; }
    int groupCount(int visibleRow) const;
    static std::vector<int> computeVisible(const ObjectRowsPtr& rows, StateFilter filter);
    static bool sameShape(const ObjectRowsPtr& a, const ObjectRowsPtr& b);
    void rebuildTexts();

    ObjectRowsPtr rows_;
    StateFilter filter_ = kAllStates;
    std::vector<int> visible_;  // 通過篩選的列，值是 rows_->rows 的索引
    mutable QHash<int, int> idToVisible_;
    mutable bool idIndexBuilt_ = false;
    std::array<QString, kOverlayStateCount> stateNames_;
    QString manualText_;
    QString boardTitle_;
};

}  // namespace aoi::ui
