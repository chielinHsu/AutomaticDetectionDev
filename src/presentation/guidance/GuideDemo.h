#pragma once

#include <QByteArray>
#include <QString>

#include "application/interaction/StepGuide.h"

// 操作步驟面板的示範畫面（2026-10-15，ADR-027）：每一步幾張關鍵畫面（PNG，逐格播放，不是影片）。
// 畫面由 tests/ui/record_guidance 從合成板子（--demo 的 FakeScene）走一遍流程產生，存在 docs/ux/guidance/，
// 由 aoi_ux.qrc 內嵌（:/aoi/ux/guidance/<步驟>-<n>.png）。張數、檔名與 copy.json 的字幕（ui.guide.demo.<步驟>.<n>）三邊一致，
// tst_messages 檢查每一張都在資源裡、每一句字幕都有。
namespace aoi::ui {

// copy.json 與檔名用的步驟名。
constexpr const char* guideStepKey(GuideStep s) noexcept {
    switch (s) {
        case GuideStep::OpenImage: return "open";
        case GuideStep::DrawSample: return "sample";
        case GuideStep::Search: return "search";
        case GuideStep::Accept: return "accept";
        case GuideStep::Refine: return "refine";
    }
    return "";
}

// 這一步的快捷鍵（步驟列右上角的標示；沒有的是空字串）。
constexpr const char* guideStepShortcut(GuideStep s) noexcept {
    switch (s) {
        case GuideStep::OpenImage: return "Ctrl+O";
        case GuideStep::DrawSample: return "S";
        case GuideStep::Search: return "F5";
        case GuideStep::Accept: return "Enter";
        case GuideStep::Refine: return "B";
    }
    return "";
}

constexpr int guideDemoFrameCount(GuideStep s) noexcept {
    switch (s) {
        case GuideStep::OpenImage: return 2;
        case GuideStep::DrawSample: return 3;
        case GuideStep::Search: return 3;
        case GuideStep::Accept: return 2;
        case GuideStep::Refine: return 3;
    }
    return 0;
}

constexpr GuideStep guideStepAt(int index) noexcept { return static_cast<GuideStep>(index); }
constexpr int guideStepIndex(GuideStep s) noexcept { return static_cast<int>(s); }

// 第 n 張（從 1 起）的資源路徑。
inline QString guideDemoFramePath(GuideStep s, int n) {
    return QStringLiteral(":/aoi/ux/guidance/%1-%2.png").arg(QLatin1String(guideStepKey(s))).arg(n);
}

// 第 n 張的字幕、步驟的標題與說明的文案 key。
inline QByteArray guideDemoCaptionKey(GuideStep s, int n) {
    return QByteArrayLiteral("ui.guide.demo.") + guideStepKey(s) + QByteArrayLiteral(".") + QByteArray::number(n);
}
inline QByteArray guideStepTitleKey(GuideStep s) { return QByteArrayLiteral("ui.guide.step.") + guideStepKey(s) + QByteArrayLiteral(".title"); }
inline QByteArray guideStepBodyKey(GuideStep s) { return QByteArrayLiteral("ui.guide.step.") + guideStepKey(s) + QByteArrayLiteral(".body"); }

}  // namespace aoi::ui
