#include "presentation/shell/EmptyCard.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "presentation/i18n/Translations.h"

namespace aoi::ui {

EmptyCard::EmptyCard(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("emptyCard"));
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);
    setFixedWidth(380);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(24, 20, 24, 20);
    column->setSpacing(10);
    title_ = new QLabel(this);
    QFont f = title_->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 1.3);
    title_->setFont(f);
    title_->setAlignment(Qt::AlignHCenter);
    body_ = new QLabel(this);
    body_->setWordWrap(true);
    body_->setAlignment(Qt::AlignHCenter);
    open_ = new QPushButton(this);
    open_->setDefault(true);
    drop_ = new QLabel(this);
    drop_->setWordWrap(true);
    drop_->setAlignment(Qt::AlignHCenter);
    drop_->setForegroundRole(QPalette::PlaceholderText);
    column->addWidget(title_);
    column->addWidget(body_);
    column->addWidget(open_, 0, Qt::AlignHCenter);
    column->addWidget(drop_);
    connect(open_, &QPushButton::clicked, this, &EmptyCard::openRequested);
    retranslate();
}

void EmptyCard::retranslate() {
    title_->setText(qtTrId("ui.empty.title"));
    body_->setText(qtTrId("ui.empty.body"));
    open_->setText(qtTrId("ui.empty.action.open"));
    drop_->setText(qtTrId("ui.empty.hint.drop"));
}

}  // namespace aoi::ui
