#pragma once

#include <QFrame>

class QLabel;
class QPushButton;

namespace aoi::ui {

// 尚未開啟影像時，置中在畫布上的卡片。這是唯一蓋在畫布中央的東西：那時畫布上沒有東西可擋。
class EmptyCard : public QFrame {
    Q_OBJECT
public:
    explicit EmptyCard(QWidget* parent = nullptr);

    void retranslate();
    QPushButton* openButton() const { return open_; }

signals:
    void openRequested();

private:
    QLabel* title_;
    QLabel* body_;
    QLabel* drop_;
    QPushButton* open_;
};

}  // namespace aoi::ui
