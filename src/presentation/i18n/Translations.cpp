#include "presentation/i18n/Translations.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <memory>

namespace aoi::ui {
namespace {

constexpr const char* kLocales[] = {"zh_TW", "en"};

struct State {
    std::unique_ptr<JsonTranslator> translator;
    QString locale;
};

State& state() {
    static State s;
    return s;
}

QByteArray readCopyResource() {
    QFile f(QStringLiteral(":/aoi/ux/copy.json"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

}  // namespace

JsonTranslator::JsonTranslator(const QByteArray& copyJson, const QString& locale, QObject* parent)
    : QTranslator(parent) {
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(copyJson, &pe);
    if (!doc.isObject()) {
        error_ = "copy.json: " + pe.errorString();
        return;
    }
    const QJsonObject keys = doc.object().value("keys").toObject();
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        const QString text = it.value().toObject().value(locale).toString();
        if (text.isEmpty()) {
            error_ = "copy.json: key '" + it.key() + "' has no text for " + locale;
            table_.clear();
            return;
        }
        table_.insert(it.key(), text);
    }
}

QString JsonTranslator::translate(const char* /*context*/, const char* sourceText, const char* /*disambiguation*/,
                                  int /*n*/) const {
    // id-based：sourceText 就是 key。context 對 id 沒有意義（qtTrId 傳 nullptr）。
    const auto it = table_.constFind(QString::fromUtf8(sourceText));
    return it == table_.constEnd() ? QString() : it.value();
}

QStringList Translations::availableLocales() {
    QStringList out;
    for (const char* l : kLocales) out << QString::fromLatin1(l);
    return out;
}

QString Translations::defaultLocale() {
    return QLocale::system().language() == QLocale::Chinese ? QStringLiteral("zh_TW") : QStringLiteral("en");
}

bool Translations::install(const QString& locale) {
    auto next = std::make_unique<JsonTranslator>(readCopyResource(), locale);
    if (!next->isValid() || next->isEmpty()) return false;
    State& s = state();
    if (s.translator) QCoreApplication::removeTranslator(s.translator.get());
    s.translator = std::move(next);
    s.locale = locale;
    QCoreApplication::installTranslator(s.translator.get());
    return true;
}

QString Translations::current() { return state().locale; }

QString formatNamed(QString text, TextArgs args) {
    for (const auto& [name, value] : args) text.replace(QStringLiteral("{%1}").arg(QLatin1String(name)), value);
    return text;
}

QString uiText(const char* id, TextArgs args) { return formatNamed(qtTrId(id), args); }

}  // namespace aoi::ui
