// ObjectsModel（物件樹的 model）與 ObjectsPanel：結構、延遲載入、篩選、不 reset 的更新、選取同步。
#include <QAbstractItemModelTester>
#include <QLabel>
#include <QSignalSpy>
#include <QToolButton>
#include <QTreeView>
#include <QtTest>

#include "presentation/i18n/Translations.h"
#include "presentation/objects/ObjectsModel.h"
#include "presentation/objects/ObjectsPanel.h"

using namespace aoi;
using namespace aoi::ui;

namespace {

ObjectRow row(int id, const char* name, OverlayState state, double score = -1.0, bool manual = false, int type = 0) {
    ObjectRow r;
    r.id = id;
    r.name = QString::fromLatin1(name);
    r.state = state;
    r.score = score;
    r.manual = manual;
    r.typeIndex = type;
    return r;
}

// 6 個元件：2 個 IC 型（主體＋Pin＋窗）、4 個晶片型（只有主體）。
std::shared_ptr<ObjectRows> sample() {
    auto rows = std::make_shared<ObjectRows>();
    rows->boardTitle = QStringLiteral("板子 board.png");
    rows->types.push_back(ObjectTypeRow{QStringLiteral("8371"),
                                        {{BoxGroupKind::Body, 1}, {BoxGroupKind::Pin, 20}, {BoxGroupKind::Window, 20}}});
    rows->types.push_back(ObjectTypeRow{QString(), {{BoxGroupKind::Body, 1}}});
    rows->rows = {row(1, "U1", OverlayState::Sample, 1.0, false, 0),    row(2, "U2", OverlayState::Found, 0.97, false, 0),
                  row(3, "R1", OverlayState::Found, 0.91, false, 1),    row(4, "R2", OverlayState::Rejected, 0.42, false, 1),
                  row(5, "R3", OverlayState::Inferred, -1.0, false, 1), row(6, "R4", OverlayState::Found, -1.0, true, 1)};
    for (const ObjectRow& r : rows->rows) ++rows->stateCounts[static_cast<std::size_t>(stateIndex(r.state))];
    return rows;
}

QString text(const ObjectsModel& m, const QModelIndex& i) { return m.data(i).toString(); }

}  // namespace

class TstObjectsModel : public QObject {
    Q_OBJECT
private slots:
    void init() { Translations::install(QStringLiteral("zh_TW")); }

    // ---- 結構 ----
    void theTreeIsBoardThenComponentsThenBoxGroups() {
        ObjectsModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(m.rowCount(), 0);  // 還沒有資料
        m.setRows(sample());
        QCOMPARE(m.rowCount(), 1);  // 只有一個 Board 列
        const QModelIndex board = m.index(0, 0);
        QCOMPARE(text(m, board), QStringLiteral("板子 board.png"));
        QCOMPARE(m.rowCount(board), 6);
        const QModelIndex u1 = m.index(0, 0, board);
        QCOMPARE(text(m, u1), QStringLiteral("U1"));
        QCOMPARE(m.rowCount(u1), 3);  // 主體、Pin、窗
        const QModelIndex r1 = m.index(2, 0, board);
        QCOMPARE(m.rowCount(r1), 1);  // 晶片：只有主體
        QCOMPARE(m.rowCount(m.index(0, 0, u1)), 0);  // 群組列沒有子節點
    }

    void parentsAreConsistent() {
        ObjectsModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.setRows(sample());
        const QModelIndex board = m.index(0, 0);
        const QModelIndex u1 = m.index(0, 0, board);
        const QModelIndex pin = m.index(1, 0, u1);
        QVERIFY(!m.parent(board).isValid());
        QCOMPARE(m.parent(u1), board);
        QCOMPARE(m.parent(pin), u1);
        QVERIFY(m.hasChildren(board));
        QVERIFY(m.hasChildren(u1));
        QVERIFY(!m.hasChildren(pin));
    }

    void rowsShowNameStateAndScore() {
        ObjectsModel m;
        m.setRows(sample());
        const QModelIndex board = m.index(0, 0);
        auto cell = [&](int row, int col) { return text(m, m.index(row, col, board)); };
        QCOMPARE(cell(1, ObjectsModel::NameColumn), QStringLiteral("U2"));
        QCOMPARE(cell(1, ObjectsModel::StateColumn), QStringLiteral("找到"));
        QCOMPARE(cell(1, ObjectsModel::ScoreColumn), QStringLiteral("0.97"));
        QCOMPARE(cell(0, ObjectsModel::StateColumn), QStringLiteral("樣本"));
        QCOMPARE(cell(3, ObjectsModel::StateColumn), QStringLiteral("拒絕"));
        QCOMPARE(cell(4, ObjectsModel::StateColumn), QStringLiteral("推算"));
        QCOMPARE(cell(4, ObjectsModel::ScoreColumn), QStringLiteral("—"));      // 沒有分數
        QCOMPARE(cell(5, ObjectsModel::ScoreColumn), QStringLiteral("手動"));    // 手動補的標示來源，不偽造分數
    }

    void groupRowsShowTheRoleAndTheBoxCount() {
        ObjectsModel m;
        m.setRows(sample());
        const QModelIndex u1 = m.index(0, 0, m.index(0, 0));
        QCOMPARE(text(m, m.index(0, ObjectsModel::NameColumn, u1)), QStringLiteral("主體"));
        QCOMPARE(text(m, m.index(1, ObjectsModel::NameColumn, u1)), QStringLiteral("Pin 腳"));
        QCOMPARE(text(m, m.index(1, ObjectsModel::ScoreColumn, u1)), QStringLiteral("×20"));
        QCOMPARE(text(m, m.index(2, ObjectsModel::NameColumn, u1)), QStringLiteral("檢測窗"));
    }

    void tooltipsNameTheType() {
        ObjectsModel m;
        m.setRows(sample());
        const QModelIndex board = m.index(0, 0);
        QVERIFY(m.data(m.index(0, 0, board), Qt::ToolTipRole).toString().contains(QStringLiteral("8371")));
        QCOMPARE(m.data(m.index(2, 0, board), Qt::ToolTipRole).toString(), QStringLiteral("R1"));  // 暫時類型沒有料號
    }

    void theBoardRowIsAHeadingNotSelectable() {
        ObjectsModel m;
        m.setRows(sample());
        QVERIFY(!(m.flags(m.index(0, 0)) & Qt::ItemIsSelectable));
        QVERIFY(m.flags(m.index(0, 0, m.index(0, 0))) & Qt::ItemIsSelectable);
    }

    void headersAreTranslated() {
        ObjectsModel m;
        QCOMPARE(m.headerData(0, Qt::Horizontal).toString(), QStringLiteral("名稱"));
        QCOMPARE(m.headerData(1, Qt::Horizontal).toString(), QStringLiteral("狀態"));
        QCOMPARE(m.headerData(2, Qt::Horizontal).toString(), QStringLiteral("分數"));
        Translations::install(QStringLiteral("en"));
        QCOMPARE(m.headerData(0, Qt::Horizontal).toString(), QStringLiteral("Name"));
    }

    // ---- 編號 ----
    void idsMapToRowsAndBack() {
        ObjectsModel m;
        m.setRows(sample());
        const QModelIndex idx = m.indexForId(3);
        QVERIFY(idx.isValid());
        QCOMPARE(text(m, idx), QStringLiteral("R1"));
        QCOMPARE(m.idAt(idx), 3);
        QCOMPARE(m.idAt(m.index(0, ObjectsModel::ScoreColumn, m.parent(idx))), 1);  // 任一欄都回該列的編號
        QVERIFY(!m.indexForId(99).isValid());
        QCOMPARE(m.idAt(m.index(0, 0)), -1);   // Board 列不是元件
        QCOMPARE(m.idAt(QModelIndex()), -1);
        const QModelIndex group = m.index(1, 0, m.indexForId(1));
        QCOMPARE(m.idAt(group), 1);            // 群組列算它的元件
    }

    // ---- 篩選 ----
    void theFilterHidesStatesWithoutCopyingData() {
        ObjectsModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        const auto rows = sample();
        m.setRows(rows);
        QCOMPARE(m.visibleCount(), 6);
        m.setFilter(stateBit(OverlayState::Found));
        QCOMPARE(m.visibleCount(), 3);
        QCOMPARE(m.rowCount(m.index(0, 0)), 3);
        QVERIFY(!m.indexForId(4).isValid());  // Rejected 被篩掉
        QVERIFY(m.indexForId(2).isValid());
        QVERIFY(m.rows() == rows);            // 資料還是同一份快照
        m.setFilter(kAllStates);
        QCOMPARE(m.visibleCount(), 6);
        m.setFilter(0);
        QCOMPARE(m.visibleCount(), 0);
    }

    void aFilterSurvivesNewData() {
        ObjectsModel m;
        m.setFilter(stateBit(OverlayState::Rejected));
        m.setRows(sample());
        QCOMPARE(m.visibleCount(), 1);
        QCOMPARE(m.idAt(m.index(0, 0, m.index(0, 0))), 4);
    }

    // ---- 更新：形狀沒變就不 reset ----
    void changingOnlyStatesAndScoresEmitsDataChangedNotReset() {
        ObjectsModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.setRows(sample());
        QSignalSpy resets(&m, &QAbstractItemModel::modelReset);
        QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
        auto next = std::make_shared<ObjectRows>(*sample());
        next->rows[3].state = OverlayState::Found;  // 門檻降低：2 號之外 4 號也過了
        next->rows[3].score = 0.42;
        m.setRows(next);
        QCOMPARE(resets.count(), 0);
        QVERIFY(changed.count() >= 1);
        QCOMPARE(text(m, m.index(3, ObjectsModel::StateColumn, m.index(0, 0))), QStringLiteral("找到"));
    }

    void changingTheRowsOrTheirOrderResets() {
        ObjectsModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.setRows(sample());
        QSignalSpy resets(&m, &QAbstractItemModel::modelReset);
        auto fewer = std::make_shared<ObjectRows>(*sample());
        fewer->rows.pop_back();
        m.setRows(fewer);
        QCOMPARE(resets.count(), 1);
        auto reordered = std::make_shared<ObjectRows>(*fewer);
        std::swap(reordered->rows[0], reordered->rows[1]);
        m.setRows(reordered);
        QCOMPARE(resets.count(), 2);
        auto proposal = std::make_shared<ObjectRows>(*reordered);
        proposal->proposal = true;  // 從文件換成提案：結構性的改變
        m.setRows(proposal);
        QCOMPARE(resets.count(), 3);
    }

    void aStateChangeThatChangesTheFilteredRowsResets() {
        ObjectsModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.setRows(sample());
        m.setFilter(stateBit(OverlayState::Found));
        QSignalSpy resets(&m, &QAbstractItemModel::modelReset);
        auto next = std::make_shared<ObjectRows>(*sample());
        next->rows[3].state = OverlayState::Found;  // 篩選只看 Found：可見的列變了
        m.setRows(next);
        QCOMPARE(resets.count(), 1);
        QCOMPARE(m.visibleCount(), 4);
    }

    void aBigTableIsServedLazily() {
        auto rows = std::make_shared<ObjectRows>();
        rows->types.push_back(ObjectTypeRow{QString(), {{BoxGroupKind::Body, 1}}});
        for (int i = 1; i <= 5000; ++i) rows->rows.push_back(row(i, "C", OverlayState::Found, 0.9));
        ObjectsModel m;
        m.setRows(rows);
        QCOMPARE(m.rowCount(m.index(0, 0)), 5000);
        const QModelIndex last = m.index(4999, 0, m.index(0, 0));
        QCOMPARE(m.idAt(last), 5000);
        QCOMPARE(m.indexForId(2500).row(), 2499);
    }

    void retranslatingRefreshesTheStateNames() {
        ObjectsModel m;
        m.setRows(sample());
        const QModelIndex found = m.index(1, ObjectsModel::StateColumn, m.index(0, 0));
        QCOMPARE(text(m, found), QStringLiteral("找到"));
        Translations::install(QStringLiteral("en"));
        m.retranslate();
        QCOMPARE(text(m, found), QStringLiteral("Found"));
    }

    // ---- ObjectsPanel ----
    void thePanelShowsChipsWithCountsAndAnEmptyLabelWhenThereIsNothing() {
        ObjectsPanel panel;
        panel.show();
        QVERIFY(panel.emptyLabel()->isVisibleTo(&panel));
        QVERIFY(!panel.view()->isVisibleTo(&panel));
        panel.setRows(sample());
        QVERIFY(!panel.emptyLabel()->isVisibleTo(&panel));
        QVERIFY(panel.view()->isVisibleTo(&panel));
        QCOMPARE(panel.chip(OverlayState::Found)->text(), QStringLiteral("找到 3"));
        QCOMPARE(panel.chip(OverlayState::Sample)->text(), QStringLiteral("樣本 1"));
        QCOMPARE(panel.chip(OverlayState::Rejected)->text(), QStringLiteral("拒絕 1"));
        QCOMPARE(panel.chip(OverlayState::Inferred)->text(), QStringLiteral("推算 1"));
    }

    void chipsFilterTheTree() {
        ObjectsPanel panel;
        panel.setRows(sample());
        panel.chip(OverlayState::Rejected)->setChecked(false);
        panel.chip(OverlayState::Inferred)->setChecked(false);
        QCOMPARE(panel.model()->visibleCount(), 4);
        panel.chip(OverlayState::Rejected)->setChecked(true);
        QCOMPARE(panel.model()->visibleCount(), 5);
        panel.setFilter(stateBit(OverlayState::Sample));
        QCOMPARE(panel.model()->visibleCount(), 1);
        QVERIFY(panel.chip(OverlayState::Sample)->isChecked());
        QVERIFY(!panel.chip(OverlayState::Found)->isChecked());
    }

    void userSelectionIsReportedButProgrammaticSelectionIsNot() {
        ObjectsPanel panel;
        panel.resize(320, 400);
        panel.show();
        panel.setRows(sample());
        QSignalSpy user(&panel, &ObjectsPanel::selectionChangedByUser);

        panel.setSelection({2, 3});  // Presenter 要樹跟著動：不是使用者的操作
        QCOMPARE(user.count(), 0);
        QCOMPARE(panel.selectedIds(), (std::vector<int>{2, 3}));

        // 使用者在樹上選一列
        panel.view()->selectionModel()->select(panel.model()->indexForId(4),
                                               QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        QCOMPARE(user.count(), 1);
        QCOMPARE(user.first().first().value<std::vector<int>>(), (std::vector<int>{4}));
    }

    void selectingAGroupRowReportsItsComponent() {
        ObjectsPanel panel;
        panel.show();
        panel.setRows(sample());
        panel.view()->expandAll();
        QSignalSpy user(&panel, &ObjectsPanel::selectionChangedByUser);
        const QModelIndex group = panel.model()->index(1, 0, panel.model()->indexForId(1));
        panel.view()->selectionModel()->select(group, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        QCOMPARE(user.count(), 1);
        QCOMPARE(user.first().first().value<std::vector<int>>(), (std::vector<int>{1}));
    }

    void newDataKeepsTheSelectionWhenTheShapeIsUnchanged() {
        ObjectsPanel panel;
        panel.show();
        panel.setRows(sample());
        panel.setSelection({3});
        auto next = std::make_shared<ObjectRows>(*sample());
        next->rows[2].score = 0.5;
        panel.setRows(next);
        QCOMPARE(panel.selectedIds(), (std::vector<int>{3}));  // dataChanged 不丟選取
    }

    void selectionOfAnIdThatIsNotListedIsIgnored() {
        ObjectsPanel panel;
        panel.setRows(sample());
        panel.setSelection({99, 2});
        QCOMPARE(panel.selectedIds(), (std::vector<int>{2}));
        panel.setSelection({});
        QVERIFY(panel.selectedIds().empty());
    }

    void thePanelRetranslates() {
        ObjectsPanel panel;
        panel.setRows(sample());
        Translations::install(QStringLiteral("en"));
        panel.retranslate();
        QCOMPARE(panel.chip(OverlayState::Found)->text(), QStringLiteral("Found 3"));
    }
};

QTEST_MAIN(TstObjectsModel)
#include "tst_objects_model.moc"
