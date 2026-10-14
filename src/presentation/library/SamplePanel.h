#pragma once

#include <QWidget>
#include <memory>
#include <vector>

#include "presentation/presenters/ViewInterfaces.h"

class QLabel;
class QLineEdit;
class QListWidget;

namespace aoi::ui {

namespace Ui {
class SamplePanel;
}

// 右側「樣本庫」dock 的內容（2026-10-14）：板子上的樣本清單。實作 ISampleListView。
//
// 每一筆：從目前影像裁下來的縮圖（Presenter 用 SampleListBuilder 做好）、名稱、寬 × 高（像素；有校正值時加 µm）、框數。
// 點一筆 → 發出 sampleClicked（MainWindow 轉給 ISelectionController::select）；Presenter 呼叫 setSelection → 只動清單的選取，
// 不發出訊號（和 ObjectsPanel 同一套規則）。沒有樣本時照舊顯示「樣本庫是空的」；篩選欄仍是停用的佔位（樣本庫檔案是 P2）。
// 版面在 SamplePanel.ui；文字由 retranslate 從 copy.json 設定。
class SamplePanel : public QWidget, public ISampleListView {
    Q_OBJECT
public:
    explicit SamplePanel(QWidget* parent = nullptr);
    ~SamplePanel() override;

    // ---- ISampleListView ----
    void setSamples(SampleListPtr samples) override;
    void setSelection(const std::vector<int>& sceneIds) override;

    void retranslate();

    QListWidget* list() const { return list_; }
    QLabel* emptyLabel() const { return empty_; }
    QLabel* headerLabel() const { return header_; }
    QLineEdit* filter() const { return filter_; }
    SampleListPtr samples() const { return samples_; }
    // 清單目前選取的畫布編號。
    std::vector<int> selectedIds() const;

signals:
    void sampleClicked(int sceneId);

private:
    void rebuild();
    QString summaryOf(const SampleEntry& e) const;

    std::unique_ptr<Ui::SamplePanel> ui_;
    QLineEdit* filter_;
    QLabel* header_;
    QListWidget* list_;
    QLabel* empty_;
    SampleListPtr samples_;
    bool syncing_ = false;
};

}  // namespace aoi::ui
