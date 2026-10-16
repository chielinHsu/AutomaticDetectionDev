#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QDockWidget>
#include <QSettings>
#include <QStringList>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string>

#include "app/CompositionRoot.h"
#include "application/backend/BackendSwitch.h"
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

// 程式之後會一直跑（視窗）的訊息：被管線接走時，不 flush 就要等結束才看得到。
void reportLine(const QString& message) {
    std::fprintf(stderr, "%s\n", qPrintable(message));
    std::fflush(stderr);
}

aoi::ui::ThemeMode parseTheme(const QString& name, bool* ok) {
    *ok = true;
    if (name == QLatin1String("light")) return aoi::ui::ThemeMode::Light;
    if (name == QLatin1String("dark")) return aoi::ui::ThemeMode::Dark;
    if (name == QLatin1String("system")) return aoi::ui::ThemeMode::System;
    *ok = false;
    return aoi::ui::ThemeMode::System;
}

// 使用者的設定（ADR-021：記住上次用的 back end）。INI 檔：%APPDATA%\AOI\AutoCadCreated.ini（看得到、可以手改）。
// 環境變數 AOI_SETTINGS_DIR 可以換資料夾（tst_cli 用暫存資料夾，測試不碰使用者的設定）。
constexpr const char* kLastBackendKey = "backend/last";
// 操作步驟 dock 的開關（2026-10-15，ADR-027）：預設打開；使用者關掉就記住（只在使用者切換時寫，視窗關閉時 dock 跟著藏起來不算）。
constexpr const char* kGuidanceVisibleKey = "guidance/visible";

std::unique_ptr<QSettings> openUserSettings() {
    const QByteArray dir = qgetenv("AOI_SETTINGS_DIR");
    if (!dir.isEmpty()) QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QString::fromLocal8Bit(dir));
    return std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(),
                                       QCoreApplication::applicationName());
}

void rememberBackend(QSettings& settings, const std::string& id) {
    settings.setValue(QLatin1String(kLastBackendKey), QString::fromStdString(id));
    settings.sync();  // 程式可能被直接結束（例如測試 kill）：現在就寫進檔案
}

std::optional<std::string> rememberedBackend(const QSettings& settings) {
    const QString id = settings.value(QLatin1String(kLastBackendKey)).toString();
    if (id.isEmpty()) return std::nullopt;
    return id.toStdString();
}

}  // namespace

int main(int argc, char* argv[]) {
    // 內部用（2026-10-09）：組裝根在這個程式碰 HALCON 之前，用另一個行程檢查授權（app/CompositionRoot.h）。不建立 Qt 的畫面。
    if (argc == 2 && std::strcmp(argv[1], aoi::app::kProbeHalconLicenseOption) == 0) return aoi::app::runHalconLicenseProbe();

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
                                        QStringLiteral("Vision back end: fake, opencv or halcon (default: the last one used, "
                                                       "else fake)."),
                                        QStringLiteral("id"));
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
    // 用哪一個 back end（ADR-021，使用者 2026-10-08 決定：記住上次）：
    //  - 命令列指定的：馬上建立；不存在或不可用時明確失敗（結束碼 2），不退回別的（使用者要的是哪一個，就是哪一個）。
    //  - 否則先用 fake（視窗馬上出現），記住的那一個在視窗出現之後於背景換過去；它不能用時留在 fake，訊息列說原因。
    const std::unique_ptr<QSettings> settings = openUserSettings();
    const aoi::BackendRegistry registry = aoi::app::makeBackendRegistry();
    std::optional<std::string> commandLineBackend;
    if (parser.isSet(backendOpt)) commandLineBackend = parser.value(backendOpt).toStdString();
    const aoi::StartupPlan plan = aoi::planStartupBackend(commandLineBackend, rememberedBackend(*settings), "fake");
    aoi::Result<aoi::app::CoreServices> core = aoi::app::composeCore(registry, plan.initial);
    if (!core) {
        reportStartupError(QStringLiteral("Cannot start the '%1' back end: %2")
                               .arg(QString::fromStdString(plan.initial), QString::fromStdString(core.error().message)));
        return 2;
    }
    if (plan.strict) rememberBackend(*settings, plan.initial);

    // 影像來源（S06）：視窗經它開圖。要活得比視窗久，所以先建立。
    const std::unique_ptr<aoi::IImageSource> imageSource = aoi::app::makeImageSource();

    // 視窗、Presenter 的建立順序＝解構順序的反面：核心活得最久，然後是視窗，最後才是 Presenter（它解構時先解除視窗的 Controller）。
    aoi::ui::MainWindow window(theme);
    window.setImageSource(imageSource.get());
    window.guidanceDock()->setVisible(settings->value(QLatin1String(kGuidanceVisibleKey), true).toBool());
    QObject::connect(window.guidanceDock()->toggleViewAction(), &QAction::toggled, &window, [&settings](bool visible) {
        settings->setValue(QLatin1String(kGuidanceVisibleKey), visible);
        settings->sync();
    });
    aoi::app::CoreServices& services = core.value();
    // back end 的提醒（S08：例如 HALCON 用的是 runtime 授權、SDK 的 dongle 沒插）：stderr 一行；畫面上是訊息列的提醒卡片
    // （按〔關閉〕才收起）與狀態列 back end 名稱後面的標示（BackendPresenter）。
    for (const aoi::BackendNotice& notice : services.engine->backend.notices) reportLine(QString::fromStdString(notice.message));

    // 錯誤卡片用的 back end 名稱（目前的、建議改用的）由 BackendPresenter 交給 AutoRoiPresenter（AppPresenters 接線）。
    aoi::ui::PresenterServices presenterServices{aoi::useCaseOf(services.engine), *services.runner, services.probe.get()};
    // 執行中切換（ADR-021）：在背景執行緒（和 JobRunner 同一個執行緒池）建立新的 back end ＋ use case；registry 與 probe 活得比它久。
    aoi::ui::BackendServices backendServices;
    backendServices.backends = registry.list();
    backendServices.current = services.engine;
    backendServices.create = [&registry, probe = services.probe.get()](const std::string& id) {
        return aoi::makeSearchEngine(registry, id, probe);
    };
    backendServices.executor = services.executor.get();
    aoi::ui::AppPresenters presenters(window, presenterServices, std::move(backendServices), tokens);
    aoi::ui::BackendPresenter& backend = presenters.backend();
    // 換成功：記住（下次啟動用它）、提醒寫到 stderr。換失敗：原因寫到 stderr（畫面上是訊息列的卡片），不改記住的值。
    QObject::connect(&backend, &aoi::ui::BackendPresenter::backendChanged, &backend,
                     [&settings](const QString& id, const QStringList& noticeMessages) {
                         rememberBackend(*settings, id.toStdString());
                         reportLine(QStringLiteral("Switched to the '%1' back end.").arg(id));
                         for (const QString& message : noticeMessages) reportLine(message);
                     });
    QObject::connect(&backend, &aoi::ui::BackendPresenter::switchFailed, &backend,
                     [&backend](const QString& id, const QString& reason) {
                         reportLine(QStringLiteral("Cannot switch to the '%1' back end: %2 Still using '%3'.")
                                        .arg(id, reason, QString::fromStdString(backend.current())));
                     });
    window.show();
    if (!plan.switchTo.empty()) {
        reportLine(QStringLiteral("Switching to the remembered back end '%1'.").arg(QString::fromStdString(plan.switchTo)));
        backend.selectBackend(plan.switchTo);
    }

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
