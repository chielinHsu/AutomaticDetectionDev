#include "application/interaction/InspectionSession.h"

#include <algorithm>

namespace aoi {

InspectionSession::InspectionSession() : InspectionSession(Config{}) {}

InspectionSession::InspectionSession(Config config) : config_(config), threshold_(config.defaultThreshold) {}

// ---- 內部 ----------------------------------------------------------------------------------------------

void InspectionSession::clearProposal() {
    proposal_.reset();
    noMatch_.reset();
}

// 回到 Idle（或 Empty）時：提案、錯誤、上次的區域、選取都不再有意義。
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
    machine_.fire(InteractionEvent::Release);
    // 新的區域是新的請求：舊的提案、選取不再有意義（ADR-002 規則 3：新結果建立新的提案）。
    clearProposal();
    selection_.clear();
    lastSearch_ = SearchTarget{region, mode};
    return Release::Search;
}

bool InspectionSession::abortRegion() { return machine_.fire(InteractionEvent::Esc); }

// ---- 搜尋 ----------------------------------------------------------------------------------------------

bool InspectionSession::searchSucceeded(std::shared_ptr<const SampleProposal> proposal) {
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
    return machine_.fire(InteractionEvent::ParamsChanged);
}

bool InspectionSession::retrySearch() {
    if (!lastSearch_ || errorSource_ != ErrorSource::Search) return false;
    if (!machine_.fire(InteractionEvent::Retry)) return false;
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
        const int id = proposal_->addManual(pose);
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
        lastSearch_ = SearchTarget{proposal_->region(), proposal_->mode()};
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
