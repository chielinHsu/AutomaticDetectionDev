#include "presentation/presenters/MessageTexts.h"

#include "presentation/i18n/Translations.h"

namespace aoi::ui {

namespace {

QString two(double v) { return QString::number(v, 'f', 2); }

QString withKey(const char* actionKey, const char* shortcut) {
    return uiText(actionKey) + QStringLiteral(" (") + QLatin1String(shortcut) + QLatin1Char(')');
}

}  // namespace

CardSpec previewCard(const SceneCounts& counts) {
    CardSpec spec;
    spec.card.title = uiText("ui.preview.summary", {{"found", QString::number(counts.found)},
                                                     {"inferred", QString::number(counts.inferred)},
                                                     {"rejected", QString::number(counts.rejected)}});
    spec.card.body = uiText("ui.preview.hint");
    if (counts.lowConfidence > 0)
        spec.card.advice << uiText("ui.preview.lowConfidence", {{"count", QString::number(counts.lowConfidence)}});
    // 幾個樣本合併的結果（ADR-025）：同一個位置被不只一個樣本找到的留分數高的；手動補的元件佔住的位置剔除。
    if (counts.merged > 0) spec.card.advice << uiText("ui.preview.merged", {{"count", QString::number(counts.merged)}});
    if (counts.excluded > 0) spec.card.advice << uiText("ui.preview.excluded", {{"count", QString::number(counts.excluded)}});
    if (counts.replaced > 0) spec.card.advice << uiText("ui.preview.replaces", {{"count", QString::number(counts.replaced)}});
    spec.card.actions << withKey("action.accept", "Enter") << withKey("action.discard", "Esc");
    spec.actions = {CardAction::Accept, CardAction::Discard};
    return spec;
}

CardSpec noMatchCard(const NoMatchInfo& info) {
    CardSpec spec;
    spec.card.title = uiText("ui.noMatch.title");
    spec.card.body = uiText("ui.noMatch.body");
    const QString count = QString::number(info.componentCount);
    auto redraw = [&] {
        spec.card.actions << uiText("ui.noMatch.action.redraw");
        spec.actions.push_back(CardAction::Redraw);
    };
    auto teachNew = [&] {
        spec.card.actions << uiText("ui.noMatch.action.teachNew");
        spec.actions.push_back(CardAction::TeachNew);
    };
    switch (info.reason) {
        case NoMatchReason::SampleTooSmall:
            spec.card.advice << uiText("ui.noMatch.advice.sampleTooSmall");
            redraw();
            break;
        case NoMatchReason::SampleTooLarge:
            spec.card.advice << uiText("ui.noMatch.advice.sampleTooLarge");
            redraw();
            break;
        case NoMatchReason::SampleCoversMany:
            spec.card.advice << uiText("ui.noMatch.advice.sampleCoversMany");
            redraw();
            break;
        case NoMatchReason::SampleOffPart:
            spec.card.advice << uiText("ui.noMatch.advice.sampleOffPart");
            redraw();
            break;
        case NoMatchReason::OnlyOne:
            spec.card.advice << uiText("ui.noMatch.advice.onlyOne");
            redraw();
            break;
        case NoMatchReason::LowContrast:
            spec.card.advice << uiText("ui.noMatch.advice.lowContrast");
            break;
        case NoMatchReason::ThresholdTooHigh:
            spec.card.advice << uiText("ui.noMatch.advice.lowerThreshold",
                                       {{"best", two(info.highestScore)}, {"suggested", two(info.suggestedThreshold)}});
            spec.card.actions << uiText("ui.noMatch.action.lowerThreshold", {{"suggested", two(info.suggestedThreshold)}});
            spec.actions.push_back(CardAction::LowerThreshold);
            spec.suggestedThreshold = info.suggestedThreshold;
            break;
        case NoMatchReason::LibraryEmpty:
            spec.card.advice << uiText("ui.noMatch.advice.libraryEmpty");
            teachNew();
            break;
        case NoMatchReason::RegionEmpty:
            spec.card.advice << uiText("ui.noMatch.advice.regionEmpty");
            redraw();
            break;
        case NoMatchReason::NoLibraryMatch:
            spec.card.advice << uiText("ui.noMatch.advice.noLibraryMatch", {{"count", count}});
            teachNew();
            break;
        case NoMatchReason::AlreadyOnBoard:
            spec.card.advice << uiText("ui.noMatch.advice.alreadyOnBoard");
            break;
        case NoMatchReason::ScopeBoardOnly:
            spec.card.advice << uiText("ui.noMatch.advice.scopeBoard", {{"count", count}});
            spec.card.actions << uiText("ui.noMatch.action.widenScope");
            spec.actions.push_back(CardAction::WidenScope);
            break;
    }
    spec.card.actions << uiText("action.dismiss");
    spec.actions.push_back(CardAction::Dismiss);
    return spec;
}

CardSpec errorCard(const Error& error, ErrorSource source, const QString& imageName, const BackendLabels& backend) {
    CardSpec spec;
    if (source == ErrorSource::Image) {
        spec.card.title = uiText("ui.error.imageOpen.title");
        spec.card.body = uiText("ui.error.imageOpen.body", {{"name", imageName}});
        spec.card.actions << uiText("action.dismiss");
        spec.actions = {CardAction::Dismiss};
        return spec;
    }
    if (error.code == ErrorCode::BackendUnavailable) {
        // 例如執行中 HALCON 的授權不見了（HalconErrors）。標題是 back end 的名稱（S08 誤把錯誤訊息放在這裡）；原因（英文，
        // 含 HALCON 的錯誤碼）放在做法清單。
        spec.card.title = uiText("ui.error.backend.title", {{"backend", backend.name}});
        if (!backend.fallbackName.isEmpty()) spec.card.body = uiText("ui.error.backend.body", {{"fallback", backend.fallbackName}});
        spec.card.advice << QString::fromStdString(error.message);
        spec.card.actions << uiText("action.dismiss") << uiText("action.retry");
        spec.actions = {CardAction::Dismiss, CardAction::Retry};
        if (!backend.fallbackId.isEmpty()) {
            spec.card.actions << uiText("ui.backend.switchTo", {{"backend", backend.fallbackName}});
            spec.actions.push_back(CardAction::SwitchBackend);
            spec.switchBackendId = backend.fallbackId;
        }
        return spec;
    } else {
        spec.card.title = uiText("ui.error.searchFailed.title");
        spec.card.body = uiText("ui.error.searchFailed.body", {{"detail", QString::fromStdString(error.message)}});
    }
    spec.card.actions << uiText("action.dismiss") << uiText("action.retry");
    spec.actions = {CardAction::Dismiss, CardAction::Retry};
    return spec;
}

QString historyLabel(CommandKind kind, std::size_t count) {
    const QString n = QString::number(count);
    switch (kind) {
        case CommandKind::Accept: return uiText("ui.history.accept", {{"count", n}});
        case CommandKind::Discard: return uiText("ui.history.discard");
        case CommandKind::DeleteComponents: return uiText("ui.history.delete", {{"count", n}});
        case CommandKind::AddComponent: return uiText("ui.history.add");
        case CommandKind::MoveComponents: return uiText("ui.history.move", {{"count", n}});
        case CommandKind::AddSample: return uiText("ui.history.addSample");
        case CommandKind::ResizeType: return uiText("ui.history.resizeType", {{"count", n}});
        case CommandKind::AddBoxGroup: return uiText("ui.history.addBoxGroup", {{"count", n}});
        case CommandKind::ResizeTypes: return uiText("ui.history.resizeTypes", {{"count", n}});
        case CommandKind::SetMatchRegion: return uiText("ui.history.setMatchRegion", {{"count", n}});
    }
    return QString();
}

}  // namespace aoi::ui
