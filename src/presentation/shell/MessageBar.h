#pragma once

#include <QFrame>
#include <QString>
#include <QStringList>

#include "presentation/theme/ThemeTokens.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QTimer;
class QHBoxLayout;
class QWidget;

namespace aoi::ui {

// 畫布正下方的訊息列。所有要閱讀的訊息（提示、吐司、無匹配卡片、錯誤、back end 的提醒）都在這裡，不蓋在影像上：
// 檢測員要看著料件才能判斷，蓋住的那一塊就是他要看的東西（docs/ux/ux-spec.md 第 5 節）。
//
// 高度固定（tokens 的 sizes.messageBarHeight），內容放不下就在列內捲動。
// 這樣不論訊息怎麼變，畫布大小都不變，板子不會被擠動。
//
// 同一時間只顯示一種內容，優先順序：進度列 > 卡片 > 提醒卡片 > 提示。吐司暫時蓋過提示（卡片上方也看得到）。
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

    // 提醒卡片（back end 的提醒、切換失敗）：不會自己消失。卡片或進度列顯示時它讓位（藏起來），它們收起之後再出現，
    // 直到 clearNotice。按鈕被按下時發出 noticeActionTriggered(索引)；收不收起由呼叫端決定。
    void showNotice(const QString& title, const QString& body, const QStringList& advice, const QStringList& actions);
    void clearNotice();

    // 進度：搜尋超過 1 s 時顯示進度條與〔取消〕。fraction 是 0..1；按下〔取消〕發出 cancelRequested。
    // 進度顯示時會蓋過提示與卡片；showCard 或 clearProgress 會收起它。
    void showProgress(const QString& text, double fraction, const QString& cancelLabel);
    void clearProgress();
    bool progressVisible() const;
    int progressValue() const;  // 0..100

    bool cardVisible() const;
    bool noticeVisible() const;  // 提醒卡片現在看得到（讓位時是 false）
    bool noticePending() const;  // 有提醒卡片（看不看得到都算），還沒 clearNotice
    QString hintText() const;   // 目前提示列顯示的文字（含吐司）
    QString cardTitle() const;
    QString noticeTitle() const;

signals:
    void actionTriggered(int index);
    void noticeActionTriggered(int index);
    void cancelRequested();

private:
    // 卡片與提醒卡片長得一樣：標題、說明、做法清單、按鈕列。
    struct CardParts {
        QWidget* box = nullptr;
        QLabel* title = nullptr;
        QLabel* body = nullptr;
        QLabel* advice = nullptr;
        QHBoxLayout* actions = nullptr;
    };
    CardParts makeCard(QWidget* parent, const QString& objectName);
    void fillCard(CardParts& parts, const QString& title, const QString& body, const QStringList& advice,
                  const QStringList& actions, bool notice);
    void showHintLabel(const QString& text);
    // 沒有卡片、沒有進度列時顯示的東西：提醒卡片（有的話），否則提示（吐司顯示中就是吐司）。
    void showResting();

    const ThemeTokens& tokens_;
    QScrollArea* scroll_;
    QLabel* hint_;
    CardParts card_;
    CardParts notice_;
    QWidget* progress_;
    QLabel* progressText_;
    QProgressBar* progressBar_;
    QPushButton* cancelButton_;
    QTimer* toastTimer_;
    QString baseHint_;
    bool noticePending_ = false;
};

}  // namespace aoi::ui
