#include <QApplication>
#include <QCommandLineParser>
#include <QStringList>

#include "presentation/canvas/ImageCanvas.h"
#include "presentation/i18n/Translations.h"
#include "presentation/shell/MainWindow.h"
#include "presentation/theme/ThemeManager.h"
#include "presentation/theme/ThemeTokens.h"

#ifdef AOI_DEMO_SCENE
#include "presentation/dev/FakeScene.h"
#endif

namespace {

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
    parser.addOption(langOpt);
    parser.addOption(themeOpt);
#ifdef AOI_DEMO_SCENE
    const QCommandLineOption demoOpt({QStringLiteral("demo")},
                                     QStringLiteral("Open a synthetic 5 MP board with this many boxes."),
                                     QStringLiteral("boxes"));
    parser.addOption(demoOpt);
#endif
    parser.process(app);

    const QString locale = parser.isSet(langOpt) ? parser.value(langOpt) : aoi::ui::Translations::defaultLocale();
    if (!aoi::ui::Translations::install(locale)) {
        qCritical("Cannot load the UI text for '%s'.", qPrintable(locale));
        return 2;
    }

    const aoi::ui::ThemeTokens& tokens = aoi::ui::ThemeTokens::builtin();
    aoi::ui::ThemeManager theme(app, tokens);
    if (parser.isSet(themeOpt)) {
        bool ok = false;
        const aoi::ui::ThemeMode mode = parseTheme(parser.value(themeOpt), &ok);
        if (!ok) {
            qCritical("Unknown theme '%s'.", qPrintable(parser.value(themeOpt)));
            return 2;
        }
        theme.setMode(mode);
    }

    aoi::ui::MainWindow window(theme);
    window.show();

    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) window.openImage(args.first());
#ifdef AOI_DEMO_SCENE
    if (parser.isSet(demoOpt)) {
        bool ok = false;
        const int boxes = parser.value(demoOpt).toInt(&ok);
        if (!ok || boxes < 0) {
            qCritical("--demo needs a box count, for example --demo 5000.");
            return 2;
        }
        const aoi::dev::FakeBoard board = aoi::dev::makeFakeBoard();
        window.setImage(board.toQImage(), QStringLiteral("demo-%1.fake").arg(boxes));
        aoi::dev::FakeSceneOptions options;
        options.boxes = static_cast<std::size_t>(boxes);
        window.canvas()->showComponents(aoi::dev::makeFakeComponents(options));
    }
#endif
    return app.exec();
}
