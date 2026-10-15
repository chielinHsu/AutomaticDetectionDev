#pragma once

#include <QWidget>
#include <memory>

#include "presentation/presenters/ViewInterfaces.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSlider;

namespace aoi::ui {

namespace Ui {
class ParamsPanel;
}

// 右側「比對參數」dock 的內容（ux-spec 第 4、8 節）。簡易模式只有分數門檻與比對範圍；打開「進階」多出角度範圍。
// S07 加「影像位元數」：自動（看最大值推測）或 8／10／12／14／16 bit；只有 16 bit 灰階的影像可以設定，其他停用。
// 實作 IParamsView；使用者的操作以訊號發出，MainWindow 轉給 IParamsController（View 不認識 Presenter）。
//
// 分數門檻的滑桿在拖曳中就發出訊號（不是放開才發）：U3 量的就是「滑桿變動到重繪」，
// 它只是對快取的候選重新過濾，所以可以每個滑鼠移動事件都做；防抖只用在需要重新搜尋的參數（角度範圍）。
//
// 版面在 ParamsPanel.ui（Qt Designer）；文字由 retranslate 從 copy.json 設定（.ui 裡的文字只是佔位、不翻譯），
// 影像位元數的項目（data ＝ 位元數）與訊號的接線在程式碼裡。
class ParamsPanel : public QWidget, public IParamsView {
    Q_OBJECT
public:
    explicit ParamsPanel(QWidget* parent = nullptr);
    ~ParamsPanel() override;

    // ---- IParamsView ----
    void setParams(const ParamsState& params) override;
    void setScoreRange(double floor, double max) override;
    void setEnabled(bool enabled) override;

    void retranslate();

    QSlider* scoreSlider() const { return scoreSlider_; }
    QLabel* scoreValue() const { return scoreValue_; }
    QCheckBox* advancedToggle() const { return advanced_; }
    QDoubleSpinBox* angleMin() const { return angleMin_; }
    QDoubleSpinBox* angleMax() const { return angleMax_; }
    QComboBox* scopeCombo() const { return scope_; }
    QComboBox* bitsCombo() const { return bits_; }
    QComboBox* positionCombo() const { return position_; }
    bool angleRowVisible() const;

signals:
    void thresholdChanged(double value);
    void angleRangeChanged(double minDeg, double maxDeg);
    void scopeChanged(int scopeIndex);
    void significantBitsChanged(int bits);  // 0 ＝ 自動
    void snapToLatticeChanged(bool snap);   // ADR-026：吸附到陣列／用匹配位置

private:
    void updateScoreLabel();
    void updateBitsTexts();

    std::unique_ptr<Ui::ParamsPanel> ui_;
    QLabel* scoreLabel_;
    QSlider* scoreSlider_;
    QLabel* scoreValue_;
    QCheckBox* advanced_;
    QLabel* angleLabel_;
    QWidget* angleRow_;
    QDoubleSpinBox* angleMin_;
    QDoubleSpinBox* angleMax_;
    QLabel* scopeLabel_;
    QComboBox* scope_;
    QLabel* bitsLabel_;
    QComboBox* bits_;    // 項目的 data 是位元數（0 ＝ 自動）
    QLabel* positionLabel_;
    QComboBox* position_;  // 0 ＝ 吸附到陣列、1 ＝ 用匹配位置（ADR-026）
    int detectedBits_ = 0;
    bool syncing_ = false;  // 程式設定的值不算使用者的操作
};

}  // namespace aoi::ui
