#pragma once

#include <QWidget>
#include <array>
#include <memory>
#include <vector>

#include "presentation/objects/ObjectRows.h"
#include "presentation/objects/ObjectsModel.h"
#include "presentation/presenters/ViewInterfaces.h"

class QCheckBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QToolButton;
class QTreeView;

namespace aoi::ui {

namespace Ui {
class ObjectsPanel;
}

// 右側「物件」dock 的內容：狀態篩選晶片、物件樹、空狀態文字，以及樹下方的**選取資訊**表單（2026-10-14，ux-spec §4）。
// 實作 IComponentTreeView 與 ISelectionInfoView。
//
// 選取同步是雙向的，靠兩個規則避免回圈：
//  - 使用者點樹 → 發出 selectionChangedByUser（MainWindow 轉給 ISelectionController，Presenter 再更新畫布）；
//  - Presenter 呼叫 setSelection → 只動樹的選取，**不**發出訊號（程式設定的選取不算使用者的操作）。
//
// 選取資訊表單：恰好一個元件被選取時顯示它的名稱與類型、中心 X／Y、寬 × 高（像素；有校正值時旁邊顯示 µm）、角度、框數。
// 中心與寬高可以改：按 Enter 或離開欄位才發出一次 centerEdited／sizeEdited（一次輸入一個 Command，ADR-002 規則 7）；
// 程式設定的值（setSelectionInfo）不發出。能不能改由 SelectionInfoView 的 canMove／canResize 決定（預覽中只能改位置）；
// 多選時整個表單停用、只顯示「已選 N 個」。〔鎖定寬高比〕只是表單的行為：改寬時高跟著變（反之亦然），不是 Command。
// 〔套用大小到所有樣本〕發出 applySizeToAllRequested。
//
// 版面在 ObjectsPanel.ui（Qt Designer：晶片、樹、空狀態、表單）；model（ObjectsModel）、欄寬與文字在程式碼裡
// （.ui 裡的文字只是佔位、不翻譯，retranslate 從 copy.json 設定）。
class ObjectsPanel : public QWidget, public IComponentTreeView, public ISelectionInfoView {
    Q_OBJECT
public:
    explicit ObjectsPanel(QWidget* parent = nullptr);
    ~ObjectsPanel() override;

    // ---- IComponentTreeView ----
    void setRows(ObjectRowsPtr rows) override;
    void setSelection(const std::vector<int>& sceneIds) override;
    void setFilter(StateFilter filter) override;

    // ---- ISelectionInfoView ----
    void setSelectionInfo(const SelectionInfoView& info) override;

    void retranslate();

    ObjectsModel* model() { return &model_; }
    QTreeView* view() const { return tree_; }
    QToolButton* chip(OverlayState state) const { return chips_[static_cast<std::size_t>(stateIndex(state))]; }
    QLabel* emptyLabel() const { return empty_; }
    // 樹目前選取的畫布編號（元件列；群組列算它的元件）。
    std::vector<int> selectedIds() const;

    // 選取資訊表單的控制項（測試用）。
    QGroupBox* selectionBox() const { return selectionBox_; }
    QLabel* selectionSummary() const { return selectionSummary_; }
    QDoubleSpinBox* centerX() const { return centerX_; }
    QDoubleSpinBox* centerY() const { return centerY_; }
    QDoubleSpinBox* sizeW() const { return sizeW_; }
    QDoubleSpinBox* sizeH() const { return sizeH_; }
    QLabel* sizeUm() const { return sizeUm_; }
    QCheckBox* lockRatio() const { return lockRatio_; }
    QLabel* angleValue() const { return angleValue_; }
    QLabel* boxesValue() const { return boxesValue_; }
    QPushButton* applyToAll() const { return applyToAll_; }
    QLabel* selectionNote() const { return selectionNote_; }
    const SelectionInfoView& selectionInfo() const { return info_; }

signals:
    void selectionChangedByUser(const std::vector<int>& sceneIds);
    // 選取資訊表單：按 Enter 或離開欄位，而且值和顯示的不同時才發出一次。
    void centerEdited(double x, double y);
    void sizeEdited(double width, double height);
    void applySizeToAllRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    void resizeColumns();
    void updateChips();
    void onChipToggled();
    void commitCenter();
    void commitSize();
    void followRatio(bool widthChanged);
    void updateSelectionTexts();

    std::unique_ptr<Ui::ObjectsPanel> ui_;
    ObjectsModel model_;
    QTreeView* tree_;
    QLabel* empty_;
    std::array<QToolButton*, kOverlayStateCount> chips_{};
    bool syncing_ = false;

    QGroupBox* selectionBox_;
    QLabel* selectionSummary_;
    QLabel* centerLabel_;
    QDoubleSpinBox* centerX_;
    QDoubleSpinBox* centerY_;
    QLabel* sizeLabel_;
    QDoubleSpinBox* sizeW_;
    QDoubleSpinBox* sizeH_;
    QLabel* sizeUm_;
    QCheckBox* lockRatio_;
    QLabel* angleLabel_;
    QLabel* angleValue_;
    QLabel* boxesLabel_;
    QLabel* boxesValue_;
    QPushButton* applyToAll_;
    QLabel* selectionNote_;
    SelectionInfoView info_;
    bool infoSyncing_ = false;
};

}  // namespace aoi::ui
