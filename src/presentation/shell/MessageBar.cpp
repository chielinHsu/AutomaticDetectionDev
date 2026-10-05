#include "presentation/shell/MessageBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace aoi::ui {

MessageBar::MessageBar(const ThemeTokens& tokens, QWidget* parent) : QFrame(parent), tokens_(tokens) {
    setObjectName(QStringLiteral("messageBar"));
    setFrameShape(QFrame::NoFrame);
    setFixedHeight(tokens.sizes.messageBarHeight);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    scroll_ = new QScrollArea(this);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setBackgroundRole(QPalette::Base);
    outer->addWidget(scroll_);

    auto* content = new QWidget;
    content->setBackgroundRole(QPalette::Base);
    auto* column = new QVBoxLayout(content);
    column->setContentsMargins(12, 6, 12, 6);
    column->setSpacing(4);

    hint_ = new QLabel(content);
    hint_->setWordWrap(true);
    hint_->setForegroundRole(QPalette::Text);
    hint_->setObjectName(QStringLiteral("messageHint"));
    column->addWidget(hint_);

    card_ = new QWidget(content);
    card_->setObjectName(QStringLiteral("messageCard"));
    auto* cardColumn = new QVBoxLayout(card_);
    cardColumn->setContentsMargins(0, 0, 0, 0);
    cardColumn->setSpacing(2);
    cardTitle_ = new QLabel(card_);
    QFont bold = cardTitle_->font();
    bold.setBold(true);
    cardTitle_->setFont(bold);
    cardBody_ = new QLabel(card_);
    cardBody_->setWordWrap(true);
    cardAdvice_ = new QLabel(card_);
    cardAdvice_->setWordWrap(true);
    auto* actionRow = new QWidget(card_);
    actions_ = new QHBoxLayout(actionRow);
    actions_->setContentsMargins(0, 2, 0, 0);
    actions_->setSpacing(8);
    actions_->addStretch(1);
    cardColumn->addWidget(cardTitle_);
    cardColumn->addWidget(cardBody_);
    cardColumn->addWidget(cardAdvice_);
    cardColumn->addWidget(actionRow);
    column->addWidget(card_);

    // 進度列：「搜尋中… 62%」＋進度條＋〔取消 (Esc)〕。搜尋超過 1 s 才出現（ux-spec 第 9 節）。
    progress_ = new QWidget(content);
    progress_->setObjectName(QStringLiteral("messageProgress"));
    auto* progressRow = new QHBoxLayout(progress_);
    progressRow->setContentsMargins(0, 0, 0, 0);
    progressRow->setSpacing(8);
    progressText_ = new QLabel(progress_);
    progressBar_ = new QProgressBar(progress_);
    progressBar_->setRange(0, 100);
    progressBar_->setTextVisible(false);
    progressBar_->setFixedHeight(10);
    cancelButton_ = new QPushButton(progress_);
    cancelButton_->setObjectName(QStringLiteral("messageCancel"));
    connect(cancelButton_, &QPushButton::clicked, this, &MessageBar::cancelRequested);
    progressRow->addWidget(progressText_);
    progressRow->addWidget(progressBar_, 1);
    progressRow->addWidget(cancelButton_);
    column->addWidget(progress_);

    column->addStretch(1);
    scroll_->setWidget(content);
    card_->hide();
    progress_->hide();

    toastTimer_ = new QTimer(this);
    toastTimer_->setSingleShot(true);
    toastTimer_->setInterval(tokens.timing.toast);
    connect(toastTimer_, &QTimer::timeout, this, [this] { showHintLabel(baseHint_); });
}

void MessageBar::showHintLabel(const QString& text) {
    hint_->setText(text);
    hint_->setVisible(!text.isEmpty());
}

void MessageBar::setHint(const QString& text) {
    baseHint_ = text;
    // 有吐司在顯示時不要打斷它；吐司結束時會還原成最新的提示。卡片與進度列顯示時提示也不出現（它們關閉時會還原）。
    if (toastTimer_->isActive() || card_->isVisibleTo(this) || progress_->isVisibleTo(this)) return;
    showHintLabel(text);
}

void MessageBar::showToast(const QString& text) {
    showHintLabel(text);
    toastTimer_->start();
}

void MessageBar::showCard(const QString& title, const QString& body, const QStringList& advice,
                          const QStringList& actions) {
    cardTitle_->setText(title);
    cardBody_->setText(body);
    cardBody_->setVisible(!body.isEmpty());
    QStringList lines;
    for (const QString& a : advice) lines << QStringLiteral("• ") + a;
    cardAdvice_->setText(lines.join(QLatin1Char('\n')));
    cardAdvice_->setVisible(!lines.isEmpty());

    // 按鈕文字沒變就沿用：分數門檻拖動時，預覽卡片每個畫格都更新摘要，重建按鈕是白做工，還會搶走焦點。
    bool sameButtons = actions_->count() - 1 == actions.size();
    for (int i = 0; sameButtons && i < actions.size(); ++i) {
        auto* b = qobject_cast<QPushButton*>(actions_->itemAt(i)->widget());
        sameButtons = b != nullptr && b->text() == actions.at(i);
    }
    if (!sameButtons) {
        // 清掉舊按鈕（保留最後的彈性空間）。
        while (actions_->count() > 1) {
            QLayoutItem* item = actions_->takeAt(0);
            delete item->widget();
            delete item;
        }
        for (int i = 0; i < actions.size(); ++i) {
            auto* b = new QPushButton(actions.at(i), card_);
            connect(b, &QPushButton::clicked, this, [this, i] { emit actionTriggered(i); });
            actions_->insertWidget(i, b);
        }
    }
    progress_->hide();
    hint_->hide();
    card_->show();
}

void MessageBar::clearCard() {
    card_->hide();
    if (!progress_->isVisibleTo(this)) showHintLabel(toastTimer_->isActive() ? hint_->text() : baseHint_);
}

void MessageBar::showProgress(const QString& text, double fraction, const QString& cancelLabel) {
    progressText_->setText(text);
    cancelButton_->setText(cancelLabel);
    progressBar_->setValue(static_cast<int>(std::lround(std::clamp(fraction, 0.0, 1.0) * 100.0)));
    card_->hide();
    hint_->hide();
    progress_->show();
}

void MessageBar::clearProgress() {
    if (!progress_->isVisibleTo(this)) return;
    progress_->hide();
    if (!card_->isVisibleTo(this)) showHintLabel(toastTimer_->isActive() ? hint_->text() : baseHint_);
}

bool MessageBar::progressVisible() const { return progress_->isVisibleTo(this); }
int MessageBar::progressValue() const { return progressBar_->value(); }

bool MessageBar::cardVisible() const { return card_->isVisibleTo(this); }
QString MessageBar::hintText() const { return hint_->text(); }
QString MessageBar::cardTitle() const { return cardTitle_->text(); }

}  // namespace aoi::ui
