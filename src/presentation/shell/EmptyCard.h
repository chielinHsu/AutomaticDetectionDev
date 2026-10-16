#pragma once

#include <QFrame>
#include <memory>

class QLabel;
class QPushButton;

namespace aoi::ui {

namespace Ui {
class EmptyCard;
}

// 尚未開啟影像時，置中在畫布上的卡片。這是唯一蓋在畫布中央的東西：那時畫布上沒有東西可擋。
// 版面在 EmptyCard.ui（Qt Designer）；文字由 retranslate 從 copy.json 設定（.ui 裡的文字只是佔位、不翻譯）。
class EmptyCard : public QFrame {
    Q_OBJECT
public:
    explicit EmptyCard(QWidget* parent = nullptr);
    ~EmptyCard() override;

    void retranslate();
    QPushButton* openButton() const { return open_; }

signals:
    void openRequested();

private:
    std::unique_ptr<Ui::EmptyCard> ui_;
    QLabel* title_;
    QLabel* body_;
    QLabel* steps_;  // 三步的摘要（2026-10-15，ADR-027）
    QLabel* drop_;
    QPushButton* open_;
};

}  // namespace aoi::ui
