#pragma once

#include <QWidget>

#include "presentation/presenters/ViewInterfaces.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSlider;

namespace aoi::ui {

// 右側「比對參數」dock 的內容（ux-spec 第 4、8 節）。簡易模式只有分數門檻與比對範圍；打開「進階」多出角度範圍。
// 實作 IParamsView；使用者的操作以訊號發出，MainWindow 轉給 IParamsController（View 不認識 Presenter）。
//
// 分數門檻的滑桿在拖曳中就發出訊號（不是放開才發）：U3 量的就是「滑桿變動到重繪」，
// 它只是對快取的候選重新過濾，所以可以每個滑鼠移動事件都做；防抖只用在需要重新搜尋的參數（角度範圍）。
class ParamsPanel : public QWidget, public IParamsView {
    Q_OBJECT
public:
    explicit ParamsPanel(QWidget* parent = nullptr);

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
    bool angleRowVisible() const;

signals:
    void thresholdChanged(double value);
    void angleRangeChanged(double minDeg, double maxDeg);
    void scopeChanged(int scopeIndex);

private:
    void updateScoreLabel();

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
    bool syncing_ = false;  // 程式設定的值不算使用者的操作
};

}  // namespace aoi::ui
