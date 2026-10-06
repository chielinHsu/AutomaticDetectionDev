#include "presentation/presenters/AutoRoiPresenter.h"

#include <QCoreApplication>
#include <QEvent>
#include <QMetaObject>
#include <algorithm>
#include <chrono>

#include "presentation/i18n/Translations.h"

namespace aoi::ui {

PresenterSettings PresenterSettings::fromTokens(const ThemeTokens& tokens) {
    PresenterSettings s;
    s.busyAfterMs = tokens.timing.busyAfter;
    s.progressAfterMs = tokens.timing.progressAfter;
    s.busyMinShowMs = tokens.timing.busyMinShow;
    s.minSamplePx = tokens.interaction.minSampleSidePx;
    return s;
}

// 搜尋中的工作送回來的結果。work 寫、UI 執行緒讀；done 之後才讀，所以不需要另外的鎖。
struct AutoRoiPresenter::JobOutcome {
    std::optional<Result<SampleProposal>> result;
};

// use case 的進度 → UI 執行緒。report 在 worker 上被呼叫（IProgressSink 的契約：要很快、不丟例外），
// 所以只做節流（每 40 ms 最多一次，每個階段結束一定送）與 post。
class AutoRoiPresenter::ProgressBridge final : public IProgressSink {
public:
    ProgressBridge(std::shared_ptr<UiPoster> poster, AutoRoiPresenter* presenter, std::uint64_t generation)
        : poster_(std::move(poster)), presenter_(presenter), generation_(generation) {}

    void report(const ProgressEvent& event) noexcept override {
        try {
            using namespace std::chrono;
            const auto now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
            const long long previous = lastMs_.load(std::memory_order_relaxed);
            if (event.fraction < 1.0 && now - previous < 40) return;
            lastMs_.store(now, std::memory_order_relaxed);

            const double fraction = std::clamp(event.fraction, 0.0, 1.0);
            double overall = fraction;
            switch (event.stage) {  // 流程 A 有四個階段，各佔四分之一；流程 B 的 Match 本身就是 0..1
                case Stage::Template: overall = fraction / 4.0; break;
                case Stage::Seed: overall = (1.0 + fraction) / 4.0; break;
                case Stage::Fit: overall = (2.0 + fraction) / 4.0; break;
                case Stage::Verify: overall = (3.0 + fraction) / 4.0; break;
                case Stage::Match: break;
            }
            poster_->post([p = presenter_, g = generation_, overall] { p->onProgress(g, overall); });
        } catch (...) {
        }
    }

private:
    std::shared_ptr<UiPoster> poster_;
    AutoRoiPresenter* presenter_;
    std::uint64_t generation_;
    std::atomic<long long> lastMs_{0};
};

namespace {

InspectionSession::Config sessionConfig(const PresenterSettings& settings) {
    InspectionSession::Config config;
    config.minSamplePx = settings.minSamplePx;
    return config;
}

Rect toRect(const QRectF& r) { return Rect{r.x(), r.y(), r.width(), r.height()}; }

RegionMode toRegionMode(DraftMode m) { return m == DraftMode::Sample ? RegionMode::Sample : RegionMode::Region; }
DraftMode toDraftMode(RegionMode m) { return m == RegionMode::Sample ? DraftMode::Sample : DraftMode::Region; }

}  // namespace

AutoRoiPresenter::AutoRoiPresenter(PresenterViews views, PresenterServices services, PresenterSettings settings,
                                   QObject* parent)
    : QObject(parent),
      views_(views),
      services_(std::move(services)),
      settings_(settings),
      session_(sessionConfig(settings)),
      poster_(std::make_shared<UiPoster>(this)) {
    busyTimer_.setSingleShot(true);
    progressTimer_.setSingleShot(true);
    connect(&busyTimer_, &QTimer::timeout, this, &AutoRoiPresenter::showBusyNow);
    connect(&progressTimer_, &QTimer::timeout, this, &AutoRoiPresenter::showProgressNow);
    if (QCoreApplication* app = QCoreApplication::instance()) app->installEventFilter(this);  // LanguageChange

    views_.params.setScoreRange(session_.config().thresholdMin, session_.config().thresholdMax);
    views_.params.setParams(currentParams());
    rebuildLabels();
    publishAll();
}

AutoRoiPresenter::~AutoRoiPresenter() {
    poster_->detach();  // 之後 worker 再 post 的都丟掉
    services_.runner.cancel();
}

// ---- 影像 ----------------------------------------------------------------------------------------------

void AutoRoiPresenter::onImageOpened(const aoi::ImageView& image, const QString& name) {
    invalidateJobs();
    imageName_ = name;
    sourceImage_ = image;  // 共用像素（零複製）；搜尋的 worker 讀同一塊
    image_ = image;
    session_.imageOpened(name.toStdString(), image.width, image.height, image.umPerPixel);
    // 有效位元數的設定換影像沿用（相機的設定；使用者 2026-10-06 決定）：套到新影像上。新影像的值超過設定（BelowData）時用推測的值並說明。
    const int requested = session_.document().board().significantBits;
    const SignificantBitsDecision bits = applyBitsSetting(requested);
    if (bits.choice == SignificantBitsChoice::BelowData) {
        views_.status.showToast(uiText("ui.params.bits.rejected", {{"requested", QString::number(requested)},
                                                                    {"bits", QString::number(bits.bits)}}));
    }
    lastMs_ = -1.0;
    nudgeClock_.invalidate();
    views_.canvas.showRegionDraft(std::nullopt, DraftMode::Sample);
    rebuildLabels();
    views_.params.setParams(currentParams());
    publishAll();
}

void AutoRoiPresenter::onImageOpenFailed(const QString& name, const QString& reason) {
    imageName_ = name;
    if (!session_.imageFailed(makeError(ErrorCode::FormatInvalid, reason.toStdString()))) {
        // 目前的狀態不收這個事件（例如搜尋中）：不打斷進行中的工作，只用吐司說一聲。
        views_.status.showToast(uiText("ui.error.imageOpen.title"));
        return;
    }
    publishAll();
}

void AutoRoiPresenter::onPointerDragChanged(bool active, ToolId tool) {
    if (!active) return;
    if (tool != ToolId::Sample && tool != ToolId::Region) return;
    drawMode_ = tool == ToolId::Sample ? RegionMode::Sample : RegionMode::Region;
    if (session_.beginRegion(drawMode_)) publishChrome();
}

// ---- ICanvasController ---------------------------------------------------------------------------------

void AutoRoiPresenter::onRegionDrawn(const QRectF& rect, DraftMode mode) {
    const RegionMode regionMode = toRegionMode(mode);
    // 工具只在放開時回報；按下時的 onPointerDragChanged 沒有送達（例如測試直接呼叫）也要能走完。
    if (session_.state() != InteractionState::DrawingRegion && !session_.beginRegion(regionMode)) {
        views_.canvas.showRegionDraft(std::nullopt, mode);
        return;
    }
    switch (session_.endRegion(toRect(rect), regionMode)) {
        case InspectionSession::Release::Ignored:
            break;
        case InspectionSession::Release::TooSmall:
            views_.status.showToast(uiText("ui.sample.tooSmall", {{"min", QString::number(settings_.minSamplePx)}}));
            publishChrome();
            publishDraft();  // 回到進入前的狀態：Idle 時清掉框，Preview／NoMatch 時還原該提案的區域
            break;
        case InspectionSession::Release::Search:
            if (regionMode == RegionMode::Region) finishRegionScan();
            else startSearch();
            break;
    }
}

void AutoRoiPresenter::onRegionAborted() {
    if (!session_.abortRegion()) return;
    publishChrome();
    publishDraft();
}

void AutoRoiPresenter::onAddRequested(const QPointF& p) {
    nudgeClock_.invalidate();
    switch (session_.addManual(Pose2D{p.x(), p.y(), 0.0})) {
        case InspectionSession::AddResult::Added: publishAll(); break;
        case InspectionSession::AddResult::NeedType: views_.status.showToast(uiText("ui.add.needType")); break;
        case InspectionSession::AddResult::NotAllowed: break;
    }
}

void AutoRoiPresenter::onComponentsMoved(const std::vector<int>& sceneIds, const QPointF& delta) {
    nudgeClock_.invalidate();
    if (delta.isNull()) {
        publishDraft();  // 放棄拖曳：拖曳中蓋掉的區域框要還原，不產生 Command
        return;
    }
    session_.select(toSessionIds(sceneIds));
    if (session_.moveSelection(Point2D{delta.x(), delta.y()}, false) > 0) publishAll();
    else publishDraft();
}

void AutoRoiPresenter::onOpenTypeEditor(int, const QPointF&) {
    // 流程 C（類型編輯器）不在 S04 的範圍：狀態機有 EditingType，但編輯器本身還沒有。
    views_.status.showToast(uiText("ui.editor.unavailable"));
}

// ---- ISelectionController ------------------------------------------------------------------------------

void AutoRoiPresenter::select(const std::vector<int>& sceneIds) {
    nudgeClock_.invalidate();
    session_.select(toSessionIds(sceneIds));
    publishSelection();
}

void AutoRoiPresenter::deleteSelected() {
    nudgeClock_.invalidate();
    if (session_.deleteSelection() == 0) return;
    publishAll();
}

// ---- IKeyboardController -------------------------------------------------------------------------------

bool AutoRoiPresenter::onKeyPressed(int key, Qt::KeyboardModifiers modifiers) {
    const double step = (modifiers & Qt::ShiftModifier) ? 10.0 : 1.0;
    switch (key) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (state() != InteractionState::Preview) return false;
            accept();
            return true;
        case Qt::Key_Escape: return escape();
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            if (session_.selection().empty()) return false;
            if (state() != InteractionState::Idle && state() != InteractionState::Preview) return false;
            deleteSelected();
            return true;
        case Qt::Key_Left: return nudge(-step, 0);
        case Qt::Key_Right: return nudge(step, 0);
        case Qt::Key_Up: return nudge(0, -step);
        case Qt::Key_Down: return nudge(0, step);
        case Qt::Key_E:
            if (session_.selection().size() != 1 || state() != InteractionState::Idle) return false;
            onOpenTypeEditor(session_.selection().front(), QPointF());
            return true;
        default: return false;
    }
}

bool AutoRoiPresenter::nudge(double dx, double dy) {
    if (session_.selection().empty()) return false;
    if (state() != InteractionState::Idle && state() != InteractionState::Preview) return false;
    // 連續的方向鍵併成一步（ADR-002 規則 7 的鍵盤版）；中間停頓夠久、或做過別的事，就是新的一步。
    const bool merge = nudgeClock_.isValid() && nudgeClock_.elapsed() < settings_.nudgeMergeMs;
    nudgeClock_.restart();
    if (session_.moveSelection(Point2D{dx, dy}, merge) > 0) {
        publishScene();
        publishSelection();
        publishHistory();
    }
    return true;
}

bool AutoRoiPresenter::escape() {
    switch (state()) {
        case InteractionState::Searching:
            cancelSearch();
            return true;
        case InteractionState::Preview:
            if (!session_.selection().empty()) {
                select({});
                return true;
            }
            discard();
            return true;
        case InteractionState::NoMatch:
        case InteractionState::Error:
            dismiss();
            return true;
        case InteractionState::Idle:
            if (session_.selection().empty()) return false;
            select({});
            return true;
        default:
            return false;  // 畫框中的 Esc 歸工具
    }
}

// ---- IProposalController -------------------------------------------------------------------------------

void AutoRoiPresenter::accept() {
    nudgeClock_.invalidate();
    const std::optional<std::size_t> count = session_.accept();
    if (!count) return;
    if (*count == 0) {
        views_.status.showToast(uiText("ui.preview.nothingToAccept"));
        return;
    }
    publishAll();
    views_.status.showToast(uiText("ui.preview.accepted", {{"count", QString::number(*count)}}));
}

void AutoRoiPresenter::discard() {
    nudgeClock_.invalidate();
    if (!session_.discard()) return;
    publishAll();
    views_.status.showToast(uiText("ui.preview.discarded"));
}

void AutoRoiPresenter::cancelSearch() {
    if (state() != InteractionState::Searching) return;
    // UI 不等 worker：世代換掉、取消旗標升起，狀態立刻回到 Idle（I4）。worker 之後回來的結果會因世代不符被丟掉。
    invalidateJobs();
    session_.cancelSearch();
    publishAll();
    views_.status.showToast(uiText("ui.search.cancelled"));
}

// ---- IMessageController --------------------------------------------------------------------------------

void AutoRoiPresenter::retry() {
    if (!session_.retrySearch()) return;
    if (session_.lastSearch()->mode == RegionMode::Region) finishRegionScan();
    else startSearch();
}

void AutoRoiPresenter::dismiss() {
    if (!session_.dismiss()) return;
    publishAll();
}

void AutoRoiPresenter::applyAdvice(int actionIndex) {
    if (actionIndex < 0 || actionIndex >= static_cast<int>(card_.actions.size())) return;
    runCardAction(card_.actions[static_cast<std::size_t>(actionIndex)]);
}

void AutoRoiPresenter::runCardAction(CardAction action) {
    switch (action) {
        case CardAction::Accept: accept(); break;
        case CardAction::Discard: discard(); break;
        case CardAction::Dismiss: dismiss(); break;
        case CardAction::Retry: retry(); break;
        case CardAction::Redraw:
        case CardAction::TeachNew:
            dismiss();
            emit toolRequested(ToolId::Sample);
            break;
        case CardAction::LowerThreshold:
            applyThreshold(card_.suggestedThreshold);  // 不重新搜尋：只是對快取的候選重新過濾
            break;
        case CardAction::WidenScope:
            applyScanScope(2);
            break;
    }
}

// ---- IHistoryController --------------------------------------------------------------------------------

void AutoRoiPresenter::undo() {
    nudgeClock_.invalidate();
    if (!session_.undo()) return;
    views_.params.setParams(currentParams());  // 復原到 Preview 時，門檻也回到當時的值
    publishAll();
}

void AutoRoiPresenter::redo() {
    nudgeClock_.invalidate();
    if (!session_.redo()) return;
    publishAll();
}

// ---- IParamsSink ---------------------------------------------------------------------------------------

void AutoRoiPresenter::applyThreshold(double threshold) {
    ScopedTimer timer(services_.probe, "ui.threshold");
    {
        ScopedTimer t(services_.probe, "ui.threshold.refilter");
        session_.setThreshold(threshold);
    }
    {
        ScopedTimer t(services_.probe, "ui.threshold.params");
        views_.params.setParams(currentParams());
    }
    // 沒有提案的時候（Idle…）門檻只是記下來，下一份提案才用它；畫面上沒有東西依賴它，不必重建場景。
    if (!proposalShown()) return;
    publishAll();
}

void AutoRoiPresenter::applySearchParams(const SearchParams& params) {
    searchParams_ = params;
    views_.params.setParams(currentParams());
    // 需要重新搜尋的參數：Searching、Preview、NoMatch 才有上次的區域可以重搜；其餘狀態只記下數值。
    if (session_.restartSearch()) {
        if (session_.lastSearch()->mode == RegionMode::Region) finishRegionScan();
        else startSearch();
    }
}

void AutoRoiPresenter::applyScanScope(int scopeIndex) {
    scanScope_ = std::clamp(scopeIndex, 0, 2);
    views_.params.setParams(currentParams());
    // 比對範圍只影響流程 B（樣本庫）；流程 B 的比對在 S07，這裡先記下來。
    if (session_.lastSearch() && session_.lastSearch()->mode == RegionMode::Region && session_.restartSearch())
        finishRegionScan();
}

void AutoRoiPresenter::applySignificantBits(int bits) {
    if (!sourceImage_.valid() || !validSignificantBitsSetting(bits)) {
        views_.params.setParams(currentParams());
        return;
    }
    const SignificantBitsDecision decision = decideSignificantBits(sourceImage_.format, sourceImage_.significantBits, bits);
    if (decision.choice == SignificantBitsChoice::BelowData) {
        // 影像裡有值超過這個位元數：設定和資料矛盾，不收（下拉選單回到原來的值）。
        views_.status.showToast(uiText("ui.params.bits.rejected", {{"requested", QString::number(bits)},
                                                                    {"bits", QString::number(image_.significantBits)}}));
        views_.params.setParams(currentParams());
        return;
    }
    session_.document().setSignificantBits(bits);
    applyBitsSetting(bits);
    views_.params.setParams(currentParams());
}

SignificantBitsDecision AutoRoiPresenter::applyBitsSetting(int requested) {
    const SignificantBitsDecision decision = decideSignificantBits(sourceImage_.format, sourceImage_.significantBits, requested);
    if (decision.bits != image_.significantBits) {
        image_ = withSignificantBits(sourceImage_, decision);
        views_.canvas.showImage(image_);  // 顯示依位元數換到 8 bit（ImageItem），所以要重畫；之後的搜尋也用新的值
    }
    return decision;
}

// ---- 搜尋 ----------------------------------------------------------------------------------------------

void AutoRoiPresenter::startSearch() {
    const SearchTarget target = *session_.lastSearch();
    ++generation_;
    const std::uint64_t generation = generation_;
    progress_ = 0.0;

    SampleRequest request;
    request.image = image_;  // 帶著 owner：像素活到工作結束
    request.sample = target.region;
    request.search = searchParams_;

    auto outcome = std::make_shared<JobOutcome>();
    auto bridge = std::make_shared<ProgressBridge>(poster_, this, generation);
    const GenerateObjectsFromSample* useCase = &services_.generate;

    beginBusy();
    // work 與 done 都在 worker 上跑，只捕獲「自己的」資料（請求的複本、結果盒、橋）；不捕獲 this 的可變狀態。
    services_.runner.submit(
        [request, useCase, outcome, bridge](const CancelToken& cancel) {
            outcome->result.emplace(useCase->run(request, cancel, bridge.get()));
        },
        [poster = poster_, this, generation, outcome](const JobReport& report) {
            poster->post([this, generation, report, outcome] { onJobFinished(generation, report, outcome); });
        });
    publishAll();
}

// 流程 B（區域掃描）：樣本庫的儲存（ILibraryStore）與比對器（ILibraryMatcher）要到 S06、S07 才有實作。
// 現在庫裡確實沒有任何樣本，所以結果是「樣本庫是空的」，並建議教一個新樣本。這是真的狀況，不是假裝比對過。
void AutoRoiPresenter::finishRegionScan() {
    session_.searchEmpty(NoMatchInfo{NoMatchReason::LibraryEmpty, 0.0, 0.0, 0});
    publishAll();
    emit searchApplied(generation_);
}

void AutoRoiPresenter::onJobFinished(std::uint64_t generation, const JobReport& report,
                                     const std::shared_ptr<JobOutcome>& outcome) {
    // 過期的結果：已經取消、被新的搜尋取代、或換了影像。JobReport::latest 是在 worker 上算的，這裡用自己的狀態再檢查一次。
    if (generation != generation_ || session_.state() != InteractionState::Searching) return;

    endBusy(false);
    if (report.status == JobStatus::Failed) {
        session_.searchFailed(makeError(ErrorCode::Internal, report.failure));
    } else if (!outcome->result) {
        return;  // 工作在開始前就被取消了；世代相符卻沒有結果不該發生，保守地忽略
    } else if (!*outcome->result) {
        const Error& error = outcome->result->error();
        if (error.code == ErrorCode::Cancelled) return;
        session_.searchFailed(error);
    } else {
        SampleProposal& proposal = outcome->result->value();
        lastMs_ = proposal.timings.totalMs;
        session_.searchSucceeded(std::make_shared<const SampleProposal>(std::move(proposal)));
    }
    publishAll();
    emit searchApplied(generation);
}

void AutoRoiPresenter::onProgress(std::uint64_t generation, double fraction) {
    if (generation != generation_ || session_.state() != InteractionState::Searching) return;
    progress_ = std::max(progress_, fraction);
    if (busyLevel_ == BusyLevel::Progress) views_.status.showProgress(progress_);
}

void AutoRoiPresenter::invalidateJobs() {
    ++generation_;
    services_.runner.cancel();
    endBusy(true);
}

// ---- 忙碌與進度（ux-spec 第 9 節）------------------------------------------------------------------------

void AutoRoiPresenter::beginBusy() {
    ++busyEpoch_;
    if (busyLevel_ == BusyLevel::Progress) {
        busyLevel_ = BusyLevel::Busy;
        views_.status.showBusy(BusyLevel::Busy);  // 重新搜尋：舊的進度條先收起來
    }
    busyTimer_.start(settings_.busyAfterMs);
    progressTimer_.start(settings_.progressAfterMs);
}

void AutoRoiPresenter::showBusyNow() {
    if (session_.state() != InteractionState::Searching) return;
    if (busyLevel_ == BusyLevel::None) busyShownClock_.start();
    if (busyLevel_ == BusyLevel::Progress) return;
    busyLevel_ = BusyLevel::Busy;
    views_.status.showBusy(BusyLevel::Busy);
    views_.gate.setBusyCursor(true);
}

void AutoRoiPresenter::showProgressNow() {
    if (session_.state() != InteractionState::Searching) return;
    if (busyLevel_ == BusyLevel::None) {
        busyShownClock_.start();
        views_.gate.setBusyCursor(true);
    }
    busyLevel_ = BusyLevel::Progress;
    views_.status.showBusy(BusyLevel::Progress);
    views_.status.showProgress(progress_);
}

void AutoRoiPresenter::endBusy(bool immediate) {
    busyTimer_.stop();
    progressTimer_.stop();
    if (busyLevel_ == BusyLevel::None) return;
    if (busyLevel_ == BusyLevel::Progress) {
        busyLevel_ = BusyLevel::Busy;
        views_.status.showBusy(BusyLevel::Busy);  // 進度條馬上收起來，結果卡片要用這個位置
    }
    // 忙碌標示一旦出現，至少停留 busyMinShowMs，避免一閃而過（只延後這個小標示，不延後結果）。
    const qint64 shown = busyShownClock_.elapsed();
    if (immediate || shown >= settings_.busyMinShowMs) {
        hideBusyLabelNow();
        return;
    }
    const std::uint64_t epoch = busyEpoch_;
    QTimer::singleShot(static_cast<int>(settings_.busyMinShowMs - shown), this, [this, epoch] {
        if (epoch == busyEpoch_) hideBusyLabelNow();
    });
}

void AutoRoiPresenter::hideBusyLabelNow() {
    busyLevel_ = BusyLevel::None;
    views_.status.showBusy(BusyLevel::None);
    views_.gate.setBusyCursor(false);
}

// ---- 規則的結果 → 畫面 -----------------------------------------------------------------------------------

std::vector<int> AutoRoiPresenter::toSessionIds(const std::vector<int>& sceneIds) const {
    std::vector<int> out;
    const InteractionState s = session_.state();
    const bool previewing = proposalShown();
    // 只有 Idle（文件）與預覽（提案）能選取；其他狀態畫布是暫停的。預覽時文件裡的元件只是背景，不能選。
    if (s != InteractionState::Idle && !(previewing && s == InteractionState::Preview)) return out;
    for (int id : sceneIds) {
        if (previewing && isProposalId(id)) out.push_back(id - kProposalIdBase);
        else if (!previewing && !isProposalId(id)) out.push_back(id);
    }
    return out;
}

std::vector<int> AutoRoiPresenter::toSceneIds(const std::vector<int>& sessionIds) const {
    if (!proposalShown()) return sessionIds;
    std::vector<int> out;
    out.reserve(sessionIds.size());
    for (int id : sessionIds) out.push_back(id + kProposalIdBase);
    return out;
}

void AutoRoiPresenter::rebuildLabels() {
    labels_.sample = uiText("state.sample");
    labels_.inferred = uiText("state.inferred");
    labels_.manual = uiText("origin.manual");
    labels_.board = uiText("ui.objects.board", {{"name", imageName_}});
}

ParamsState AutoRoiPresenter::currentParams() const {
    ParamsState p;
    p.threshold = session_.threshold();
    p.angleMinDeg = searchParams_.angleMinDeg;
    p.angleMaxDeg = searchParams_.angleMaxDeg;
    p.scopeIndex = scanScope_;
    p.significantBits = session_.document().board().significantBits;
    p.detectedBits = sourceImage_.format == PixelFormat::Gray16 ? sourceImage_.significantBits : 0;
    p.bitsApplicable = sourceImage_.valid() && sourceImage_.format == PixelFormat::Gray16;
    return p;
}

QString AutoRoiPresenter::hintText() const {
    switch (session_.state()) {
        case InteractionState::Idle:
        case InteractionState::Searching: return uiText("ui.idle.hint");
        case InteractionState::DrawingRegion: return uiText(drawMode_ == RegionMode::Sample ? "ui.sample.hint" : "ui.scan.hint");
        default: return QString();  // 空白沒有提示；預覽、無匹配、錯誤的提示在卡片裡
    }
}

QString AutoRoiPresenter::summaryText() const {
    const QString backend = services_.backendName;
    if (scene_.counts.components == 0) return uiText("ui.status.none", {{"backend", backend}});
    const QString ms = lastMs_ >= 0.0 ? QString::number(qRound(lastMs_)) : QStringLiteral("—");
    return uiText("ui.status.summary", {{"backend", backend},
                                         {"ms", ms},
                                         {"found", QString::number(scene_.counts.found)},
                                         {"inferred", QString::number(scene_.counts.inferred)}}) +
           QStringLiteral(" · ") + uiText("ui.status.boxes", {{"boxes", QString::number(scene_.counts.boxes)}});
}

// 各步驟都有 PerfProbe 計時（預設關閉，關閉時不讀時鐘）：S04 的 perf.md 用它拆解 U3 的時間花在哪。
void AutoRoiPresenter::publishScene() {
    ScopedTimer timer(services_.probe, "ui.publishScene");
    {
        ScopedTimer t(services_.probe, "ui.scene.build");
        scene_ = buildScene(session_, labels_);
    }
    {
        ScopedTimer t(services_.probe, "ui.scene.canvas");
        views_.canvas.showComponents(scene_.snapshot);
    }
    {
        ScopedTimer t(services_.probe, "ui.scene.tree");
        views_.tree.setRows(scene_.rows);
    }
    views_.status.showSummary(summaryText());
}

void AutoRoiPresenter::publishSelection() {
    ScopedTimer timer(services_.probe, "ui.selection");
    const std::vector<int> ids = toSceneIds(session_.selection());
    views_.canvas.setSelection(ids);
    views_.tree.setSelection(ids);
}

void AutoRoiPresenter::publishDraft() {
    ScopedTimer timer(services_.probe, "ui.draft");
    const InteractionState s = session_.state();
    if (s == InteractionState::DrawingRegion) return;  // 拖曳中的框歸工具
    const bool show = s == InteractionState::Searching || s == InteractionState::Preview || s == InteractionState::NoMatch ||
                      (s == InteractionState::Error && session_.errorSource() == ErrorSource::Search);
    if (!show || !session_.lastSearch()) {
        views_.canvas.showRegionDraft(std::nullopt, DraftMode::Sample);
        return;
    }
    const SearchTarget& t = *session_.lastSearch();
    views_.canvas.showRegionDraft(QRectF(t.region.x, t.region.y, t.region.w, t.region.h), toDraftMode(t.mode));
}

void AutoRoiPresenter::publishMessage() {
    switch (session_.state()) {
        case InteractionState::Preview:
            card_ = previewCard(scene_.counts);
            break;
        case InteractionState::NoMatch:
            if (!session_.noMatch()) {
                cardVisible_ = false;
                views_.message.hideCard();
                return;
            }
            card_ = noMatchCard(*session_.noMatch());
            break;
        case InteractionState::Error:
            if (!session_.error()) {
                cardVisible_ = false;
                views_.message.hideCard();
                return;
            }
            card_ = errorCard(*session_.error(), session_.errorSource(), imageName_, services_.fallbackBackendName);
            break;
        default:
            if (cardVisible_) views_.message.hideCard();
            cardVisible_ = false;
            card_ = CardSpec{};
            return;
    }
    cardVisible_ = true;
    views_.message.showCard(card_.card);
}

void AutoRoiPresenter::publishStatus() {
    views_.status.showSummary(summaryText());
    views_.status.setHint(hintText());
}

void AutoRoiPresenter::publishHistory() {
    const std::optional<CommandInfo> undoInfo = session_.undoInfo();
    const std::optional<CommandInfo> redoInfo = session_.redoInfo();
    views_.history.setUndo(undoInfo.has_value(), undoInfo ? historyLabel(undoInfo->kind, undoInfo->count) : QString());
    views_.history.setRedo(redoInfo.has_value(), redoInfo ? historyLabel(redoInfo->kind, redoInfo->count) : QString());
}

void AutoRoiPresenter::publishGate() {
    const InteractionState s = session_.state();
    const bool enabled = s == InteractionState::Idle || s == InteractionState::Preview || s == InteractionState::NoMatch ||
                         s == InteractionState::DrawingRegion;
    views_.gate.setToolsEnabled(enabled);
    views_.params.setEnabled(s != InteractionState::Empty);
}

void AutoRoiPresenter::notifyStateIfChanged() {
    if (session_.state() == lastNotified_) return;
    lastNotified_ = session_.state();
    emit stateChanged(lastNotified_);
}

// 狀態變了，但場景（框與物件樹）沒變：按下開始拖曳、放開太小、拖曳中按 Esc。
void AutoRoiPresenter::publishChrome() {
    ScopedTimer timer(services_.probe, "ui.chrome");
    publishMessage();
    publishStatus();
    publishHistory();
    publishGate();
    notifyStateIfChanged();
}

void AutoRoiPresenter::publishAll() {
    ScopedTimer timer(services_.probe, "ui.publishAll");
    publishScene();
    publishSelection();
    publishDraft();
    publishChrome();
}

void AutoRoiPresenter::refreshTexts() {
    rebuildLabels();
    publishAll();
}

bool AutoRoiPresenter::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::LanguageChange && watched == QCoreApplication::instance()) {
        // 在下一輪事件迴圈重發：此時 MainWindow 等 widget 也正在處理同一個 LanguageChange。
        QMetaObject::invokeMethod(this, [this] { refreshTexts(); }, Qt::QueuedConnection);
    }
    return QObject::eventFilter(watched, event);
}

}  // namespace aoi::ui
