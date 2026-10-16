// 產生操作步驟面板的示範畫面（2026-10-15，ADR-027；presentation/guidance/GuideDemo.h）。
//
// 不是測試：走 Presenter 的測試台（和產品一樣的接線、合成板子、假 back end），把 S → F5 → Enter → B 的關鍵時刻各抓一張
// 視窗的畫面，縮到 kFrameWidth（400 px）寬，存成 docs/ux/guidance/<步驟>-<n>.png（aoi_ux.qrc 內嵌）。張數要和 guideDemoFrameCount 一致，
// 字幕在 copy.json（ui.guide.demo.<步驟>.<n>）；tst_messages 檢查三邊一致。
//
// 用法（offscreen，test_environment.cpp 設好）：build\qmake-release\bin\record_guidance.exe [輸出資料夾]
// 預設輸出到 repo 的 docs/ux/guidance/。UI 改了版面或文案之後重跑一次，再重新建置（rcc 會把新的 PNG 編進去）。
#include <QDir>
#include <QDockWidget>
#include <QImage>
#include <QPixmap>
#include <cstdio>

#include "PresenterRig.h"
#include "presentation/guidance/GuideDemo.h"
#include "presentation/shell/MessageBar.h"

using namespace aoi;
using namespace aoi::ui;
using namespace aoi::test;

namespace {

// 400 px：面板的示範區約 250 px 寬，再大只是解碼變慢（Debug 的 Qt 解 720 px 的 PNG 要幾百 ms，tst_presenter 曾因此逾時）。
constexpr int kFrameWidth = 400;

struct Recorder {
    Rig& rig;
    QDir out;
    int written = 0;

    bool grab(GuideStep step, int n) {
        QCoreApplication::processEvents();
        const QImage image = rig.window.grab().toImage().scaledToWidth(kFrameWidth, Qt::SmoothTransformation);
        const QString name = QStringLiteral("%1-%2.png").arg(QLatin1String(guideStepKey(step))).arg(n);
        if (!image.save(out.filePath(name))) {
            std::fprintf(stderr, "cannot write %s\n", qPrintable(out.filePath(name)));
            return false;
        }
        ++written;
        return true;
    }
};

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    const QString outPath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral(AOI_SOURCE_DIR "/docs/ux/guidance");
    if (!QDir().mkpath(outPath)) {
        std::fprintf(stderr, "cannot create %s\n", qPrintable(outPath));
        return 2;
    }

    auto world = defaultWorld();
    world.halfCols = 8;
    world.halfRows = 4;
    Rig r(world);
    r.window.resize(1180, 760);
    r.window.guidanceDock()->hide();  // 示範畫面裡不要有面板自己
    QCoreApplication::processEvents();
    Recorder rec{r, QDir(outPath)};
    const int toastMs = ThemeTokens::builtin().timing.toast + 300;

    // ① 開啟影像：空白卡片 → 影像開好了（Idle 的提示）
    if (!rec.grab(GuideStep::OpenImage, 1)) return 1;
    r.open(1200, 800);
    if (!rec.grab(GuideStep::OpenImage, 2)) return 1;

    // ② 框一個樣本：按 S → 拖到一半 → 放開（吐司「已建立樣本」）
    const QRectF sample(560, 360, 60, 32);
    r.window.toolAction(ToolId::Sample)->trigger();
    if (!rec.grab(GuideStep::DrawSample, 1)) return 1;
    press(r.vp(), r.at(sample.left(), sample.top()));
    move(r.vp(), r.at(sample.left() + 40, sample.top() + 22));
    if (!rec.grab(GuideStep::DrawSample, 2)) return 1;
    move(r.vp(), r.at(sample.right(), sample.bottom()));
    release(r.vp(), r.at(sample.right(), sample.bottom()));
    if (!r.waitState(InteractionState::Idle, 2000) || !r.presenter().session().hasSamples()) return 1;
    if (!rec.grab(GuideStep::DrawSample, 3)) return 1;

    // ③ 搜尋：等吐司結束（提示變成「F5 搜尋」）→ 搜尋中（進度列）→ 結果
    QTest::qWait(toastMs);
    if (!rec.grab(GuideStep::Search, 1)) return 1;
    r.probe->gateWindows = true;  // 讓搜尋卡在最後一步，拍得到進度列
    r.window.searchAction()->trigger();
    if (!QTest::qWaitFor([&] { return r.window.messageBar()->progressVisible(); }, 3000)) return 1;
    if (!rec.grab(GuideStep::Search, 2)) return 1;
    r.probe->windowReleased = 1 << 20;
    r.probe->gateWindows = false;
    if (!r.waitState(InteractionState::Preview)) return 1;
    if (!rec.grab(GuideStep::Search, 3)) return 1;

    // ④ 接受：預覽的卡片 → Enter 之後（吐司「已接受」）
    r.clickComponent(5);
    if (!rec.grab(GuideStep::Accept, 1)) return 1;
    r.key(Qt::Key_Return);
    if (!r.waitState(InteractionState::Idle, 2000)) return 1;
    if (!rec.grab(GuideStep::Accept, 2)) return 1;

    // ⑤ 微調：選一個元件（把手）→ B 畫子框 → Ctrl+Shift+A 選同類型
    QTest::qWait(toastMs);
    r.clickComponent(0);
    if (!rec.grab(GuideStep::Refine, 1)) return 1;
    const QPointF c = r.centerOfComponent(0);
    r.window.toolAction(ToolId::Box)->trigger();
    press(r.vp(), r.at(c.x() - 20, c.y() - 10));
    move(r.vp(), r.at(c.x() - 6, c.y() - 2));
    release(r.vp(), r.at(c.x() - 6, c.y() - 2));
    QCoreApplication::processEvents();
    if (!rec.grab(GuideStep::Refine, 2)) return 1;
    QTest::qWait(toastMs);
    r.key(Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);
    if (!rec.grab(GuideStep::Refine, 3)) return 1;

    int expected = 0;
    for (int i = 0; i < kGuideStepCount; ++i) expected += guideDemoFrameCount(guideStepAt(i));
    if (rec.written != expected) {
        std::fprintf(stderr, "wrote %d frames, GuideDemo.h expects %d\n", rec.written, expected);
        return 1;
    }
    std::printf("wrote %d frames to %s\n", rec.written, qPrintable(outPath));
    return 0;
}
