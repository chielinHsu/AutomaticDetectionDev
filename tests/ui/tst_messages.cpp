// MessageTexts：預覽、無匹配、錯誤的卡片，以及復原按鈕的說明。每一個用到的文案 key 在兩種語言都存在。
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QtTest>

#include "presentation/guidance/GuideDemo.h"
#include "presentation/i18n/Translations.h"
#include "presentation/presenters/MessageTexts.h"

using namespace aoi;
using namespace aoi::ui;

namespace {

NoMatchInfo info(NoMatchReason reason, double best = 0, double suggested = 0, std::size_t count = 0) {
    return NoMatchInfo{reason, best, suggested, count};
}

bool hasAction(const CardSpec& spec, CardAction a) {
    return std::find(spec.actions.begin(), spec.actions.end(), a) != spec.actions.end();
}

constexpr NoMatchReason kAllReasons[] = {
    NoMatchReason::SampleTooSmall, NoMatchReason::SampleTooLarge,  NoMatchReason::SampleCoversMany,
    NoMatchReason::SampleOffPart,  NoMatchReason::OnlyOne,         NoMatchReason::LowContrast,
    NoMatchReason::ThresholdTooHigh, NoMatchReason::LibraryEmpty,  NoMatchReason::RegionEmpty,
    NoMatchReason::NoLibraryMatch, NoMatchReason::AlreadyOnBoard,  NoMatchReason::ScopeBoardOnly};

}  // namespace

class TstMessages : public QObject {
    Q_OBJECT
private slots:
    void init() { Translations::install(QStringLiteral("zh_TW")); }

    // ---- 預覽 ----
    void thePreviewCardSummarisesAndOffersAcceptAndDiscard() {
        SceneCounts c;
        c.found = 79;
        c.inferred = 12;
        c.rejected = 26;
        const CardSpec spec = previewCard(c);
        QCOMPARE(spec.card.title, QStringLiteral("找到 79 個 · 推算 12 個 · 拒絕 26 個"));
        QVERIFY(spec.card.body.contains(QStringLiteral("Enter")));
        QCOMPARE(spec.card.actions.size(), 2);
        QCOMPARE(spec.card.actions.at(0), QStringLiteral("接受 (Enter)"));
        QCOMPARE(spec.card.actions.at(1), QStringLiteral("捨棄 (Esc)"));
        QCOMPARE(spec.actions, (std::vector<CardAction>{CardAction::Accept, CardAction::Discard}));
        QVERIFY(spec.card.advice.isEmpty());  // 沒有信心偏低的框：不提醒
    }

    void lowConfidenceIsMentionedOnlyWhenThereAreSome() {
        SceneCounts c;
        c.lowConfidence = 12;
        const CardSpec spec = previewCard(c);
        QCOMPARE(spec.card.advice.size(), 1);
        QVERIFY(spec.card.advice.first().contains(QStringLiteral("12")));
    }

    // ADR-026：不在陣列格點上的（單獨的、偏離陣列的）有幾個才提醒。
    void partsOffTheArrayAreMentionedOnlyWhenThereAreSome() {
        SceneCounts c;
        c.offLattice = 3;
        const CardSpec spec = previewCard(c);
        QCOMPARE(spec.card.advice.size(), 1);
        QVERIFY(spec.card.advice.first().contains(QStringLiteral("3")));
        QVERIFY(spec.card.advice.first().contains(QStringLiteral("格點")));
    }

    // ---- 無匹配 ----
    void everyReasonGivesAnAdviceLineAndEndsWithDismiss() {
        for (NoMatchReason reason : kAllReasons) {
            const CardSpec spec = noMatchCard(info(reason, 0.6, 0.55, 3));
            QVERIFY2(!spec.card.advice.isEmpty(), qPrintable(QString::number(static_cast<int>(reason))));
            QVERIFY(spec.actions.back() == CardAction::Dismiss);  // 只用滑鼠也要走得完
            QCOMPARE(static_cast<int>(spec.actions.size()), static_cast<int>(spec.card.actions.size()));
            QCOMPARE(spec.card.title, QStringLiteral("沒有找到相似的元件"));
        }
    }

    void sampleProblemsOfferToRedrawTheSample() {
        for (NoMatchReason reason : {NoMatchReason::SampleTooSmall, NoMatchReason::SampleTooLarge, NoMatchReason::SampleCoversMany,
                                     NoMatchReason::SampleOffPart, NoMatchReason::OnlyOne, NoMatchReason::RegionEmpty}) {
            const CardSpec spec = noMatchCard(info(reason));
            QVERIFY(hasAction(spec, CardAction::Redraw));
            QVERIFY(spec.card.actions.contains(QStringLiteral("重畫樣本")));
        }
    }

    void aThresholdThatIsTooHighNamesTheBestScoreAndTheSuggestion() {
        const CardSpec spec = noMatchCard(info(NoMatchReason::ThresholdTooHigh, 0.6234, 0.62));
        QVERIFY(spec.card.advice.first().contains(QStringLiteral("0.62")));
        QVERIFY(hasAction(spec, CardAction::LowerThreshold));
        QCOMPARE(spec.suggestedThreshold, 0.62);
        QVERIFY(spec.card.actions.first().contains(QStringLiteral("降低門檻到 0.62")));
    }

    void libraryProblemsOfferToTeachANewSample() {
        for (NoMatchReason reason : {NoMatchReason::LibraryEmpty, NoMatchReason::NoLibraryMatch}) {
            const CardSpec spec = noMatchCard(info(reason, 0, 0, 4));
            QVERIFY(hasAction(spec, CardAction::TeachNew));
        }
        QVERIFY(noMatchCard(info(NoMatchReason::NoLibraryMatch, 0, 0, 4)).card.advice.first().contains(QStringLiteral("4")));
    }

    void aBoardOnlyScopeOffersToWidenIt() {
        const CardSpec spec = noMatchCard(info(NoMatchReason::ScopeBoardOnly, 0, 0, 2));
        QVERIFY(hasAction(spec, CardAction::WidenScope));
        QVERIFY(spec.card.advice.first().contains(QStringLiteral("2")));
    }

    void lowContrastAndAlreadyOnBoardOnlyOfferDismiss() {
        for (NoMatchReason reason : {NoMatchReason::LowContrast, NoMatchReason::AlreadyOnBoard}) {
            const CardSpec spec = noMatchCard(info(reason));
            QCOMPARE(spec.actions, (std::vector<CardAction>{CardAction::Dismiss}));
        }
    }

    // ---- 錯誤 ----
    void anImageErrorOffersOnlyDismissAndNamesTheFile() {
        const CardSpec spec = errorCard(makeError(ErrorCode::FormatInvalid, "x"), ErrorSource::Image, QStringLiteral("board_07.png"), {});
        QCOMPARE(spec.card.title, QStringLiteral("無法開啟影像"));
        QVERIFY(spec.card.body.contains(QStringLiteral("board_07.png")));
        QCOMPARE(spec.actions, (std::vector<CardAction>{CardAction::Dismiss}));
    }

    void aSearchErrorShowsTheDetailAndOffersRetry() {
        const CardSpec spec =
            errorCard(makeError(ErrorCode::BackendFailed, "template build failed"), ErrorSource::Search, QString(), {});
        QCOMPARE(spec.card.title, QStringLiteral("搜尋失敗"));
        QVERIFY(spec.card.body.contains(QStringLiteral("template build failed")));
        QCOMPARE(spec.actions, (std::vector<CardAction>{CardAction::Dismiss, CardAction::Retry}));
    }

    // 2026-10-09：標題是 back end 的名稱（S08 誤把錯誤訊息放進標題），原因放在做法清單，多一個〔改用 OpenCV〕（ux-spec 第 10 節）。
    void anUnavailableBackendNamesItAndOffersTheFallback() {
        const std::string reason = "HALCON has no valid license while searching. Error 2300.";
        const CardSpec spec = errorCard(makeError(ErrorCode::BackendUnavailable, reason), ErrorSource::Search, QString(),
                                        BackendLabels{QStringLiteral("HALCON"), QStringLiteral("opencv"), QStringLiteral("OpenCV")});
        QCOMPARE(spec.card.title, QStringLiteral("HALCON 無法使用"));
        QVERIFY(!spec.card.title.contains(QStringLiteral("license")));
        QVERIFY(spec.card.body.contains(QStringLiteral("OpenCV")));
        QCOMPARE(spec.card.advice, QStringList{QString::fromStdString(reason)});
        QCOMPARE(spec.actions, (std::vector<CardAction>{CardAction::Dismiss, CardAction::Retry, CardAction::SwitchBackend}));
        QCOMPARE(spec.card.actions.last(), QStringLiteral("改用 OpenCV"));
        QCOMPARE(spec.switchBackendId, QStringLiteral("opencv"));
        // 沒有別的可用的 back end：不建議、沒有〔改用 …〕
        const CardSpec alone = errorCard(makeError(ErrorCode::BackendUnavailable, reason), ErrorSource::Search, QString(),
                                         BackendLabels{QStringLiteral("HALCON"), QString(), QString()});
        QVERIFY(alone.card.body.isEmpty());
        QCOMPARE(alone.actions, (std::vector<CardAction>{CardAction::Dismiss, CardAction::Retry}));
    }

    // ---- 復原按鈕 ----
    void historyLabelsNameTheCommandAndTheCount() {
        QCOMPARE(historyLabel(CommandKind::Accept, 4), QStringLiteral("接受 4 個元件"));
        QCOMPARE(historyLabel(CommandKind::DeleteComponents, 12), QStringLiteral("刪除 12 個元件"));
        QCOMPARE(historyLabel(CommandKind::MoveComponents, 2), QStringLiteral("移動 2 個元件"));
        QCOMPARE(historyLabel(CommandKind::Discard, 0), QStringLiteral("捨棄提案"));
        QCOMPARE(historyLabel(CommandKind::AddComponent, 1), QStringLiteral("補一個元件"));
        QCOMPARE(historyLabel(CommandKind::ResizeTypes, 3), QStringLiteral("把 3 個類型的框設成同樣大小"));
    }

    // ---- 文案齊全 ----
    void everyTextIsTranslatedInBothLanguages() {
        for (const QString& locale : {QStringLiteral("zh_TW"), QStringLiteral("en")}) {
            QVERIFY(Translations::install(locale));
            auto check = [&](const QString& s) {
                QVERIFY2(!s.isEmpty(), qPrintable(locale));
                QVERIFY2(!s.contains(QStringLiteral("{")) && !s.contains(QStringLiteral("ui.")) && !s.contains(QStringLiteral("action.")),
                         qPrintable(locale + ": " + s));  // 沒換掉的佔位字，或找不到 key 時 Qt 回傳的 key 本身
            };
            SceneCounts c;
            c.found = 1;
            c.lowConfidence = 2;
            for (const CardSpec& spec : {previewCard(c), errorCard(makeError(ErrorCode::BackendFailed, "x"), ErrorSource::Search, QStringLiteral("a.png"), {}),
                                         errorCard(makeError(ErrorCode::BackendUnavailable, "HALCON lost its license"), ErrorSource::Search, QString(),
                                                   BackendLabels{QStringLiteral("HALCON"), QStringLiteral("opencv"), QStringLiteral("OpenCV")}),
                                         errorCard(makeError(ErrorCode::FormatInvalid, "x"), ErrorSource::Image, QStringLiteral("a.png"), {})}) {
                check(spec.card.title);
                check(spec.card.body);
                for (const QString& a : spec.card.advice) check(a);
                for (const QString& a : spec.card.actions) check(a);
            }
            for (NoMatchReason reason : kAllReasons) {
                const CardSpec spec = noMatchCard(info(reason, 0.5, 0.45, 3));
                check(spec.card.title);
                check(spec.card.body);
                for (const QString& a : spec.card.advice) check(a);
                for (const QString& a : spec.card.actions) check(a);
            }
            for (CommandKind k : {CommandKind::Accept, CommandKind::Discard, CommandKind::DeleteComponents, CommandKind::AddComponent,
                                  CommandKind::MoveComponents, CommandKind::AddSample, CommandKind::ResizeType, CommandKind::AddBoxGroup,
                                  CommandKind::ResizeTypes})
                check(historyLabel(k, 3));
        }
    }

    void theNewCopyKeysExistInBothLanguagesInTheSourceFile() {
        QFile file(QStringLiteral(":/aoi/ux/copy.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject keys = QJsonDocument::fromJson(file.readAll()).object().value("keys").toObject();
        for (const char* key : {"ui.history.accept", "ui.history.discard", "ui.history.delete", "ui.history.add", "ui.history.move",
                                "ui.history.undo", "ui.history.redo", "ui.preview.nothingToAccept", "ui.editor.unavailable",
                                "ui.objects.col.name", "ui.objects.col.state", "ui.objects.col.score",
                                // session B（2026-10-08／09）：back end 的提醒、切換、錯誤卡片的〔改用 …〕、狀態列的摘要
                                "ui.backend.halcon.runtimeLicense", "ui.backend.notice.title", "ui.backend.status.tip",
                                "ui.backend.switching", "ui.backend.reverted", "ui.backend.switchTo", "ui.status.counts",
                                "ui.status.noObjects",
                                // 2026-10-14（ADR-025）：樣本的建立與搜尋分開、改大小、子框、框選
                                "ui.preview.merged", "ui.preview.excluded", "ui.sample.added", "ui.sample.hintIdle", "ui.search.noSamples",
                                "ui.box.hint", "ui.box.needOne", "ui.box.added", "ui.resize.applied", "ui.history.addSample",
                                "ui.history.resizeType", "ui.history.addBoxGroup", "action.search", "action.selectAll",
                                "action.selectSameType", "tool.box",
                                // 2026-10-14：選取資訊表單與樣本庫 dock 的樣本清單
                                "ui.selection.title", "ui.selection.none", "ui.selection.multi", "ui.selection.summary",
                                "ui.selection.center", "ui.selection.size", "ui.selection.um", "ui.selection.angle",
                                "ui.selection.boxes", "ui.selection.lockRatio", "ui.selection.applyToAll",
                                "ui.selection.applyToAll.tip", "ui.selection.previewOnlyMove", "ui.resize.appliedAll",
                                "ui.resize.nothingToApply", "ui.history.resizeTypes", "ui.library.boardSamples",
                                "ui.library.sample.summary", "ui.library.sample.summaryUm", "ui.library.sample.tip",
                                // 2026-10-15（ADR-027）：操作步驟的面板、說明選單、搜尋中的提示、空白卡片的三步
                                "menu.help", "action.guide", "ui.guide.title", "ui.guide.intro", "ui.guide.busy", "ui.guide.attention",
                                "ui.guide.done", "ui.guide.demo.title", "ui.guide.demo.frame", "ui.guide.demo.missing", "ui.search.hint",
                                "ui.empty.steps",
                                // 2026-10-15（ADR-026）：吸附到陣列、不在格點上的提醒、選取資訊的「位置」
                                "ui.preview.offLattice", "ui.selection.position", "ui.selection.position.lattice",
                                "ui.selection.position.member", "ui.selection.position.matched", "ui.selection.position.moved",
                                "ui.selection.position.noLattice", "ui.selection.position.board", "ui.params.position",
                                "ui.params.position.lattice", "ui.params.position.matched", "ui.params.position.tip"}) {
            const QJsonObject entry = keys.value(QLatin1String(key)).toObject();
            QVERIFY2(!entry.value("zh_TW").toString().isEmpty() && !entry.value("en").toString().isEmpty(), key);
        }
        // 暫代用的文案已經移除
        QVERIFY(!keys.contains("ui.shell.regionDrawn"));
        QVERIFY(!keys.contains("ui.shell.deleteRequested"));
        // 狀態列的摘要不再帶 back end 名稱（它在最左邊的標示，2026-10-08）：舊的兩個 key 已移除（2026-10-09）
        QVERIFY(!keys.contains("ui.status.summary"));
        QVERIFY(!keys.contains("ui.status.none"));
    }

    // 操作步驟的示範（ADR-027）：每一步的標題、說明、每一張畫面的字幕在兩種語言都有；每一張畫面都在資源裡
    // （tests/ui/record_guidance 產生、aoi_ux.qrc 內嵌）。張數的唯一來源是 GuideDemo.h 的 guideDemoFrameCount。
    void everyGuideStepHasTextsAndEveryDemoFrameIsEmbedded() {
        QFile file(QStringLiteral(":/aoi/ux/copy.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject keys = QJsonDocument::fromJson(file.readAll()).object().value("keys").toObject();
        auto both = [&](const QByteArray& key) {
            const QJsonObject entry = keys.value(QLatin1String(key)).toObject();
            QVERIFY2(!entry.value("zh_TW").toString().isEmpty() && !entry.value("en").toString().isEmpty(), key.constData());
        };
        for (int i = 0; i < kGuideStepCount; ++i) {
            const GuideStep step = guideStepAt(i);
            both(guideStepTitleKey(step));
            both(guideStepBodyKey(step));
            QVERIFY(guideDemoFrameCount(step) >= 1);
            for (int n = 1; n <= guideDemoFrameCount(step); ++n) {
                both(guideDemoCaptionKey(step, n));
                QVERIFY2(QFile::exists(guideDemoFramePath(step, n)), qPrintable(guideDemoFramePath(step, n)));
            }
            // 沒有多出來的字幕（張數改了要三邊一起改）
            QVERIFY2(!keys.contains(QLatin1String(guideDemoCaptionKey(step, guideDemoFrameCount(step) + 1))), guideStepKey(step));
        }
    }
};

QTEST_MAIN(TstMessages)
#include "tst_messages.moc"
