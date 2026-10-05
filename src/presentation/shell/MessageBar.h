#pragma once

#include <QFrame>
#include <QString>
#include <QStringList>

#include "presentation/theme/ThemeTokens.h"

class QLabel;
class QPushButton;
class QScrollArea;
class QTimer;
class QHBoxLayout;
class QWidget;

namespace aoi::ui {

// 畫布正下方的訊息列。所有要閱讀的訊息（提示、吐司、無匹配卡片、錯誤）都在這裡，不蓋在影像上：
// 檢測員要看著料件才能判斷，蓋住的那一塊就是他要看的東西（docs/ux/ux-spec.md 第 5 節）。
//
// 高度固定（tokens 的 sizes.messageBarHeight），內容放不下就在列內捲動。
// 這樣不論訊息怎麼變，畫布大小都不變，板子不會被擠動。
class MessageBar : public QFrame {
    Q_OBJECT
public:
    explicit MessageBar(const ThemeTokens& tokens, QWidget* parent = nullptr);

    // 常駐提示（例如「S 教一個新樣本 · R 框選區域…」）。空字串＝沒有提示。
    void setHint(const QString& text);
    // 暫時蓋過提示，timing.toast 毫秒後還原。
    void showToast(const QString& text);
    // 卡片：標題、說明、做法清單、動作按鈕。按鈕被按下時發出 actionTriggered(索引)。
    void showCard(const QString& title, const QString& body, const QStringList& advice, const QStringList& actions);
    void clearCard();

    bool cardVisible() const;
    QString hintText() const;   // 目前提示列顯示的文字（含吐司）
    QString cardTitle() const;

signals:
    void actionTriggered(int index);

private:
    void showHintLabel(const QString& text);

    const ThemeTokens& tokens_;
    QScrollArea* scroll_;
    QLabel* hint_;
    QWidget* card_;
    QLabel* cardTitle_;
    QLabel* cardBody_;
    QLabel* cardAdvice_;
    QHBoxLayout* actions_;
    QTimer* toastTimer_;
    QString baseHint_;
};

}  // namespace aoi::ui
