#include "presentation/params/ParamsPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <algorithm>
#include <cmath>

#include "presentation/i18n/Translations.h"

namespace aoi::ui {

ParamsPanel::ParamsPanel(QWidget* parent) : QWidget(parent) {
    auto* form = new QFormLayout(this);
    form->setContentsMargins(12, 8, 12, 8);

    scoreLabel_ = new QLabel(this);
    scoreSlider_ = new QSlider(Qt::Horizontal, this);
    scoreSlider_->setRange(30, 99);  // 百分之一為單位；搜尋下限 0.30，上限 0.99（InspectionSession::Config）
    scoreSlider_->setValue(70);
    scoreValue_ = new QLabel(this);
    scoreValue_->setMinimumWidth(fontMetrics().horizontalAdvance(QStringLiteral("0.00")) + 4);
    auto* scoreRow = new QWidget(this);
    auto* scoreLayout = new QHBoxLayout(scoreRow);
    scoreLayout->setContentsMargins(0, 0, 0, 0);
    scoreLayout->addWidget(scoreSlider_, 1);
    scoreLayout->addWidget(scoreValue_);
    form->addRow(scoreLabel_, scoreRow);

    advanced_ = new QCheckBox(this);
    form->addRow(QString(), advanced_);

    angleLabel_ = new QLabel(this);
    angleRow_ = new QWidget(this);
    auto* angleLayout = new QHBoxLayout(angleRow_);
    angleLayout->setContentsMargins(0, 0, 0, 0);
    angleMin_ = new QDoubleSpinBox(angleRow_);
    angleMax_ = new QDoubleSpinBox(angleRow_);
    for (QDoubleSpinBox* box : {angleMin_, angleMax_}) {
        box->setRange(-180.0, 180.0);
        box->setDecimals(1);
        box->setSingleStep(1.0);
        box->setSuffix(QStringLiteral("°"));
        box->setKeyboardTracking(false);  // 打字時不要每個字元都觸發重新搜尋，按 Enter 或離開才算
    }
    angleMin_->setValue(-5.0);
    angleMax_->setValue(5.0);
    angleLayout->addWidget(angleMin_, 1);
    angleLayout->addWidget(new QLabel(QStringLiteral("~"), angleRow_));
    angleLayout->addWidget(angleMax_, 1);
    form->addRow(angleLabel_, angleRow_);
    angleLabel_->setVisible(false);
    angleRow_->setVisible(false);

    scopeLabel_ = new QLabel(this);
    scope_ = new QComboBox(this);
    for (int i = 0; i < 3; ++i) scope_->addItem(QString());
    form->addRow(scopeLabel_, scope_);

    connect(scoreSlider_, &QSlider::valueChanged, this, [this](int v) {
        updateScoreLabel();
        if (!syncing_) emit thresholdChanged(v / 100.0);
    });
    connect(advanced_, &QCheckBox::toggled, this, [this](bool on) {
        angleLabel_->setVisible(on);
        angleRow_->setVisible(on);
    });
    auto emitAngles = [this] {
        if (syncing_) return;
        // 下限不能高過上限：使用者動到哪一邊，另一邊跟著走。
        if (angleMin_->value() > angleMax_->value()) {
            const QSignalBlocker blockMin(angleMin_);
            const QSignalBlocker blockMax(angleMax_);
            if (sender() == angleMin_) angleMax_->setValue(angleMin_->value());
            else angleMin_->setValue(angleMax_->value());
        }
        emit angleRangeChanged(angleMin_->value(), angleMax_->value());
    };
    connect(angleMin_, &QDoubleSpinBox::valueChanged, this, emitAngles);
    connect(angleMax_, &QDoubleSpinBox::valueChanged, this, emitAngles);
    connect(scope_, &QComboBox::activated, this, [this](int i) { emit scopeChanged(i); });

    retranslate();
}

bool ParamsPanel::angleRowVisible() const { return angleRow_->isVisibleTo(this); }

void ParamsPanel::updateScoreLabel() { scoreValue_->setText(QString::number(scoreSlider_->value() / 100.0, 'f', 2)); }

void ParamsPanel::setParams(const ParamsState& params) {
    syncing_ = true;
    {
        const QSignalBlocker blocker(scoreSlider_);  // 滑桿本身正在被拖時，不要把值再設回去
        const int v = static_cast<int>(std::lround(params.threshold * 100.0));
        if (scoreSlider_->value() != v) scoreSlider_->setValue(v);
    }
    updateScoreLabel();
    if (std::abs(angleMin_->value() - params.angleMinDeg) > 1e-9) angleMin_->setValue(params.angleMinDeg);
    if (std::abs(angleMax_->value() - params.angleMaxDeg) > 1e-9) angleMax_->setValue(params.angleMaxDeg);
    if (scope_->currentIndex() != params.scopeIndex) scope_->setCurrentIndex(params.scopeIndex);
    syncing_ = false;
}

void ParamsPanel::setScoreRange(double floor, double max) {
    const QSignalBlocker blocker(scoreSlider_);
    scoreSlider_->setRange(static_cast<int>(std::lround(floor * 100.0)), static_cast<int>(std::lround(max * 100.0)));
}

void ParamsPanel::setEnabled(bool enabled) { QWidget::setEnabled(enabled); }

void ParamsPanel::retranslate() {
    scoreLabel_->setText(uiText("ui.params.score"));
    advanced_->setText(uiText("ui.params.advanced"));
    angleLabel_->setText(uiText("ui.params.angle"));
    scopeLabel_->setText(uiText("ui.params.scanScope"));
    scope_->setItemText(0, uiText("ui.params.scope.auto"));
    scope_->setItemText(1, uiText("ui.params.scope.board"));
    scope_->setItemText(2, uiText("ui.params.scope.all"));
    updateScoreLabel();
}

}  // namespace aoi::ui
