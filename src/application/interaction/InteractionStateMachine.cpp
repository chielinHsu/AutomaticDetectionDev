#include "application/interaction/InteractionStateMachine.h"

namespace aoi {

std::optional<InteractionState> InteractionStateMachine::peek(InteractionEvent event, TransitionContext ctx) const noexcept {
    using S = InteractionState;
    using E = InteractionEvent;
    switch (state_) {
        case S::Empty:
            switch (event) {
                case E::ImageOpened: return S::Idle;
                case E::ImageFailed: return S::Error;
                default: return std::nullopt;
            }
        case S::Idle:
            switch (event) {
                case E::PressRegion: return S::DrawingRegion;
                case E::EditType: return S::EditingType;
                case E::UndoToPreview: return S::Preview;
                case E::ImageOpened: return S::Idle;
                case E::ImageFailed: return S::Error;
                default: return std::nullopt;
            }
        case S::DrawingRegion:
            switch (event) {
                case E::Release: return S::Searching;
                case E::ReleaseTooSmall:
                case E::Esc: return before_;
                default: return std::nullopt;
            }
        case S::Searching:
            switch (event) {
                case E::ResultFound: return S::Preview;
                case E::ResultEmpty: return S::NoMatch;
                case E::Failure: return S::Error;
                case E::Cancel: return S::Idle;
                case E::ParamsChanged: return S::Searching;  // 重新搜尋，最新的優先
                case E::ImageOpened: return S::Idle;
                default: return std::nullopt;
            }
        case S::Preview:
            switch (event) {
                case E::ThresholdChanged: return S::Preview;
                case E::ParamsChanged: return S::Searching;
                case E::Accept:
                case E::Discard: return S::Idle;
                case E::PressRegion: return S::DrawingRegion;
                case E::ImageOpened: return S::Idle;
                case E::ImageFailed: return S::Error;
                default: return std::nullopt;
            }
        case S::NoMatch:
            switch (event) {
                case E::ThresholdChanged: return ctx.foundAny ? S::Preview : S::NoMatch;
                case E::ParamsChanged: return S::Searching;
                case E::Dismiss: return S::Idle;
                case E::PressRegion: return S::DrawingRegion;
                case E::ImageOpened: return S::Idle;
                case E::ImageFailed: return S::Error;
                default: return std::nullopt;
            }
        case S::Error:
            switch (event) {
                case E::Dismiss: return ctx.hasImage ? S::Idle : S::Empty;
                case E::Retry: return S::Searching;
                case E::ImageOpened: return S::Idle;
                default: return std::nullopt;
            }
        case S::EditingType:
            switch (event) {
                case E::Save:
                case E::Cancel: return S::Idle;
                default: return std::nullopt;
            }
    }
    return std::nullopt;
}

bool InteractionStateMachine::fire(InteractionEvent event, TransitionContext ctx) noexcept {
    const std::optional<InteractionState> next = peek(event, ctx);
    if (!next) return false;
    if (event == InteractionEvent::PressRegion) before_ = state_;  // 要在換狀態之前記
    state_ = *next;
    return true;
}

}  // namespace aoi
