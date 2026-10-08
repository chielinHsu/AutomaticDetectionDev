#include <QApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <cstdio>

#include "app/CompositionRoot.h"
#include "presentation/canvas/ImageCanvas.h"
#include "presentation/i18n/Translations.h"
#include "presentation/presenters/AppPresenters.h"
#include "presentation/shell/MainWindow.h"
#include "presentation/theme/ThemeManager.h"
#include "presentation/theme/ThemeTokens.h"

#ifdef AOI_DEMO_SCENE
#include "presentation/dev/FakeScene.h"
#endif

namespace {

// 啟動失敗的訊息直接寫到 stderr（結束碼 2）。不用 qCritical：Qt 在 Windows 上只有 stderr 是主控台時才寫到 stderr，
// 被腳本或管線接走時（例如測試）會落到偵錯輸出，使用者與測試都看不到。
void reportStartupError(const QString& message) { std::fprintf(stderr, "%s\n", qPrintable(message)); }

aoi::ui::ThemeMode parseTheme(const QString& name, bool* ok) {
    *ok = true;
    if (name == QLatin1String("light")) return aoi::ui::ThemeMode::Light;
    if (name == QLatin1String("dark")) return aoi::ui::ThemeMode::Dark;
    if (name == QLatin1String("system")) return aoi::ui::ThemeMode::System;
    *ok = false;
    return aoi::ui::ThemeMode::System;
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("AOI"));
    QCoreApplication::setApplicationName(QStringLiteral("AutoCadCreated"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("AOI teach tool"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("image"), QStringLiteral("Image file to open."), QStringLiteral("[image]"));
    const QCommandLineOption langOpt({QStringLiteral("lang")}, QStringLiteral("UI language: zh_TW or en."), QStringLiteral("locale"));
    const QCommandLineOption themeOpt({QStringLiteral("theme")}, QStringLiteral("Theme: light, dark or system."), QStringLiteral("mode"));
    const QCommandLineOption backendOpt({QStringLiteral("backend")},
                                        QStringLiteral("Vision back end: fake, opencv or halcon (default: fake)."),
                                        QStringLiteral("id"), QStringLiteral("fake"));
    parser.addOption(langOpt);
    parser.addOption(themeOpt);
    parser.addOption(backendOpt);
#ifdef AOI_DEMO_SCENE
    const QCommandLineOption demoOpt({QStringLiteral("demo")},
                                     QStringLiteral("Open a synthetic 5 MP board with this many boxes."),
                                     QStringLiteral("boxes"));
    parser.addOption(demoOpt);
#endif
    parser.process(app);

    const QString locale = parser.isSet(langOpt) ? parser.value(langOpt) : aoi::ui::Translations::defaultLocale();
    if (!aoi::ui::Translations::install(locale)) {
        reportStartupError(QStringLiteral("Cannot load the UI text for '%1'.").arg(locale));
        return 2;
    }

    const aoi::ui::ThemeTokens& tokens = aoi::ui::ThemeTokens::builtin();
    aoi::ui::ThemeManager theme(app, tokens);
    if (parser.isSet(themeOpt)) {
        bool ok = false;
        const aoi::ui::ThemeMode mode = parseTheme(parser.value(themeOpt), &ok);
        if (!ok) {
            reportStartupError(QStringLiteral("Unknown theme '%1'.").arg(parser.value(themeOpt)));
            return 2;
        }
        theme.setMode(mode);
    }

    // 核心（back end、執行緒、use case）。S04 起交給 Presenter；組裝在這裡，Presenter 只認識 use case 與 JobRunner。
    // back end 不存在或不可用時明確失敗，不退回別的 back end（使用者要的是哪一個，就是哪一個）。
    const aoi::BackendRegistry registry = aoi::app::makeBackendRegistry();
    aoi::Result<aoi::app::CoreServices> core =
        aoi::app::composeCore(registry, parser.value(backendOpt).toStdString());
    if (!core) {
        reportStartupError(QStringLiteral("Cannot start the '%1' back end: %2")
                               .arg(parser.value(backendOpt), QString::fromStdString(core.error().message)));
        return 2;
    }

    // 影像來源（S06）：視窗經它開圖。要活得比視窗久，所以先建立。
    const std::unique_ptr<aoi::IImageSource> imageSource = aoi::app::makeImageSource();

    // 視窗、Presenter 的建立順序＝解構順序的反面：核心活得最久，然後是視窗，最後才是 Presenter（它解構時先解除視窗的 Controller）。
    aoi::ui::MainWindow window(theme);
    window.setImageSource(imageSource.get());
    aoi::app::CoreServices& services = core.value();
    // back end 的提醒（S08：例如 HALCON 用的是 runtime 授權、SDK 的 dongle 沒插）：stderr 一行；畫面上是訊息列的提醒卡片
    // （按〔關閉〕才收起）與狀態列 back end 名稱後面的標示（BackendPresenter）。
    for (const aoi::BackendNotice& notice : services.backend.notices) {
        std::fprintf(stderr, "%s\n", notice.message.c_str());
        std::fflush(stderr);  // 程式之後會一直跑（視窗）：被管線接走時，不 flush 就要等結束才看得到
    }

    // 錯誤橫幅（搜尋時 back end 無法使用）建議改用的：目前以外、可用的第一個。
    QString fallbackBackend = QStringLiteral("OpenCV");
    for (const aoi::BackendInfo& info : registry.list()) {
        if (info.available && info.id != services.backend.id) {
            fallbackBackend = QString::fromStdString(info.displayName);
            break;
        }
    }

    aoi::ui::PresenterServices presenterServices{*services.generateObjects, *services.runner, services.probe.get(),
                                                 fallbackBackend};
    aoi::ui::BackendSetup backendSetup{registry.list(), services.backend.id, services.backend.notices};
    aoi::ui::AppPresenters presenters(window, presenterServices, std::move(backendSetup), tokens);
    window.show();

    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) window.openImage(args.first());
#ifdef AOI_DEMO_SCENE
    if (parser.isSet(demoOpt)) {
        bool ok = false;
        const int boxes = parser.value(demoOpt).toInt(&ok);
        if (!ok || boxes < 0) {
            reportStartupError(QStringLiteral("--demo needs a box count, for example --demo 5000."));
            return 2;
        }
        const aoi::dev::FakeBoard board = aoi::dev::makeFakeBoard();
        window.setImage(board.view(), QStringLiteral("demo-%1.fake").arg(boxes));
        aoi::dev::FakeSceneOptions options;
        options.boxes = static_cast<std::size_t>(boxes);
        window.canvas()->showComponents(aoi::dev::makeFakeComponents(options));
    }
#endif
    return app.exec();
}
