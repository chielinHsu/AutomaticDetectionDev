#include "presentation/presenters/BackendPresenter.h"

#include <QCoreApplication>
#include <QEvent>
#include <QMetaObject>
#include <QStringList>

#include "presentation/i18n/Translations.h"

namespace aoi::ui {

namespace {

// 提醒的文案：key 與 {名稱} 佔位字的值都在 BackendNotice 裡（組裝根決定，presentation 只負責翻譯）。
QString noticeText(const std::string& id, const std::vector<std::pair<std::string, std::string>>& args) {
    if (id.empty()) return QString();
    QString text = qtTrId(id.c_str());
    for (const auto& [name, value] : args) text = formatNamed(text, {{name.c_str(), QString::fromStdString(value)}});
    return text;
}

}  // namespace

BackendPresenter::BackendPresenter(IBackendView& view, BackendSetup setup, QObject* parent)
    : QObject(parent), view_(view), setup_(std::move(setup)), noticePending_(!setup_.notices.empty()) {
    if (QCoreApplication* app = QCoreApplication::instance()) app->installEventFilter(this);  // LanguageChange
    publish();
}

void BackendPresenter::applyNoticeAction(int actionIndex) {
    // 提醒卡片只有一個按鈕：〔關閉〕。
    if (actionIndex != 0 || !noticePending_) return;
    noticePending_ = false;
    view_.hideNotice();
}

QString BackendPresenter::currentName() const {
    for (const BackendInfo& info : setup_.backends) {
        if (info.id == setup_.current) return QString::fromStdString(info.displayName);
    }
    return QString::fromStdString(setup_.current);
}

void BackendPresenter::publish() {
    QStringList bodies;
    QStringList marks;
    for (const BackendNotice& notice : setup_.notices) {
        bodies << noticeText(notice.textId, notice.args);
        const QString mark = noticeText(notice.markId, notice.args);
        if (!mark.isEmpty()) marks << mark;
    }

    // 選單：不可用的附原因；目前的那一個附提醒（S08 的提示照舊）。
    QList<BackendChoice> choices;
    int current = 0;
    for (const BackendInfo& info : setup_.backends) {
        BackendChoice choice;
        choice.id = QString::fromStdString(info.id);
        choice.name = QString::fromStdString(info.displayName);
        choice.available = info.available;
        if (!info.available) choice.note = QString::fromStdString(info.unavailable.message);
        if (info.id == setup_.current) {
            current = static_cast<int>(choices.size());
            choice.note = bodies.join(QLatin1Char(' '));
        }
        choices << choice;
    }
    view_.setBackends(choices, current);

    // 狀態列：「HALCON · runtime 授權」，滑鼠移上去看完整說明。
    const QString name = currentName();
    QStringList status{name};
    status << marks;
    view_.setBackendStatus(status.join(QStringLiteral(" · ")),
                           bodies.isEmpty() ? uiText("ui.backend.status.tip", {{"backend", name}})
                                            : bodies.join(QLatin1Char('\n')));

    if (!noticePending_) return;
    MessageCard card;
    card.title = uiText("ui.backend.notice.title", {{"backend", name}});
    card.body = bodies.join(QLatin1Char('\n'));
    card.actions << uiText("action.dismiss");
    view_.showNotice(card);
}

void BackendPresenter::refreshTexts() { publish(); }

bool BackendPresenter::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::LanguageChange && watched == QCoreApplication::instance()) {
        // 下一輪事件迴圈再發布：此時 MainWindow 也正在處理同一個 LanguageChange（它會用舊的提示重建選單）。
        QMetaObject::invokeMethod(this, [this] { refreshTexts(); }, Qt::QueuedConnection);
    }
    return QObject::eventFilter(watched, event);
}

}  // namespace aoi::ui
