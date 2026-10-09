#include "presentation/compare/ComparePresenter.h"

#include <QCoreApplication>
#include <QEvent>
#include <QMetaObject>
#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>

#include "presentation/i18n/Translations.h"

namespace aoi::ui {

namespace {

// 差異清單最多列幾個（真實影像上兩邊可以差上千個；表格太長沒有人看，疊圖上全部都有）。
constexpr int kMaxDifferenceRows = 1000;

QString number(double v, int decimals) { return QString::number(v, 'f', decimals); }

// 帶正負號的差（B − A）；負號用 U+2212，和表頭的「B − A」一致。
QString signedNumber(double d, int decimals) {
    if (std::abs(d) < 0.5 * std::pow(10.0, -decimals)) return number(0.0, decimals);
    return (d > 0 ? QStringLiteral("+") : QStringLiteral("−")) + number(std::abs(d), decimals);
}

QString signedCount(std::size_t a, std::size_t b) {
    const long long d = static_cast<long long>(b) - static_cast<long long>(a);
    if (d == 0) return QStringLiteral("0");
    return (d > 0 ? QStringLiteral("+") : QStringLiteral("−")) + QString::number(std::llabs(d));
}

CompareTableRow timeRow(const char* labelId, double a, double b) {
    return {uiText(labelId), number(a, 1), number(b, 1), signedNumber(b - a, 1)};
}

CompareTableRow countRow(const char* labelId, std::size_t a, std::size_t b) {
    return {uiText(labelId), QString::number(a), QString::number(b), signedCount(a, b)};
}

QString scoreText(const aoi::CompareHit& hit) {
    if (!hit.present) return QStringLiteral("—");
    if (!hit.score) return uiText("ui.compare.score.inferred");
    return number(*hit.score, 2);
}

}  // namespace

struct ComparePresenter::Outcome {
    SearchEnginePtr a;
    SearchEnginePtr b;
    std::string failedId;          // 建立失敗的那一個（空 ＝ 不是建立失敗）
    std::optional<Error> error;    // 建立或搜尋失敗（含取消）
    std::optional<aoi::SideBySide> result;
};

// worker 的進度 → UI 執行緒（只傳數字）。
class ComparePresenter::ProgressBridge final : public IProgressSink {
public:
    ProgressBridge(std::shared_ptr<UiPoster> poster, ComparePresenter* presenter, std::uint64_t generation)
        : poster_(std::move(poster)), presenter_(presenter), generation_(generation) {}
    void report(const ProgressEvent& event) noexcept override {
        try {
            poster_->post([p = presenter_, g = generation_, f = event.fraction] { p->onProgress(g, f); });
        } catch (...) {  // 契約：不丟例外
        }
    }

private:
    std::shared_ptr<UiPoster> poster_;
    ComparePresenter* presenter_;
    std::uint64_t generation_;
};

ComparePresenter::ComparePresenter(ICompareEntryView& entry, ICompareView& view, ICompareSource& source, CompareServices services,
                                   QObject* parent)
    : QObject(parent),
      entry_(entry),
      view_(view),
      source_(source),
      services_(std::move(services)),
      poster_(std::make_shared<UiPoster>(this)) {
    if (services_.executor != nullptr) runner_ = std::make_unique<JobRunner>(*services_.executor);
    for (const SearchEnginePtr& e : services_.ready) {
        if (e) engines_[e->backend.id] = e;
    }
    if (QCoreApplication* app = QCoreApplication::instance()) app->installEventFilter(this);  // LanguageChange
    refreshEntry();
}

ComparePresenter::~ComparePresenter() {
    poster_->detach();  // 背景還在跑的比較：結果回來時丟掉
    runner_.reset();    // 等進行中的比較結束（它只捕獲自己的資料與 poster）
}

// ---- 入口 ----------------------------------------------------------------------------------------------

std::vector<std::string> ComparePresenter::availableIds() const {
    std::vector<std::string> ids;
    for (const BackendInfo& info : services_.backends) {
        if (info.available) ids.push_back(info.id);
    }
    return ids;
}

bool ComparePresenter::available(const std::string& id) const {
    return std::any_of(services_.backends.begin(), services_.backends.end(),
                       [&](const BackendInfo& info) { return info.id == id && info.available; });
}

QString ComparePresenter::displayName(const std::string& id) const {
    for (const BackendInfo& info : services_.backends) {
        if (info.id == id) return QString::fromStdString(info.displayName);
    }
    return QString::fromStdString(id);
}

SearchEnginePtr ComparePresenter::engine(const std::string& id) const {
    const auto it = engines_.find(id);
    return it == engines_.end() ? SearchEnginePtr() : it->second;
}

// 註冊順序由簡單到完整（fake、opencv、halcon；ADR-021）。A ＝ 主視窗目前的；B ＝ 不是 A 的最後一個（最完整的）。
// 主視窗用的是最簡單的那一個（假的）而且還有兩個以上：比後面兩個（假的不看影像，和它比沒有意義）。
std::pair<std::string, std::string> ComparePresenter::defaultPair() const {
    const std::vector<std::string> ids = availableIds();
    if (ids.size() < 2) return {};
    std::string a = services_.currentBackend ? services_.currentBackend() : std::string();
    if (!available(a)) a = ids.front();
    if (a == ids.front() && ids.size() >= 3) a = ids[ids.size() - 2];
    std::string b;
    for (auto it = ids.rbegin(); it != ids.rend(); ++it) {
        if (*it != a) {
            b = *it;
            break;
        }
    }
    return {a, b};
}

void ComparePresenter::refreshEntry() {
    if (availableIds().size() < 2) {
        entry_.setCompareAvailable(false, uiText("ui.compare.entry.needTwo"));
    } else if (!source_.compareRequest()) {
        entry_.setCompareAvailable(false, uiText("ui.compare.entry.needSample"));
    } else {
        entry_.setCompareAvailable(true, uiText("ui.compare.entry.tip"));
    }
}

void ComparePresenter::openCompare() {
    const std::optional<CompareRequest> request = source_.compareRequest();
    const auto [a, b] = defaultPair();
    if (!request || a.empty() || b.empty()) {
        refreshEntry();
        return;
    }
    idA_ = a;
    idB_ = b;
    thresholdA_ = request->threshold;
    thresholdB_ = request->threshold;
    publishChoices();
    view_.showRequest(request->image, thresholdA_, thresholdB_);
    runCompare(QString::fromStdString(a), QString::fromStdString(b));
}

// ---- 比較 ----------------------------------------------------------------------------------------------

void ComparePresenter::runCompare(const QString& idA, const QString& idB) {
    const std::string a = idA.toStdString();
    const std::string b = idB.toStdString();
    const std::optional<CompareRequest> request = source_.compareRequest();
    QString problem;
    if (!request) problem = uiText("ui.compare.noSample");
    else if (a == b) problem = uiText("ui.compare.sameBackend");
    else if (!available(a)) problem = uiText("ui.compare.failed", {{"backend", displayName(a)}, {"reason", uiText("ui.compare.notUsable")}});
    else if (!available(b)) problem = uiText("ui.compare.failed", {{"backend", displayName(b)}, {"reason", uiText("ui.compare.notUsable")}});
    if (!problem.isEmpty()) {
        status_ = problem;
        view_.showBusy(false, status_);
        emit compareFinished(false);
        return;
    }

    idA_ = a;
    idB_ = b;
    request_ = request;
    view_.showRequest(request->image, thresholdA_, thresholdB_);
    publishChoices();
    ++generation_;
    const std::uint64_t generation = generation_;
    running_ = true;
    status_ = uiText("ui.compare.preparing", {{"a", displayName(a)}, {"b", displayName(b)}});
    view_.showBusy(true, status_);

    SampleRequest sampleRequest;
    sampleRequest.image = request->image;  // 帶著 owner：像素活到工作結束
    sampleRequest.sample = request->sample;
    sampleRequest.search = request->search;
    auto outcome = std::make_shared<Outcome>();
    outcome->a = engine(a);
    outcome->b = engine(b);
    auto bridge = std::make_shared<ProgressBridge>(poster_, this, generation);
    // work 只捕獲自己的資料（請求的複本、結果盒、工廠、橋）；engine 的 shared_ptr 讓 adapter 活到工作結束。
    auto work = [a, b, sampleRequest, outcome, bridge, create = services_.create](const CancelToken& cancel) {
        const auto make = [&](const std::string& id, SearchEnginePtr& slot) {
            if (slot) return true;
            Result<SearchEnginePtr> made = create ? create(id) : Result<SearchEnginePtr>(makeError(ErrorCode::Internal, "No back end factory is set."));
            if (!made) {
                outcome->failedId = id;
                outcome->error = made.error();
                return false;
            }
            slot = made.value();
            return true;
        };
        if (!make(a, outcome->a) || !make(b, outcome->b)) return;
        if (cancel.cancelled()) {
            outcome->error = makeError(ErrorCode::Cancelled, "The comparison was cancelled.");
            return;
        }
        Result<aoi::SideBySide> r = aoi::runSideBySide(*outcome->a->generate, *outcome->b->generate, sampleRequest, cancel, bridge.get());
        if (r) outcome->result = std::move(r.value());
        else outcome->error = r.error();
    };
    auto done = [poster = poster_, this, generation, outcome](const JobReport& report) {
        poster->post([this, generation, report, outcome] { onFinished(generation, report, outcome); });
    };
    if (runner_) {
        runner_->submit(std::move(work), std::move(done));
        return;
    }
    // 沒有執行緒池（測試）：在這裡同步跑，結果一樣排到下一輪事件迴圈。
    JobReport report;
    report.latest = true;
    try {
        work(CancelToken());
    } catch (const std::exception& e) {
        report.status = JobStatus::Failed;
        report.failure = e.what();
    }
    done(report);
}

void ComparePresenter::onProgress(std::uint64_t generation, double fraction) {
    if (generation != generation_ || !running_) return;
    const std::string& side = fraction < 0.5 ? idA_ : idB_;
    const QString text = uiText("ui.compare.running", {{"backend", displayName(side)}});
    if (text == status_) return;
    status_ = text;
    view_.showBusy(true, status_);
}

void ComparePresenter::onFinished(std::uint64_t generation, const JobReport& report, const std::shared_ptr<Outcome>& outcome) {
    // 建好的 back end 留著下次用（過期的比較也是：它建好的 back end 一樣能用）。
    if (outcome->a) engines_[outcome->a->backend.id] = outcome->a;
    if (outcome->b) engines_[outcome->b->backend.id] = outcome->b;
    if (generation != generation_) return;
    running_ = false;

    bool succeeded = false;
    if (report.status == JobStatus::Failed) {
        status_ = uiText("ui.compare.failed", {{"backend", displayName(idA_) + QStringLiteral(" / ") + displayName(idB_)},
                                               {"reason", QString::fromStdString(report.failure)}});
    } else if (outcome->error && outcome->error->code != ErrorCode::Cancelled) {
        const QString who = outcome->failedId.empty() ? displayName(idA_) + QStringLiteral(" / ") + displayName(idB_)
                                                      : displayName(outcome->failedId);
        status_ = uiText("ui.compare.failed", {{"backend", who}, {"reason", QString::fromStdString(outcome->error->message)}});
    } else if (!outcome->result) {
        status_ = uiText("ui.compare.cancelled");
    } else {
        proposalA_ = std::make_shared<const aoi::SampleProposal>(std::move(outcome->result->a));
        proposalB_ = std::make_shared<const aoi::SampleProposal>(std::move(outcome->result->b));
        comparison_.reset();
        publishResult();
        succeeded = true;
    }
    view_.showBusy(false, status_);
    emit compareFinished(succeeded);
}

void ComparePresenter::cancelCompare() {
    if (!running_) return;
    ++generation_;  // 舊的結果回來時丟掉（latest-wins：不等它停下）
    if (runner_) runner_->cancel();
    running_ = false;
    status_ = uiText("ui.compare.cancelled");
    view_.showBusy(false, status_);
    emit compareFinished(false);
}

void ComparePresenter::setThresholds(double a, double b) {
    thresholdA_ = a;
    thresholdB_ = b;
    if (!proposalA_ || !proposalB_) return;
    comparison_.reset();  // 只重新比較（不重新搜尋）
    publishResult();
}

// ---- 畫面 ----------------------------------------------------------------------------------------------

void ComparePresenter::publishChoices() {
    QList<BackendChoice> choices;
    int a = 0;
    int b = 0;
    for (const BackendInfo& info : services_.backends) {
        BackendChoice choice;
        choice.id = QString::fromStdString(info.id);
        choice.name = QString::fromStdString(info.displayName);
        choice.available = info.available;
        if (!info.available) choice.note = QString::fromStdString(info.unavailable.message);
        if (info.id == idA_) a = static_cast<int>(choices.size());
        if (info.id == idB_) b = static_cast<int>(choices.size());
        choices << choice;
    }
    view_.setChoices(choices, a, b);
}

void ComparePresenter::publishResult() {
    if (!proposalA_ || !proposalB_ || !request_) return;
    if (!comparison_) {
        comparison_ = aoi::compareProposals(*proposalA_, thresholdA_, *proposalB_, thresholdB_);
        auto overlay = std::make_shared<CompareOverlay>();
        const aoi::Size2D part = proposalA_->partSize.w > 0.0 ? proposalA_->partSize : proposalB_->partSize;
        overlay->partSize = QSizeF(part.w, part.h);
        overlay->marks.reserve(comparison_->items.size());
        for (const aoi::ComparedItem& item : comparison_->items) {
            const aoi::Pose2D p = item.pose();
            overlay->marks.push_back({QPointF(p.x, p.y), p.angleDeg, item.kind});
        }
        result_.overlay = std::move(overlay);
    }
    const aoi::CompareSummary& s = comparison_->summary;
    const QString nameA = displayName(idA_);
    const QString nameB = displayName(idB_);

    CompareResultView& r = result_;
    r.title = uiText("ui.compare.title", {{"a", nameA}, {"b", nameB}, {"image", request_->imageName}});
    r.status = uiText("ui.compare.done", {{"a", nameA}, {"msA", number(s.a.timings.totalMs, 1)}, {"b", nameB}, {"msB", number(s.b.timings.totalMs, 1)}});
    status_ = r.status;
    r.legend = QStringList{uiText("ui.compare.kind.both", {{"n", QString::number(s.both)}}),
                           uiText("ui.compare.kind.onlyA", {{"n", QString::number(s.onlyA)}}),
                           uiText("ui.compare.kind.onlyB", {{"n", QString::number(s.onlyB)}})};

    r.table.clear();
    r.table << timeRow("ui.compare.row.template", s.a.timings.templateMs, s.b.timings.templateMs)
            << timeRow("ui.compare.row.seed", s.a.timings.seedMs, s.b.timings.seedMs)
            << timeRow("ui.compare.row.fit", s.a.timings.fitMs, s.b.timings.fitMs)
            << timeRow("ui.compare.row.verify", s.a.timings.verifyMs, s.b.timings.verifyMs)
            << timeRow("ui.compare.row.total", s.a.timings.totalMs, s.b.timings.totalMs)
            << countRow("ui.compare.row.found", s.a.found, s.b.found)
            << countRow("ui.compare.row.inferred", s.a.inferred, s.b.inferred)
            << countRow("ui.compare.row.rejected", s.a.rejected, s.b.rejected)
            << countRow("ui.compare.row.only", s.onlyA, s.onlyB);
    r.table << CompareTableRow{uiText("ui.compare.row.position"), number(s.meanDistancePx, 3), number(s.maxDistancePx, 3), QString()};
    r.table << CompareTableRow{uiText("ui.compare.row.angle"), number(s.maxAngleDiffDeg, 2), QString(), QString()};

    r.differences.clear();
    int index = 0;
    for (const aoi::ComparedItem& item : comparison_->items) {
        const int markIndex = index++;
        if (item.kind == aoi::CompareKind::Both) continue;
        if (r.differences.size() >= kMaxDifferenceRows) break;
        const bool onlyA = item.kind == aoi::CompareKind::OnlyA;
        const QString other = onlyA ? nameB : nameA;
        const aoi::CompareHit& otherHit = onlyA ? item.b : item.a;
        QString reason;
        switch (item.gap) {
            case aoi::CompareGap::BelowThreshold:
                reason = uiText("ui.compare.gap.belowThreshold",
                                {{"backend", other}, {"score", scoreText(otherHit)}, {"threshold", number(onlyA ? thresholdB_ : thresholdA_, 2)}});
                break;
            case aoi::CompareGap::InferredOnly: reason = uiText("ui.compare.gap.inferredOnly", {{"backend", other}}); break;
            case aoi::CompareGap::NotFound: reason = uiText("ui.compare.gap.notFound", {{"backend", other}}); break;
            case aoi::CompareGap::None: break;
        }
        const aoi::Pose2D p = item.pose();
        r.differences << CompareDifferenceRow{uiText("ui.compare.diff.only", {{"backend", onlyA ? nameA : nameB}}),
                                              QStringLiteral("%1, %2").arg(number(p.x, 1), number(p.y, 1)), scoreText(item.a),
                                              scoreText(item.b), reason, markIndex};
    }
    const std::size_t differences = s.onlyA + s.onlyB;
    r.differencesTitle = differences > static_cast<std::size_t>(kMaxDifferenceRows)
                             ? uiText("ui.compare.differences.capped", {{"n", QString::number(differences)}, {"shown", QString::number(kMaxDifferenceRows)}})
                             : uiText("ui.compare.differences", {{"n", QString::number(differences)}});
    view_.showResult(r);
}

void ComparePresenter::refreshTexts() {
    refreshEntry();
    publishChoices();
    if (proposalA_ && proposalB_) publishResult();
}

bool ComparePresenter::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::LanguageChange && watched == QCoreApplication::instance()) {
        // 下一輪事件迴圈再發布：此時視窗也正在處理同一個 LanguageChange（它會重設靜態的文字）。
        QMetaObject::invokeMethod(this, [this] { refreshTexts(); }, Qt::QueuedConnection);
    }
    return QObject::eventFilter(watched, event);
}

}  // namespace aoi::ui
