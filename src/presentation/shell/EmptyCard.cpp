#include "presentation/shell/EmptyCard.h"

#include <QLabel>
#include <QPushButton>

#include "presentation/i18n/Translations.h"
#include "ui_EmptyCard.h"

namespace aoi::ui {

EmptyCard::EmptyCard(QWidget* parent) : QFrame(parent), ui_(std::make_unique<Ui::EmptyCard>()) {
    // 版面在 EmptyCard.ui（寬 380、置中的標題、說明、〔開啟影像…〕、拖放的提示）；文字由 retranslate 從 copy.json 設定。
    ui_->setupUi(this);
    setObjectName(QStringLiteral("emptyCard"));
    setBackgroundRole(QPalette::Base);
    title_ = ui_->title;
    body_ = ui_->body;
    open_ = ui_->open;
    drop_ = ui_->drop;
    // 標題比內文大 1.3 倍（跟著字級縮放：依目前的字型算，不寫死點數）。
    QFont f = title_->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 1.3);
    title_->setFont(f);
    drop_->setForegroundRole(QPalette::PlaceholderText);
    connect(open_, &QPushButton::clicked, this, &EmptyCard::openRequested);
    retranslate();
}

EmptyCard::~EmptyCard() = default;

void EmptyCard::retranslate() {
    title_->setText(qtTrId("ui.empty.title"));
    body_->setText(qtTrId("ui.empty.body"));
    open_->setText(qtTrId("ui.empty.action.open"));
    drop_->setText(qtTrId("ui.empty.hint.drop"));
}

}  // namespace aoi::ui
