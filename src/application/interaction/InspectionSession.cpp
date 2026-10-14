#include "application/interaction/InspectionSession.h"

#include <algorithm>

#include "domain/box_expander.h"

namespace aoi {

InspectionSession::InspectionSession() : InspectionSession(Config{}) {}

InspectionSession::InspectionSession(Config config) : config_(config), threshold_(config.defaultThreshold) {}

// ---- 內部 ----------------------------------------------------------------------------------------------

void InspectionSession::clearProposal() {
    proposal_.reset();
    noMatch_.reset();
}

// 回到 Idle（或 Empty）時：提案、錯誤、上次的目標、選取都不再有意義。
void InspectionSession::enterIdleCleanup() {
    clearProposal();
    error_.reset();
    lastSearch_.reset();
    selection_.clear();
}

void InspectionSession::pruneSelection() {
    std::vector<int> kept;
    for (int id : selection_) {
        const bool exists = state() == InteractionState::Preview && proposal_ ? proposal_->contains(id)
                                                                              : document_.find(id) != nullptr;
        if (exists) kept.push_back(id);
    }
    selection_ = std::move(kept);
}

bool InspectionSession::refreshSampleTarget() {
    std::vector<SampleOnBoard> samples = document_.samples();
    if (samples.empty()) return false;
    SearchTarget target;
    target.mode = RegionMode::Sample;
    target.region = samples.front().rect;
    target.samples = std::move(samples);
    lastSearch_ = std::move(target);
    return true;
}

// ---- 影像 ----------------------------------------------------------------------------------------------

void InspectionSession::imageOpened(std::string name, int width, int height, double umPerPixel) {
    // 畫到一半的區域與類型編輯不收 ImageOpened（轉移表）：先放棄它們，再開新影像。
    if (state() == InteractionState::DrawingRegion) machine_.fire(InteractionEvent::Esc);
    if (state() == InteractionState::EditingType) machine_.fire(InteractionEvent::Cancel);
    document_.reset(std::move(name), width, height, umPerPixel);
    enterIdleCleanup();
    hasImage_ = true;
    machine_.fire(InteractionEvent::ImageOpened);
}

bool InspectionSession::imageFailed(Error error) {
    if (!machine_.fire(InteractionEvent::ImageFailed)) return false;
    error_ = std::move(error);
    errorSource_ = ErrorSource::Image;
    return true;
}

// ---- 區域 ----------------------------------------------------------------------------------------------

bool InspectionSession::beginRegion(RegionMode) { return machine_.fire(InteractionEvent::PressRegion); }

InspectionSession::Release InspectionSession::endRegion(const Rect& region, RegionMode mode) {
    if (state() != InteractionState::DrawingRegion) return Release::Ignored;
    if (std::min(region.w, region.h) < config_.minSamplePx) {
        machine_.fire(InteractionEvent::ReleaseTooSmall);
        return Release::TooSmall;
    }
    if (mode == RegionMode::Sample) {
        // 建立樣本（ADR-025）：文件堆疊上的一個 Command，回到 Idle。舊的提案不再有意義（它是用舊的樣本集合搜尋的）。
        machine_.fire(InteractionEvent::ReleaseSample);
        clearProposal();
        error_.reset();
        lastSearch_.reset();
        const int id = document_.addSample(region);
        selection_ = id > 0 ? std::vector<int>{id} : std::vector<int>{};
        return Release::SampleAdded;
    }
    machine_.fire(InteractionEvent::Release);
    // 新的區域是新的請求：舊的提案、選取不再有意義（ADR-002 規則 3：新結果建立新的提案）。
    clearProposal();
    selection_.clear();
    lastSearch_ = SearchTarget{region, mode, {}};
    return Release::Search;
}

bool InspectionSession::abortRegion() { return machine_.fire(InteractionEvent::Esc); }

// ---- 搜尋 ----------------------------------------------------------------------------------------------

bool InspectionSession::startSearch() {
    if (!machine_.peek(InteractionEvent::SearchRequested)) return false;
    if (!refreshSampleTarget()) return false;
    machine_.fire(InteractionEvent::SearchRequested);
    error_.reset();
    selection_.clear();
    return true;
}

bool InspectionSession::searchSucceeded(std::shared_ptr<const BoardProposal> proposal) {
    if (state() != InteractionState::Searching || !lastSearch_ || !proposal) return false;
    ProposalSession session(std::move(proposal), lastSearch_->region, lastSearch_->mode, threshold_, config_.candidateFloor);
    noMatch_ = session.objects().noMatch;
    proposal_ = std::move(session);
    selection_.clear();
    machine_.fire(noMatch_ ? InteractionEvent::ResultEmpty : InteractionEvent::ResultFound);
    return true;
}

bool InspectionSession::searchEmpty(NoMatchInfo info) {
    if (state() != InteractionState::Searching) return false;
    clearProposal();
    selection_.clear();
    noMatch_ = info;
    machine_.fire(InteractionEvent::ResultEmpty);
    return true;
}

bool InspectionSession::searchFailed(Error error) {
    if (state() != InteractionState::Searching) return false;
    machine_.fire(InteractionEvent::Failure);
    error_ = std::move(error);
    errorSource_ = ErrorSource::Search;
    return true;
}

bool InspectionSession::cancelSearch() {
    if (!machine_.fire(InteractionEvent::Cancel)) return false;
    enterIdleCleanup();
    return true;
}

bool InspectionSession::restartSearch() {
    if (!lastSearch_) return false;
    if (!machine_.peek(InteractionEvent::ParamsChanged)) return false;
    if (lastSearch_->mode == RegionMode::Sample && !refreshSampleTarget()) return false;
    return machine_.fire(InteractionEvent::ParamsChanged);
}

bool InspectionSession::retrySearch() {
    if (!lastSearch_ || errorSource_ != ErrorSource::Search) return false;
    if (!machine_.peek(InteractionEvent::Retry)) return false;
    if (lastSearch_->mode == RegionMode::Sample && !refreshSampleTarget()) return false;
    machine_.fire(InteractionEvent::Retry);
    error_.reset();
    return true;
}

bool InspectionSession::dismiss() {
    TransitionContext ctx;
    ctx.hasImage = hasImage_;
    if (!machine_.fire(InteractionEvent::Dismiss, ctx)) return false;
    enterIdleCleanup();
    return true;
}

// ---- 門檻 ----------------------------------------------------------------------------------------------

void InspectionSession::setThreshold(double threshold) {
    threshold_ = std::clamp(threshold, config_.thresholdMin, config_.thresholdMax);
    if (!proposal_) return;
    const InteractionState s = state();
    if (s != InteractionState::Preview && s != InteractionState::NoMatch) return;

    proposal_->setThreshold(threshold_);
    // 門檻變了，之前復原出來的「接受」帶的是舊門檻的提案，不能再重做。
    document_.dropRedo();
    const std::optional<NoMatchInfo> info = proposal_->objects().noMatch;
    TransitionContext ctx;
    ctx.foundAny = !info.has_value();
    machine_.fire(InteractionEvent::ThresholdChanged, ctx);
    // noMatch 只在 NoMatch 狀態有意義（Preview 的門檻拉到沒有任何找到，仍是 Preview：轉移表）。
    noMatch_ = state() == InteractionState::NoMatch ? info : std::nullopt;
    pruneSelection();
}

// ---- 提案 ----------------------------------------------------------------------------------------------

std::optional<std::size_t> InspectionSession::accept() {
    if (!previewAcceptsEdits()) return std::nullopt;
    proposal_->dropRedo();  // ADR-002 規則 4：復原回預覽之後，Ctrl+Y 要重新接受，而不是先重做殘留的舊動作
    const std::size_t count = document_.accept(*proposal_);
    if (count == 0) return std::size_t{0};
    machine_.fire(InteractionEvent::Accept);
    enterIdleCleanup();
    return count;
}

bool InspectionSession::discard() {
    if (!previewAcceptsEdits()) return false;
    proposal_->dropRedo();
    document_.discard(*proposal_);
    machine_.fire(InteractionEvent::Discard);
    enterIdleCleanup();
    return true;
}

// ---- 選取與編輯 ------------------------------------------------------------------------------------------

void InspectionSession::select(const std::vector<int>& ids) {
    selection_ = ids;
    pruneSelection();
}

std::size_t InspectionSession::selectAll() {
    std::vector<int> ids;
    if (state() == InteractionState::Idle) {
        for (const ComponentInstance& c : document_.components()) ids.push_back(c.id);
    } else if (previewAcceptsEdits()) {
        for (const InspectionObject& o : proposal_->objects().objects) ids.push_back(o.id);
    } else {
        return 0;
    }
    selection_ = std::move(ids);
    return selection_.size();
}

std::size_t InspectionSession::selectSameType() {
    if (selection_.empty()) return 0;
    std::vector<int> ids;
    if (state() == InteractionState::Idle) {
        const ComponentInstance* c = document_.find(selection_.front());
        if (c == nullptr) return 0;
        ids = document_.componentsOfType(c->typeId);
    } else if (previewAcceptsEdits()) {
        const std::optional<ProposalSession::TypeAndSize> type = proposal_->typeOf(selection_.front());
        if (!type) return 0;
        for (const InspectionObject& o : proposal_->objects().objects) {
            if (o.typeId == type->typeId) ids.push_back(o.id);
        }
    } else {
        return 0;
    }
    selection_ = std::move(ids);
    return selection_.size();
}

std::size_t InspectionSession::deleteSelection() {
    if (selection_.empty()) return 0;
    std::size_t removed = 0;
    if (state() == InteractionState::Idle) {
        removed = document_.removeComponents(selection_);
    } else if (previewAcceptsEdits()) {
        removed = proposal_->deleteObjects(selection_);
        if (removed > 0) document_.dropRedo();
    }
    if (removed > 0) selection_.clear();
    return removed;
}

InspectionSession::AddResult InspectionSession::addManual(const Pose2D& pose) {
    if (state() == InteractionState::Idle) {
        std::string typeId;
        for (int id : selection_) {
            if (const ComponentInstance* c = document_.find(id)) {
                typeId = c->typeId;
                break;
            }
        }
        if (typeId.empty()) typeId = document_.lastTypeId();
        if (typeId.empty()) return AddResult::NeedType;
        const int id = document_.addManualComponent(pose, typeId);
        if (id == 0) return AddResult::NeedType;
        selection_ = {id};
        return AddResult::Added;
    }
    if (previewAcceptsEdits()) {
        std::optional<ProposalSession::TypeAndSize> type;
        for (int id : selection_) {
            type = proposal_->typeOf(id);
            if (type) break;
        }
        if (!type && !proposal_->proposal().candidates.empty()) {
            const BoardCandidate& first = proposal_->proposal().candidates.front();
            type = ProposalSession::TypeAndSize{first.typeId, first.partSize};
        }
        if (!type) return AddResult::NeedType;
        const int id = proposal_->addManual(pose, type->typeId, type->partSize);
        document_.dropRedo();
        selection_ = {id};
        return AddResult::Added;
    }
    return AddResult::NotAllowed;
}

std::size_t InspectionSession::moveSelection(Point2D delta, bool allowMerge) {
    if (selection_.empty()) return 0;
    if (state() == InteractionState::Idle) return document_.moveComponents(selection_, delta, allowMerge);
    if (previewAcceptsEdits()) {
        const std::size_t moved = proposal_->moveObjects(selection_, delta, allowMerge);
        if (moved > 0) document_.dropRedo();
        return moved;
    }
    return 0;
}

bool InspectionSession::resizeBody(int componentId, const Rect& rectPx) {
    if (state() != InteractionState::Idle) return false;
    if (std::min(rectPx.w, rectPx.h) < config_.minSamplePx) return false;
    const ComponentInstance* c = document_.find(componentId);
    if (c == nullptr) return false;
    const Point2D center = rectPx.center();
    return document_.setBodySize(c->typeId, rectPx.size(), componentId, Pose2D{center.x, center.y, c->pose.angleDeg});
}

bool InspectionSession::addBoxGroup(int componentId, const Rect& rectPx) {
    if (state() != InteractionState::Idle) return false;
    if (rectPx.empty()) return false;
    const ComponentInstance* c = document_.find(componentId);
    if (c == nullptr) return false;
    // 影像像素 → 類型座標（µm、相對主體中心、把元件的旋轉轉回去）。
    const double um = effectiveUmPerPixel(document_.board().umPerPixel);
    const Point2D center = rectPx.center();
    const Point2D local = rotate({center.x - c->pose.x, center.y - c->pose.y}, -c->pose.angleDeg);
    BoxGroup group;
    group.kind = BoxGroupKind::Pin;
    group.widthUm = rectPx.w * um;
    group.heightUm = rectPx.h * um;
    group.centerXUm = local.x * um;
    group.centerYUm = local.y * um;
    group.matrix.rows = 1;
    group.matrix.cols = 1;
    return document_.addBoxGroup(c->typeId, std::move(group));
}

// ---- 歷史 ----------------------------------------------------------------------------------------------

bool InspectionSession::undo() {
    if (previewAcceptsEdits()) {
        // 預覽：只動提案堆疊，不會越過去動文件歷史（多餘的復原忽略）。
        if (!proposal_->undo()) return false;
        pruneSelection();
        return true;
    }
    if (state() != InteractionState::Idle) return false;

    UndoOutcome outcome = document_.undo();
    if (!outcome.performed) return false;
    if (outcome.restoredProposal) {
        proposal_ = std::move(outcome.restoredProposal);
        threshold_ = proposal_->threshold();
        noMatch_ = proposal_->objects().noMatch;
        lastSearch_ = SearchTarget{proposal_->region(), proposal_->mode(), {}};
        machine_.fire(InteractionEvent::UndoToPreview);
        selection_.clear();
    } else {
        pruneSelection();
    }
    return true;
}

bool InspectionSession::redo() {
    if (previewAcceptsEdits()) {
        if (proposal_->canRedo()) {
            proposal_->redo();
            pruneSelection();
            return true;
        }
        // 提案堆疊沒有可重做的：看文件堆疊的下一步是不是「重新接受／捨棄」這份提案。
        const ProposalSession* next = document_.nextRedoProposal();
        if (next == nullptr || next->proposalPtr() != proposal_->proposalPtr()) return false;
        const UndoOutcome outcome = document_.redo();
        if (!outcome.performed) return false;
        machine_.fire(outcome.kind == CommandKind::Accept ? InteractionEvent::Accept : InteractionEvent::Discard);
        enterIdleCleanup();
        return true;
    }
    if (state() != InteractionState::Idle) return false;
    const UndoOutcome outcome = document_.redo();
    if (!outcome.performed) return false;
    pruneSelection();
    return true;
}

std::optional<CommandInfo> InspectionSession::undoInfo() const {
    if (previewAcceptsEdits()) return proposal_->undoInfo();
    if (state() == InteractionState::Idle) return document_.undoInfo();
    return std::nullopt;
}

std::optional<CommandInfo> InspectionSession::redoInfo() const {
    if (previewAcceptsEdits()) {
        if (proposal_->canRedo()) return proposal_->redoInfo();
        const ProposalSession* next = document_.nextRedoProposal();
        if (next != nullptr && next->proposalPtr() == proposal_->proposalPtr()) return document_.redoInfo();
        return std::nullopt;
    }
    if (state() == InteractionState::Idle) return document_.redoInfo();
    return std::nullopt;
}

}  // namespace aoi
