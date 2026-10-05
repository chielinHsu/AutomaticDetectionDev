#pragma once

#include <QObject>
#include <QPalette>
#include <QString>

#include "presentation/theme/ThemeTokens.h"

class QApplication;

namespace aoi::ui {

enum class ThemeMode { Light, Dark, System };

// 把 tokens 變成 QPalette 與 QSS，並在淺色、深色、跟隨系統之間切換。
// 字級縮放只放大文字（QApplication 的字型）；把手與點擊區是 tokens 的固定像素，不隨字級變。
class ThemeManager : public QObject {
    Q_OBJECT
public:
    ThemeManager(QApplication& app, const ThemeTokens& tokens, QObject* parent = nullptr);

    ThemeMode mode() const { return mode_; }
    void setMode(ThemeMode mode);

    double fontScale() const { return fontScale_; }
    void setFontScale(double scale);

    // 目前實際生效的是深色嗎（System 時看作業系統設定）。
    bool isDark() const;
    const ThemeColors& colors() const;
    const ThemeTokens& tokens() const { return tokens_; }

    static QPalette makePalette(const ThemeColors& colors);
    static QString makeStyleSheet(const ThemeColors& colors, const SizeTokens& sizes);

signals:
    void changed();

private:
    void apply();

    QApplication& app_;
    const ThemeTokens& tokens_;
    ThemeMode mode_ = ThemeMode::System;
    double fontScale_ = 1.0;
};

}  // namespace aoi::ui
