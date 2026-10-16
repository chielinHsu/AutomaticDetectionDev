#include "presentation/params/ParamsPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <algorithm>
#include <cmath>

#include "domain/image_depth.h"
#include "presentation/i18n/Translations.h"
#include "ui_ParamsPanel.h"

namespace aoi::ui {

ParamsPanel::ParamsPanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::ParamsPanel>()) {
    // 版面在 ParamsPanel.ui：分數門檻 30..99（百分之一為單位；搜尋下限 0.30，上限 0.99，InspectionSession::Config）、
    // 角度 −180..180°、一位小數、keyboardTracking 關（打字時不要每個字元都觸發重新搜尋，按 Enter 或離開才算）。
    ui_->setupUi(this);
    scoreLabel_ = ui_->scoreLabel;
    scoreSlider_ = ui_->scoreSlider;
    scoreValue_ = ui_->scoreValue;
    advanced_ = ui_->advanced;
    angleLabel_ = ui_->angleLabel;
    angleRow_ = ui_->angleRow;
    angleMin_ = ui_->angleMin;
    angleMax_ = ui_->angleMax;
    scopeLabel_ = ui_->scopeLabel;
    scope_ = ui_->scope;
    bitsLabel_ = ui_->bitsLabel;
    bits_ = ui_->bits;
    positionLabel_ = ui_->positionLabel;
    position_ = ui_->position;

    scoreValue_->setMinimumWidth(fontMetrics().horizontalAdvance(QStringLiteral("0.00")) + 4);
    angleLabel_->setVisible(false);  // 「進階」打開才看得到
    angleRow_->setVisible(false);
    bits_->addItem(QString(), 0);
    for (const int b : aoi::kSignificantBitsChoices) bits_->addItem(QString(), b);

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
    connect(bits_, &QComboBox::activated, this, [this](int i) { emit significantBitsChanged(bits_->itemData(i).toInt()); });
    connect(position_, &QComboBox::activated, this, [this](int i) { emit snapToLatticeChanged(i == 0); });

    retranslate();
}

ParamsPanel::~ParamsPanel() = default;

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
    detectedBits_ = params.detectedBits;
    updateBitsTexts();
    const int bitsIndex = bits_->findData(params.significantBits);
    bits_->setCurrentIndex(bitsIndex < 0 ? 0 : bitsIndex);
    bits_->setEnabled(params.bitsApplicable);
    const int positionIndex = params.snapToLattice ? 0 : 1;
    if (position_->currentIndex() != positionIndex) position_->setCurrentIndex(positionIndex);
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
    bitsLabel_->setText(uiText("ui.params.bits"));
    positionLabel_->setText(uiText("ui.params.position"));
    position_->setItemText(0, uiText("ui.params.position.lattice"));
    position_->setItemText(1, uiText("ui.params.position.matched"));
    position_->setToolTip(uiText("ui.params.position.tip"));
    updateBitsTexts();
    updateScoreLabel();
}

void ParamsPanel::updateBitsTexts() {
    bits_->setItemText(0, detectedBits_ > 0 ? uiText("ui.params.bits.auto", {{"bits", QString::number(detectedBits_)}})
                                            : uiText("ui.params.bits.autoNone"));
    for (int i = 1; i < bits_->count(); ++i) bits_->setItemText(i, uiText("ui.params.bits.value", {{"bits", QString::number(bits_->itemData(i).toInt())}}));
}

}  // namespace aoi::ui
