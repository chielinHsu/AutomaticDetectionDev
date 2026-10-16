#pragma once

#include <QImage>
#include <QPixmap>
#include <QVector>
#include <QWidget>
#include <array>
#include <memory>

#include "application/interaction/StepGuide.h"
#include "presentation/presenters/ViewInterfaces.h"

class QFrame;
class QLabel;
class QTimer;
template <typename T>
class QFutureWatcher;

namespace aoi::ui {

namespace Ui {
class GuidancePanel;
}

// 背景縮好的一組示範畫面（某個寬度）。
struct GuideScaledFrames {
    int width = 0;
    QVector<QImage> frames;
};

// 「操作步驟」dock 的內容（2026-10-15，ADR-027）：五步的清單＋目前那一步的示範。實作 IGuidanceView。
//
//  - 哪一步是目前、哪些做完了，由 application 的 guideStatus 決定（Presenter 只在它變了的時候才交過來）；這裡只畫。
//    目前的那一步用主題的 Highlight 色（palette，跟著主題走）；做完的打勾、字變淡；進行中、要重做時那一步下面多一行說明。
//  - 示範：每一步幾張關鍵畫面（GuideDemo.h），逐格播放、循環；跟著目前的步驟走，點另一步就看那一步的。
//    面板看不到時計時器停下（示範不花時間在畫面外）；畫面從資源延遲載入：面板看得到、第一次看那一步時才解碼，而且在
//    背景執行緒解（QtConcurrent，QFutureWatcher 掛在面板上：面板解構時不會再收到結果）。UI 執行緒永遠不等解碼——
//    Debug 的 Qt 解一張 PNG 要幾百 ms，第一版在狀態一變時同步解碼，tst_presenter（每個測試台一個面板）整組逾時。
//  - 文字全部來自 copy.json（retranslate）；版面在 GuidancePanel.ui，步驟列依資料在程式裡建立。
class GuidancePanel : public QWidget, public IGuidanceView {
    Q_OBJECT
public:
    explicit GuidancePanel(QWidget* parent = nullptr);
    ~GuidancePanel() override;

    // ---- IGuidanceView ----
    void setGuideStatus(const GuideStatus& status) override;

    void retranslate();
    // 看這一步的示範（點步驟列也是走這裡）。
    void showDemo(GuideStep step);

    // ---- 讀（測試用）----
    const GuideStatus& status() const { return status_; }
    bool hasStatus() const { return hasStatus_; }
    int updateCount() const { return updateCount_; }  // setGuideStatus 被呼叫的次數（門檻拖動不該增加）
    QString rowTitle(GuideStep step) const;
    QString rowNote(GuideStep step) const;  // 進行中、要重做的那一行（空 ＝ 沒有）
    bool isRowCurrent(GuideStep step) const;
    bool isRowDone(GuideStep step) const;
    GuideStep demoStep() const { return demoStep_; }
    int demoFrame() const { return demoFrame_; }  // 從 0 起
    int demoFrameCount() const;
    QString demoCaption() const;
    bool demoPlaying() const;
    bool demoHasFrames() const;
    bool demoLoading() const;  // 這一步的畫面還在背景解碼
    // 示範的畫格播放間隔（毫秒）；測試可以縮短。
    void setFrameIntervalMs(int ms);

signals:
    void stepClicked(aoi::GuideStep step);

protected:
    // 建議大小固定（寬度＝dock 的最小寬度）：內容變了（進行中那一行出現、示範換圖）不改 dock 的寬度，畫布就不會被重排、重畫。
    // 沒有這一條時，「進行中…」那一行一出現 dock 就變寬，畫布跟著重排、整張板子重畫。
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Row {
        QFrame* box = nullptr;
        QLabel* number = nullptr;
        QLabel* title = nullptr;
        QLabel* shortcut = nullptr;
        QLabel* body = nullptr;
        QLabel* note = nullptr;
        // 上次畫的樣子：沒變的列不碰（每一列的 palette 與文字都重設會讓整個面板重畫；Debug 實測一次約 60 ms）。
        int painted = -1;  // -1 ＝ 還沒畫；否則 current<<2 | done<<1 | hasNote
        QString paintedNote;
    };
    void buildRows();
    void paintRows();
    void loadFrames(GuideStep step);
    void showFrame(int index);
    void advanceFrame();
    void updatePlayback();
    void relayoutImage();
    int availableWidth() const;
    void requestScaled(GuideStep step, int width);

    std::unique_ptr<Ui::GuidancePanel> ui_;
    QLabel* intro_;
    QWidget* stepsBox_;
    QFrame* demoBox_;
    QLabel* demoTitle_;
    QLabel* demoImage_;
    QLabel* demoCaption_;
    QLabel* demoIndex_;
    QTimer* frameTimer_;
    std::array<Row, kGuideStepCount> rows_{};
    std::array<QVector<QPixmap>, kGuideStepCount> frames_{};
    std::array<bool, kGuideStepCount> framesLoaded_{};
    std::array<QFutureWatcher<QVector<QImage>>*, kGuideStepCount> loaders_{};  // 正在背景解碼的（nullptr ＝ 沒有）
    std::array<QFutureWatcher<GuideScaledFrames>*, kGuideStepCount> scalers_{};  // 正在背景縮放的（nullptr ＝ 沒有）
    // 縮到面板寬度的複本：每個寬度只縮一次（步驟換了、計時器換畫格都只是換 pixmap，不重新縮放；I4、I6 量到過），
    // 而且平滑縮放在背景做（Debug 的 Qt 一組要上百 ms）；做好之前先用快速縮放頂著。
    struct ScaledFrames {
        int width = 0;
        QVector<QPixmap> frames;
    };
    std::array<ScaledFrames, kGuideStepCount> scaled_{};
    GuideStatus status_;
    bool hasStatus_ = false;
    int updateCount_ = 0;
    GuideStep demoStep_ = GuideStep::OpenImage;
    int demoFrame_ = 0;
};

}  // namespace aoi::ui
