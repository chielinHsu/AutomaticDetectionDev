#include "presentation/shell/MessageBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <algorithm>
#include <cmath>

#include "ui_MessageBar.h"

namespace aoi::ui {

MessageBar::MessageBar(QWidget* parent) : MessageBar(ThemeTokens::builtin(), parent) {}

MessageBar::~MessageBar() = default;

MessageBar::MessageBar(const ThemeTokens& tokens, QWidget* parent)
    : QFrame(parent), tokens_(tokens), ui_(std::make_unique<Ui::MessageBar>()) {
    // 版面在 MessageBar.ui（Qt Designer）：可捲動的一欄，依序是提示、卡片、提醒卡片（back end 的提醒、切換失敗；和卡片同樣的樣子，
    // 但優先順序低一級，showResting）、進度列（「搜尋中… 62%」＋進度條＋〔取消 (Esc)〕，搜尋超過 1 s 才出現，ux-spec 第 9 節）。
    // 卡片的按鈕依內容在執行時建立（fillCard）；高度與吐司的時間來自 tokens.json。
    ui_->setupUi(this);
    setObjectName(QStringLiteral("messageBar"));
    setFixedHeight(tokens.sizes.messageBarHeight);
    setBackgroundRole(QPalette::Base);
    scroll_ = ui_->scroll;
    scroll_->setBackgroundRole(QPalette::Base);
    ui_->content->setBackgroundRole(QPalette::Base);

    hint_ = ui_->messageHint;
    hint_->setForegroundRole(QPalette::Text);
    card_ = CardParts{ui_->messageCard, ui_->cardTitle, ui_->cardBody, ui_->cardAdvice, ui_->cardActions};
    notice_ = CardParts{ui_->messageNotice, ui_->noticeTitle, ui_->noticeBody, ui_->noticeAdvice, ui_->noticeActions};

    progress_ = ui_->messageProgress;
    progressText_ = ui_->progressText;
    progressBar_ = ui_->progressBar;
    cancelButton_ = ui_->messageCancel;
    connect(cancelButton_, &QPushButton::clicked, this, &MessageBar::cancelRequested);

    hint_->clear();
    card_.box->hide();
    notice_.box->hide();
    progress_->hide();

    toastTimer_ = new QTimer(this);
    toastTimer_->setSingleShot(true);
    toastTimer_->setInterval(tokens.timing.toast);
    connect(toastTimer_, &QTimer::timeout, this, [this] {
        if (cardVisible() || progressVisible()) hint_->hide();
        else showResting();
    });
}

void MessageBar::fillCard(CardParts& parts, const QString& title, const QString& body, const QStringList& advice,
                          const QStringList& actions, bool notice) {
    parts.title->setText(title);
    parts.body->setText(body);
    parts.body->setVisible(!body.isEmpty());
    QStringList lines;
    for (const QString& a : advice) lines << QStringLiteral("• ") + a;
    parts.advice->setText(lines.join(QLatin1Char('\n')));
    parts.advice->setVisible(!lines.isEmpty());

    // 按鈕文字沒變就沿用：分數門檻拖動時，預覽卡片每個畫格都更新摘要，重建按鈕是白做工，還會搶走焦點。
    bool sameButtons = parts.actions->count() - 1 == actions.size();
    for (int i = 0; sameButtons && i < actions.size(); ++i) {
        auto* b = qobject_cast<QPushButton*>(parts.actions->itemAt(i)->widget());
        sameButtons = b != nullptr && b->text() == actions.at(i);
    }
    if (sameButtons) return;
    // 清掉舊按鈕（保留最後的彈性空間）。
    while (parts.actions->count() > 1) {
        QLayoutItem* item = parts.actions->takeAt(0);
        delete item->widget();
        delete item;
    }
    for (int i = 0; i < actions.size(); ++i) {
        auto* b = new QPushButton(actions.at(i), parts.box);
        if (notice) connect(b, &QPushButton::clicked, this, [this, i] { emit noticeActionTriggered(i); });
        else connect(b, &QPushButton::clicked, this, [this, i] { emit actionTriggered(i); });
        parts.actions->insertWidget(i, b);
    }
}

void MessageBar::showHintLabel(const QString& text) {
    hint_->setText(text);
    hint_->setVisible(!text.isEmpty());
}

void MessageBar::showResting() {
    if (noticePending_) {
        notice_.box->show();
        if (!toastTimer_->isActive()) hint_->hide();  // 吐司顯示中就留著它（在提醒卡片上方）
        return;
    }
    notice_.box->hide();
    showHintLabel(toastTimer_->isActive() ? hint_->text() : baseHint_);
}

void MessageBar::setHint(const QString& text) {
    baseHint_ = text;
    // 有吐司在顯示時不要打斷它；吐司結束時會還原成最新的提示。卡片、提醒卡片與進度列顯示時提示也不出現（它們關閉時會還原）。
    if (toastTimer_->isActive() || cardVisible() || progressVisible() || noticeVisible()) return;
    showHintLabel(text);
}

void MessageBar::showToast(const QString& text) {
    showHintLabel(text);
    toastTimer_->start();
}

void MessageBar::showCard(const QString& title, const QString& body, const QStringList& advice,
                          const QStringList& actions) {
    fillCard(card_, title, body, advice, actions, false);
    progress_->hide();
    hint_->hide();
    notice_.box->hide();  // 讓位：卡片收起之後再出現
    card_.box->show();
}

void MessageBar::clearCard() {
    card_.box->hide();
    if (!progressVisible()) showResting();
}

void MessageBar::showNotice(const QString& title, const QString& body, const QStringList& advice,
                            const QStringList& actions) {
    fillCard(notice_, title, body, advice, actions, true);
    noticePending_ = true;
    if (cardVisible() || progressVisible()) return;  // 讓位
    showResting();
}

void MessageBar::clearNotice() {
    noticePending_ = false;
    notice_.box->hide();
    if (!cardVisible() && !progressVisible()) showResting();
}

void MessageBar::showProgress(const QString& text, double fraction, const QString& cancelLabel) {
    progressText_->setText(text);
    cancelButton_->setText(cancelLabel);
    progressBar_->setValue(static_cast<int>(std::lround(std::clamp(fraction, 0.0, 1.0) * 100.0)));
    card_.box->hide();
    notice_.box->hide();
    hint_->hide();
    progress_->show();
}

void MessageBar::clearProgress() {
    if (!progressVisible()) return;
    progress_->hide();
    if (!cardVisible()) showResting();
}

bool MessageBar::progressVisible() const { return progress_->isVisibleTo(this); }
int MessageBar::progressValue() const { return progressBar_->value(); }

bool MessageBar::cardVisible() const { return card_.box->isVisibleTo(this); }
bool MessageBar::noticeVisible() const { return notice_.box->isVisibleTo(this); }
bool MessageBar::noticePending() const { return noticePending_; }
QString MessageBar::hintText() const { return hint_->text(); }
QString MessageBar::cardTitle() const { return card_.title->text(); }
QString MessageBar::noticeTitle() const { return notice_.title->text(); }

}  // namespace aoi::ui
