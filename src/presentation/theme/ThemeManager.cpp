#include "presentation/theme/ThemeManager.h"

#include <QApplication>
#include <QFont>
#include <QGuiApplication>
#include <QStyleFactory>
#include <QStyleHints>

namespace aoi::ui {

ThemeManager::ThemeManager(QApplication& app, const ThemeTokens& tokens, QObject* parent)
    : QObject(parent), app_(app), tokens_(tokens) {
    // Fusion 會完整遵守 QPalette；Windows 原生樣式在深色時有一部分元件不理會調色盤。
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) app_.setStyle(fusion);
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (mode_ == ThemeMode::System) apply();
    });
    apply();
}

void ThemeManager::setMode(ThemeMode mode) {
    if (mode == mode_) return;
    mode_ = mode;
    apply();
}

void ThemeManager::setFontScale(double scale) {
    if (scale == fontScale_) return;
    fontScale_ = scale;
    apply();
}

bool ThemeManager::isDark() const {
    switch (mode_) {
        case ThemeMode::Light: return false;
        case ThemeMode::Dark: return true;
        case ThemeMode::System: return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }
    return false;
}

const ThemeColors& ThemeManager::colors() const { return isDark() ? tokens_.dark : tokens_.light; }

QPalette ThemeManager::makePalette(const ThemeColors& c) {
    const SurfaceColors& s = c.surface;
    QPalette p;
    p.setColor(QPalette::Window, s.window);
    p.setColor(QPalette::WindowText, s.text);
    p.setColor(QPalette::Base, s.panel);
    p.setColor(QPalette::AlternateBase, s.window);
    p.setColor(QPalette::Text, s.text);
    p.setColor(QPalette::Button, s.panel);
    p.setColor(QPalette::ButtonText, s.text);
    p.setColor(QPalette::BrightText, s.onAccent);
    p.setColor(QPalette::ToolTipBase, s.panel);
    p.setColor(QPalette::ToolTipText, s.text);
    p.setColor(QPalette::PlaceholderText, s.textMuted);
    p.setColor(QPalette::Highlight, s.accent);
    p.setColor(QPalette::HighlightedText, s.onAccent);
    p.setColor(QPalette::Link, s.accent);
    p.setColor(QPalette::Accent, s.accent);
    p.setColor(QPalette::Light, s.panel);
    p.setColor(QPalette::Midlight, s.border);
    p.setColor(QPalette::Mid, s.border);
    p.setColor(QPalette::Dark, s.border);
    p.setColor(QPalette::Shadow, s.border);
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, s.textMuted);
    return p;
}

QString ThemeManager::makeStyleSheet(const ThemeColors& c, const SizeTokens& sz) {
    const SurfaceColors& s = c.surface;
    // 只放調色盤做不到的部分：工具按鈕的大小與選取色、焦點環、狀態列高度。
    return QStringLiteral(
               "QToolBar { background: %1; border: 0; spacing: 4px; padding: 4px; }\n"
               "QToolBar QToolButton { min-width: %2px; min-height: %2px; padding: 0 6px;"
               " border: 1px solid transparent; border-radius: 6px; color: %3; }\n"
               "QToolBar QToolButton:hover { border-color: %4; }\n"
               "QToolBar QToolButton:checked { background: %5; color: %6; }\n"
               "QToolBar QToolButton:focus { border: 2px solid %7; }\n"
               "QToolBar QToolButton:disabled { color: %8; }\n"
               "QPushButton:focus, QComboBox:focus, QSlider:focus { border: 2px solid %7; }\n"
               "QStatusBar { min-height: %9px; max-height: %9px; background: %1; }\n"
               "QStatusBar::item { border: 0; }\n"
               "QDockWidget { font-weight: bold; }\n"
               "QToolTip { color: %3; background: %1; border: 1px solid %4; }\n")
        .arg(s.panel.name(), QString::number(sz.toolbarButton), s.text.name(), s.border.name(), s.accent.name(),
             s.onAccent.name(), s.focusRing.name(), s.textMuted.name(), QString::number(sz.statusBarHeight));
}

void ThemeManager::apply() {
    const ThemeColors& c = colors();
    app_.setPalette(makePalette(c));
    app_.setStyleSheet(makeStyleSheet(c, tokens_.sizes));
    QFont font = app_.font();
    font.setPointSizeF(tokens_.fontBasePt * fontScale_);
    app_.setFont(font);
    emit changed();
}

}  // namespace aoi::ui
