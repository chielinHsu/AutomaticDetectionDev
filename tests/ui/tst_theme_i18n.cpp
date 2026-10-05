#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTest>

#include "presentation/i18n/Translations.h"
#include "presentation/theme/ThemeManager.h"
#include "presentation/theme/ThemeTokens.h"

using namespace aoi::ui;

namespace {

QByteArray readResource(const char* path) {
    QFile f(QString::fromLatin1(path));
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QSet<QString> placeholders(const QString& text) {
    QSet<QString> out;
    static const QRegularExpression re(QStringLiteral("\\{(\\w+)\\}"));
    for (auto it = re.globalMatch(text); it.hasNext();) out.insert(it.next().captured(1));
    return out;
}

}  // namespace

class TstThemeI18n : public QObject {
    Q_OBJECT
private slots:
    // ---- tokens ----
    void builtinTokensParse() {
        const ThemeTokens& t = ThemeTokens::builtin();
        QVERIFY2(t.valid, qPrintable(t.error));
        QCOMPARE(t.dark.surface.window, QColor("#0D1117"));
        QCOMPARE(t.light.surface.accent, QColor("#0969DA"));
        QCOMPARE(t.overlay.state[static_cast<size_t>(stateIndex(OverlayState::Found))].stroke, QColor("#3FB950"));
        QCOMPARE(t.sizes.handleHit, 24.0);
        QCOMPARE(t.sizes.windowMin, QSize(1024, 640));
        QCOMPARE(t.interaction.zoomMaxPercent, 3200.0);
        QCOMPARE(t.timing.toast, 4000);
        QCOMPARE(t.fontScaleOptions.size(), 3);
    }

    void tokensReportTheBrokenPath() {
        QByteArray json = readResource(":/aoi/ux/tokens.json");
        QVERIFY(!json.isEmpty());
        QJsonObject root = QJsonDocument::fromJson(json).object();
        QJsonObject overlay = root.value("overlay").toObject();
        overlay.remove("region");
        root.insert("overlay", overlay);
        const ThemeTokens broken = ThemeTokens::parse(QJsonDocument(root).toJson());
        QVERIFY(!broken.valid);
        QVERIFY2(broken.error.contains("overlay.region"), qPrintable(broken.error));

        const ThemeTokens garbage = ThemeTokens::parse("{ not json");
        QVERIFY(!garbage.valid);
        QVERIFY(!garbage.error.isEmpty());
    }

    void tokensKeepStateLinesDistinctWithoutColor() {
        // 「不只靠顏色」：四種狀態的線型（寬度、虛線、端點）兩兩不同。
        const ThemeTokens& t = ThemeTokens::builtin();
        for (int a = 0; a < kOverlayStateCount; ++a) {
            for (int b = a + 1; b < kOverlayStateCount; ++b) {
                const LineSpec& x = t.line[static_cast<size_t>(a)];
                const LineSpec& y = t.line[static_cast<size_t>(b)];
                const bool same = x.width == y.width && x.dashPx == y.dashPx && x.cap == y.cap;
                QVERIFY2(!same, "two states share the same line style");
            }
        }
    }

    // ---- 調色盤與 QSS ----
    void paletteFollowsTokens() {
        const ThemeTokens& t = ThemeTokens::builtin();
        for (const ThemeColors* c : {&t.light, &t.dark}) {
            const QPalette p = ThemeManager::makePalette(*c);
            QCOMPARE(p.color(QPalette::Window), c->surface.window);
            QCOMPARE(p.color(QPalette::Base), c->surface.panel);
            QCOMPARE(p.color(QPalette::Text), c->surface.text);
            QCOMPARE(p.color(QPalette::Highlight), c->surface.accent);
            QCOMPARE(p.color(QPalette::HighlightedText), c->surface.onAccent);
            QCOMPARE(p.color(QPalette::Disabled, QPalette::Text), c->surface.textMuted);
        }
    }

    void styleSheetCarriesSizesAndFocusRing() {
        const ThemeTokens& t = ThemeTokens::builtin();
        const QString qss = ThemeManager::makeStyleSheet(t.dark, t.sizes);
        QVERIFY(qss.contains(QStringLiteral("min-width: 36px")));
        QVERIFY(qss.contains(QStringLiteral("min-height: 28px")));
        QVERIFY(qss.contains(t.dark.surface.focusRing.name()));
        QVERIFY(qss.contains(t.dark.surface.accent.name()));
    }

    void themeManagerSwitchesModeAndFontScale() {
        ThemeManager theme(*qApp, ThemeTokens::builtin());
        int changes = 0;
        QObject::connect(&theme, &ThemeManager::changed, [&] { ++changes; });

        theme.setMode(ThemeMode::Dark);
        QVERIFY(theme.isDark());
        QCOMPARE(qApp->palette().color(QPalette::Window), ThemeTokens::builtin().dark.surface.window);
        theme.setMode(ThemeMode::Light);
        QVERIFY(!theme.isDark());
        QCOMPARE(qApp->palette().color(QPalette::Window), ThemeTokens::builtin().light.surface.window);
        QCOMPARE(changes, 2);
        theme.setMode(ThemeMode::Light);  // 沒變就不重套
        QCOMPARE(changes, 2);

        theme.setFontScale(1.5);
        QCOMPARE(qApp->font().pointSizeF(), ThemeTokens::builtin().fontBasePt * 1.5);
        theme.setFontScale(1.0);
        QCOMPARE(qApp->font().pointSizeF(), ThemeTokens::builtin().fontBasePt);
    }

    // ---- 文案 ----
    void copyHasBothLocalesAndMatchingPlaceholders() {
        const QJsonObject keys = QJsonDocument::fromJson(readResource(":/aoi/ux/copy.json")).object().value("keys").toObject();
        QVERIFY(keys.size() > 100);
        for (auto it = keys.begin(); it != keys.end(); ++it) {
            const QJsonObject entry = it.value().toObject();
            const QString zh = entry.value("zh_TW").toString();
            const QString en = entry.value("en").toString();
            QVERIFY2(!zh.isEmpty(), qPrintable("zh_TW missing: " + it.key()));
            QVERIFY2(!en.isEmpty(), qPrintable("en missing: " + it.key()));
            QVERIFY2(placeholders(zh) == placeholders(en),
                     qPrintable("placeholders differ between locales: " + it.key()));
        }
    }

    void translatorSwitchesLanguageAndFallsBackToTheKey() {
        QVERIFY(Translations::install(QStringLiteral("zh_TW")));
        QCOMPARE(qtTrId("action.open"), QStringLiteral("開啟"));
        QVERIFY(Translations::install(QStringLiteral("en")));
        QCOMPARE(qtTrId("action.open"), QStringLiteral("Open"));
        QCOMPARE(Translations::current(), QStringLiteral("en"));
        // 找不到的 key 回傳 key 本身，畫面上看得到錯字。
        QCOMPARE(qtTrId("ui.does.not.exist"), QStringLiteral("ui.does.not.exist"));
        // 不存在的語系：安裝失敗，目前的語系不變。
        QVERIFY(!Translations::install(QStringLiteral("xx")));
        QCOMPARE(Translations::current(), QStringLiteral("en"));
        QCOMPARE(qtTrId("action.open"), QStringLiteral("Open"));
    }

    void namedPlaceholdersSurviveDifferentWordOrder() {
        QVERIFY(Translations::install(QStringLiteral("zh_TW")));
        // zh：「找到 {found} 個 · 推算 {inferred} 個 · 拒絕 {rejected} 個」；en 的順序相同但字不同，
        // 這裡只驗證具名替換不依賴位置。
        const QString zh = uiText("ui.preview.summary", {{"rejected", "3"}, {"found", "118"}, {"inferred", "2"}});
        QCOMPARE(zh, QStringLiteral("找到 118 個 · 推算 2 個 · 拒絕 3 個"));
        QCOMPARE(formatNamed("{b}-{a}-{b}", {{"a", "1"}, {"b", "2"}}), QStringLiteral("2-1-2"));
        // 沒有對應值的佔位字原樣保留，不會被吃掉。
        QCOMPARE(formatNamed("{a} {missing}", {{"a", "1"}}), QStringLiteral("1 {missing}"));
    }

    void defaultLocaleIsOneOfTheShippedOnes() {
        QVERIFY(Translations::availableLocales().contains(Translations::defaultLocale()));
    }

    void cleanupTestCase() { Translations::install(QStringLiteral("zh_TW")); }
};

QTEST_MAIN(TstThemeI18n)
#include "tst_theme_i18n.moc"
