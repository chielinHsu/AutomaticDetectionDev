#pragma once

#include <QElapsedTimer>
#include <QWidget>
#include <memory>

#include "presentation/compare/CompareInterfaces.h"
#include "presentation/theme/ThemeTokens.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace aoi::ui {

class CompareOverlayItem;
class ImageCanvas;

namespace Ui {
class CompareWindow;
}

// A/B 比較的視窗（S09）：同一個樣本、兩個 back end 的結果疊在同一張影像上，旁邊是各階段的耗時與數量、差異的清單。
//
// 獨立的視窗（不是主視窗的一個狀態）：比較不改板子、不進復原堆疊，也不必經過主視窗的互動狀態機；主視窗照常可以用。
// 畫布是 ImageCanvas（縮放、平移、適配），工具一律停用（這裡只看不改），疊圖是 CompareOverlayItem。
// 版面在 CompareWindow.ui（Qt Designer）；文字由 retranslate 從 copy.json 設定（.ui 裡的文字只是佔位、不翻譯，ADR-022）。
// 實作 ICompareView；使用者的操作交給 ICompareController（View 不認識 Presenter）。
class CompareWindow : public QWidget, public ICompareView {
    Q_OBJECT
public:
    explicit CompareWindow(const ThemeTokens& tokens, QWidget* parent = nullptr);
    ~CompareWindow() override;

    void setController(ICompareController* controller) { controller_ = controller; }
    void applyTheme(const ThemeColors& colors);
    void retranslate();

    // ---- ICompareView ----
    void setChoices(const QList<BackendChoice>& choices, int a, int b) override;
    void showRequest(const aoi::ImageView& image, double thresholdA, double thresholdB) override;
    void showResult(const CompareResultView& result) override;
    void showBusy(bool busy, const QString& text) override;

    // 差異清單選了第 row 列：疊圖強調它、畫布移過去（放大到至少 100%）。
    void focusDifference(int row);

    // 測試與效能量測用。
    ImageCanvas* canvas() const { return canvas_; }
    CompareOverlayItem* overlayItem() const { return overlay_; }
    QComboBox* backendA() const;
    QComboBox* backendB() const;
    QPushButton* runButton() const;
    QPushButton* cancelButton() const;
    QDoubleSpinBox* thresholdA() const;
    QDoubleSpinBox* thresholdB() const;
    QCheckBox* legendCheck(aoi::CompareKind kind) const;
    QLabel* statusLabel() const;
    QTableWidget* summaryTable() const;
    QTableWidget* differencesTable() const;
    bool busy() const { return busy_; }

signals:
    // 新的結果交進來之後，畫布第一次畫完（ms ＝ showResult 到畫完；U1 類的量測）。
    void resultPainted(double ms);

protected:
    void changeEvent(QEvent* event) override;

private:
    void emitThresholds();

    std::unique_ptr<Ui::CompareWindow> ui_;
    ImageCanvas* canvas_ = nullptr;
    CompareOverlayItem* overlay_ = nullptr;
    ICompareController* controller_ = nullptr;
    CompareResultView result_;
    const void* imageData_ = nullptr;  // 目前畫的影像（換了影像才重畫）
    bool busy_ = false;
    bool syncing_ = false;  // 程式設定的值不算使用者的操作
    bool awaitingPaint_ = false;
    QElapsedTimer resultClock_;
};

}  // namespace aoi::ui
