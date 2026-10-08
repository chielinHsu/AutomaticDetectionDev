#pragma once

#include <QWidget>
#include <array>
#include <memory>
#include <vector>

#include "presentation/objects/ObjectRows.h"
#include "presentation/objects/ObjectsModel.h"
#include "presentation/presenters/ViewInterfaces.h"

class QLabel;
class QToolButton;
class QTreeView;

namespace aoi::ui {

namespace Ui {
class ObjectsPanel;
}

// 右側「物件」dock 的內容：狀態篩選晶片、物件樹、空狀態文字。實作 IComponentTreeView。
//
// 選取同步是雙向的，靠兩個規則避免回圈：
//  - 使用者點樹 → 發出 selectionChangedByUser（MainWindow 轉給 ISelectionController，Presenter 再更新畫布）；
//  - Presenter 呼叫 setSelection → 只動樹的選取，**不**發出訊號（程式設定的選取不算使用者的操作）。
//
// 版面在 ObjectsPanel.ui（Qt Designer：晶片、樹、空狀態）；model（ObjectsModel）、欄寬與文字在程式碼裡
// （.ui 裡的文字只是佔位、不翻譯，retranslate 從 copy.json 設定）。
class ObjectsPanel : public QWidget, public IComponentTreeView {
    Q_OBJECT
public:
    explicit ObjectsPanel(QWidget* parent = nullptr);
    ~ObjectsPanel() override;

    // ---- IComponentTreeView ----
    void setRows(ObjectRowsPtr rows) override;
    void setSelection(const std::vector<int>& sceneIds) override;
    void setFilter(StateFilter filter) override;

    void retranslate();

    ObjectsModel* model() { return &model_; }
    QTreeView* view() const { return tree_; }
    QToolButton* chip(OverlayState state) const { return chips_[static_cast<std::size_t>(stateIndex(state))]; }
    QLabel* emptyLabel() const { return empty_; }
    // 樹目前選取的畫布編號（元件列；群組列算它的元件）。
    std::vector<int> selectedIds() const;

signals:
    void selectionChangedByUser(const std::vector<int>& sceneIds);

protected:
    void changeEvent(QEvent* event) override;

private:
    void resizeColumns();
    void updateChips();
    void onChipToggled();

    std::unique_ptr<Ui::ObjectsPanel> ui_;
    ObjectsModel model_;
    QTreeView* tree_;
    QLabel* empty_;
    std::array<QToolButton*, kOverlayStateCount> chips_{};
    bool syncing_ = false;
};

}  // namespace aoi::ui
