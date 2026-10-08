#include "presentation/presenters/BackendPresenter.h"

#include <QCoreApplication>
#include <QEvent>
#include <QMetaObject>
#include <exception>
#include <utility>

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

BackendPresenter::BackendPresenter(IBackendView& view, ISearchEngineSink& sink, BackendServices services, QObject* parent)
    : QObject(parent),
      view_(view),
      sink_(sink),
      services_(std::move(services)),
      switch_(services_.backends, services_.current ? services_.current->backend.id : std::string()),
      engine_(services_.current),
      noticePending_(engine_ && !engine_->backend.notices.empty()),
      poster_(std::make_shared<UiPoster>(this)) {
    if (QCoreApplication* app = QCoreApplication::instance()) app->installEventFilter(this);  // LanguageChange
    publish();
}

BackendPresenter::~BackendPresenter() {
    poster_->detach();  // 背景還在建立的 back end：結果回來時丟掉（它在那個 worker 上釋放）
}

// ---- 切換 ----------------------------------------------------------------------------------------------

void BackendPresenter::selectBackend(const std::string& id) {
    const BackendSwitch::RequestResult request = switch_.request(id);
    switch (request.kind) {
        case BackendSwitch::Request::AlreadyCurrent:
        case BackendSwitch::Request::Busy:
            publish();  // 選單回到正確的那一項
            return;
        case BackendSwitch::Request::Unknown:
        case BackendSwitch::Request::Unavailable:
            fail(id, request.error);  // 不必嘗試：登記時就知道不能用（或沒有這個 id）
            emit switchFinished(false);
            return;
        case BackendSwitch::Request::Start: break;
    }

    // latest-wins：進行中的搜尋先作廢（不等它停下）；換好之後再用新的 back end 搜尋同一個區域（researchAfter）。
    if (BackendSwitch::pausesSearch(sink_.interactionState())) sink_.pauseSearch();
    publish();  // 選單停用、狀態列「切換到 HALCON…」

    // 建立在背景執行緒（HALCON 的授權檢查約 1.5 秒，UI 不能卡住）；結果只經 UiPoster 回到 UI 執行緒。
    auto task = [create = services_.create, id, poster = poster_, this] {
        Result<SearchEnginePtr> result = makeError(ErrorCode::Internal, "No back end factory is set.");
        try {
            if (create) result = create(id);
        } catch (const std::exception& e) {
            result = makeError(ErrorCode::Internal, e.what());
        } catch (...) {
            result = makeError(ErrorCode::Internal, "Unknown exception while creating the back end.");
        }
        auto box = std::make_shared<Result<SearchEnginePtr>>(std::move(result));
        poster->post([this, id, box] { onCreated(id, std::move(*box)); });
    };
    if (services_.executor != nullptr) services_.executor->post(std::move(task));
    else task();  // 測試：同步建立，但結果一樣排到下一輪事件迴圈
}

void BackendPresenter::onCreated(const std::string& id, Result<SearchEnginePtr> result) {
    const BackendSwitch::Outcome outcome = switch_.finish(id, result.ok());
    if (outcome == BackendSwitch::Outcome::Stale) return;

    if (outcome == BackendSwitch::Outcome::Switched) {
        engine_ = std::move(result.value());
        failure_.reset();
        noticePending_ = !engine_->backend.notices.empty();
        if (!noticePending_) view_.hideNotice();  // 舊的提醒（或上次的失敗）是關於別的 back end 的
        const std::string fallback = switch_.fallback();
        sink_.useSearchEngine(useCaseOf(engine_), displayName(fallback.empty() ? switch_.current() : fallback));
        publish();
        QStringList messages;
        for (const BackendNotice& notice : engine_->backend.notices) messages << QString::fromStdString(notice.message);
        emit backendChanged(QString::fromStdString(id), messages);
    } else {
        fail(id, result.error());
    }
    if (BackendSwitch::researchAfter(outcome, sink_.interactionState())) sink_.researchSameRegion();
    emit switchFinished(outcome == BackendSwitch::Outcome::Switched);
}

// 留在原本的 back end；訊息列的卡片說原因（ui.error.backend.*），按〔關閉〕才收起。
void BackendPresenter::fail(const std::string& id, const Error& error) {
    failure_ = Failure{id, error};
    noticePending_ = true;
    publish();
    emit switchFailed(QString::fromStdString(id), QString::fromStdString(error.message));
}

void BackendPresenter::applyNoticeAction(int actionIndex) {
    // 提醒卡片只有一個按鈕：〔關閉〕。
    if (actionIndex != 0 || !noticePending_) return;
    noticePending_ = false;
    failure_.reset();
    view_.hideNotice();
}

// ---- 畫面 ----------------------------------------------------------------------------------------------

QString BackendPresenter::displayName(const std::string& id) const {
    if (const BackendInfo* info = switch_.find(id)) return QString::fromStdString(info->displayName);
    return QString::fromStdString(id);
}

void BackendPresenter::publish() {
    QStringList bodies;
    QStringList marks;
    if (engine_) {
        for (const BackendNotice& notice : engine_->backend.notices) {
            bodies << noticeText(notice.textId, notice.args);
            const QString mark = noticeText(notice.markId, notice.args);
            if (!mark.isEmpty()) marks << mark;
        }
    }

    // 選單：不可用的附原因；目前的那一個附提醒（S08 的提示照舊）。切換中選單顯示要換成的那一個（停用）。
    const std::string& shown = switch_.switching() ? switch_.pending() : switch_.current();
    QList<BackendChoice> choices;
    int current = 0;
    for (const BackendInfo& info : switch_.backends()) {
        BackendChoice choice;
        choice.id = QString::fromStdString(info.id);
        choice.name = QString::fromStdString(info.displayName);
        choice.available = info.available;
        if (!info.available) choice.note = QString::fromStdString(info.unavailable.message);
        if (info.id == switch_.current()) choice.note = bodies.join(QLatin1Char(' '));
        if (info.id == shown) current = static_cast<int>(choices.size());
        choices << choice;
    }
    view_.setBackends(choices, current, switch_.switching());

    // 狀態列：「HALCON · runtime 授權」（滑鼠移上去看完整說明）；切換中是「切換到 HALCON…」。
    const QString name = displayName(switch_.current());
    if (switch_.switching()) {
        const QString text = uiText("ui.backend.switching", {{"backend", displayName(switch_.pending())}});
        view_.setBackendStatus(text, text);
    } else {
        QStringList status{name};
        status << marks;
        view_.setBackendStatus(status.join(QStringLiteral(" · ")),
                               bodies.isEmpty() ? uiText("ui.backend.status.tip", {{"backend", name}})
                                                : bodies.join(QLatin1Char('\n')));
    }

    if (!noticePending_) return;
    MessageCard card;
    if (failure_) {
        card.title = uiText("ui.error.backend.title", {{"backend", displayName(failure_->id)}});
        card.body = uiText("ui.error.backend.body", {{"fallback", name}});
        card.advice << QString::fromStdString(failure_->error.message) << uiText("ui.backend.reverted", {{"backend", name}});
    } else {
        card.title = uiText("ui.backend.notice.title", {{"backend", name}});
        card.body = bodies.join(QLatin1Char('\n'));
    }
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
