#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include <initializer_list>
#include <utility>

namespace aoi::ui {

// 文案的唯一來源是 docs/ux/copy.json（內嵌為資源 :/aoi/ux/copy.json）。
// 這個 QTranslator 直接讀它，所以程式裡用標準的 qtTrId("ui.noMatch.title")，不需要另外產生 .ts／.qm。
// 找不到的 key，Qt 會回傳 key 本身，畫面上看得到錯字。
class JsonTranslator : public QTranslator {
public:
    // locale 是 copy.json 的語系欄位名，例如 "zh_TW"、"en"。
    JsonTranslator(const QByteArray& copyJson, const QString& locale, QObject* parent = nullptr);

    bool isValid() const { return error_.isEmpty(); }
    QString error() const { return error_; }
    int keyCount() const { return static_cast<int>(table_.size()); }

    QString translate(const char* context, const char* sourceText, const char* disambiguation = nullptr,
                      int n = -1) const override;
    bool isEmpty() const override { return table_.isEmpty(); }

private:
    QHash<QString, QString> table_;
    QString error_;
};

// 一個語系：載入資源、換掉舊的 translator。語系用 copy.json 的欄位名（"zh_TW"、"en"）。
class Translations {
public:
    static QStringList availableLocales();
    static QString defaultLocale();  // 依作業系統語言：中文 → zh_TW，其餘 → en
    static bool install(const QString& locale);
    static QString current();
};

using TextArgs = std::initializer_list<std::pair<const char*, QString>>;

// 取文案並把 {name} 換成值。用具名佔位而不是 %1，因為兩種語言的語序可以不同。
QString uiText(const char* id, TextArgs args = {});

// 單獨替換佔位字（測試與不經 Qt 翻譯的字串用）。
QString formatNamed(QString text, TextArgs args);

}  // namespace aoi::ui
