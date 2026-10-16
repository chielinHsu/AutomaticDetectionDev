#include "presentation/guidance/GuidancePanel.h"

#include <QEvent>
#include <QFrame>
#include <QFutureWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <QSizePolicy>
#include <QtConcurrent/QtConcurrentRun>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

#include "presentation/guidance/GuideDemo.h"
#include "presentation/i18n/Translations.h"
#include "presentation/theme/ThemeTokens.h"
#include "ui_GuidancePanel.h"

namespace aoi::ui {

namespace {

constexpr int kDefaultFrameMs = 1500;

// 整個行程共用一份解碼結果：產品只有一個面板，但測試每個測試台都開一個（tst_presenter 有 85 個）；第一個面板解一次，
// 之後的直接拿（Debug 的 Qt 解一張 400 px 的 PNG 也要幾十到幾百 ms）。只在背景執行緒碰它（QtConcurrent 的工作裡）。
struct DemoFrameStore {
    QMutex mutex;
    std::array<bool, kGuideStepCount> loaded{};
    std::array<QVector<QImage>, kGuideStepCount> frames{};
};

DemoFrameStore& demoFrameStore() {
    static DemoFrameStore store;
    return store;
}

// 背景工作：這一步的畫面（第一次解碼並存起來；缺一張就當作整組還沒產生）。
QVector<QImage> decodeDemoFrames(GuideStep step) {
    DemoFrameStore& store = demoFrameStore();
    const std::size_t i = static_cast<std::size_t>(guideStepIndex(step));
    QMutexLocker lock(&store.mutex);
    if (store.loaded[i]) return store.frames[i];
    QVector<QImage> out;
    for (int n = 1; n <= guideDemoFrameCount(step); ++n) {
        QImage image(guideDemoFramePath(step, n));
        if (image.isNull()) {
            out.clear();
            break;
        }
        out.push_back(image);
    }
    store.loaded[i] = true;
    store.frames[i] = out;
    return out;
}

}  // namespace

GuidancePanel::GuidancePanel(QWidget* parent) : QWidget(parent), ui_(std::make_unique<Ui::GuidancePanel>()) {
    // 版面在 GuidancePanel.ui：說明、步驟列的容器（列在 buildRows 建立）、示範（標題、畫面、字幕、第幾張）。
    ui_->setupUi(this);
    intro_ = ui_->intro;
    stepsBox_ = ui_->stepsBox;
    demoBox_ = ui_->demoBox;
    demoTitle_ = ui_->demoTitle;
    demoImage_ = ui_->demoImage;
    demoCaption_ = ui_->demoCaption;
    demoIndex_ = ui_->demoIndex;
    intro_->setForegroundRole(QPalette::PlaceholderText);
    demoIndex_->setForegroundRole(QPalette::PlaceholderText);
    // 示範圖不參與版面的寬度（Ignored）：否則圖的寬度會推寬 dock、dock 變寬又重新縮圖，來回重排。高度跟著目前的圖。
    demoImage_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    demoImage_->setMinimumSize(0, 0);
    demoImage_->setFixedHeight(120);
    frameTimer_ = new QTimer(this);
    frameTimer_->setInterval(kDefaultFrameMs);
    connect(frameTimer_, &QTimer::timeout, this, &GuidancePanel::advanceFrame);
    buildRows();
    retranslate();
    showDemo(GuideStep::OpenImage);
}

GuidancePanel::~GuidancePanel() = default;

void GuidancePanel::buildRows() {
    auto* layout = qobject_cast<QVBoxLayout*>(stepsBox_->layout());
    for (int i = 0; i < kGuideStepCount; ++i) {
        Row& row = rows_[static_cast<std::size_t>(i)];
        row.box = new QFrame(stepsBox_);
        row.box->setObjectName(QStringLiteral("guideStep%1").arg(i));
        row.box->setAutoFillBackground(true);
        row.box->setFrameShape(QFrame::NoFrame);
        row.box->setCursor(Qt::PointingHandCursor);
        auto* h = new QHBoxLayout(row.box);
        h->setContentsMargins(6, 4, 6, 4);
        h->setSpacing(6);
        row.number = new QLabel(row.box);
        row.number->setFixedWidth(20);
        row.number->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        QFont bold = row.number->font();
        bold.setBold(true);
        row.number->setFont(bold);
        h->addWidget(row.number);
        auto* v = new QVBoxLayout();
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(1);
        auto* titleRow = new QHBoxLayout();
        titleRow->setContentsMargins(0, 0, 0, 0);
        row.title = new QLabel(row.box);
        row.title->setFont(bold);
        row.shortcut = new QLabel(row.box);
        row.shortcut->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        titleRow->addWidget(row.title, 1);
        titleRow->addWidget(row.shortcut, 0);
        v->addLayout(titleRow);
        row.body = new QLabel(row.box);
        row.body->setWordWrap(true);
        v->addWidget(row.body);
        row.note = new QLabel(row.box);
        row.note->setWordWrap(true);
        row.note->hide();
        v->addWidget(row.note);
        h->addLayout(v, 1);
        layout->addWidget(row.box);
        row.box->installEventFilter(this);
    }
}

void GuidancePanel::retranslate() {
    intro_->setText(qtTrId("ui.guide.intro"));
    for (int i = 0; i < kGuideStepCount; ++i) {
        const GuideStep step = guideStepAt(i);
        Row& row = rows_[static_cast<std::size_t>(i)];
        row.title->setText(qtTrId(guideStepTitleKey(step).constData()));
        row.body->setText(qtTrId(guideStepBodyKey(step).constData()));
        row.shortcut->setText(QLatin1String(guideStepShortcut(step)));
        row.painted = -1;  // 文字換了（語言切換）：每一列重畫
    }
    paintRows();
    showFrame(demoFrame_);
}

void GuidancePanel::setGuideStatus(const GuideStatus& status) {
    ++updateCount_;
    const bool stepChanged = !hasStatus_ || status.current != status_.current;
    status_ = status;
    hasStatus_ = true;
    paintRows();
    if (stepChanged) showDemo(status.current);
}

void GuidancePanel::paintRows() {
    for (int i = 0; i < kGuideStepCount; ++i) {
        const GuideStep step = guideStepAt(i);
        Row& row = rows_[static_cast<std::size_t>(i)];
        const bool current = hasStatus_ && status_.current == step;
        const bool done = hasStatus_ && status_.isDone(step);
        QString note;
        if (current && status_.busy) note = qtTrId("ui.guide.busy");
        else if (current && status_.attention) note = qtTrId("ui.guide.attention");
        const int key = (current ? 4 : 0) | (done ? 2 : 0) | (note.isEmpty() ? 0 : 1);
        if (row.painted == key && row.paintedNote == note) continue;  // 沒變的列不碰
        row.painted = key;
        row.paintedNote = note;
        // 目前：主題的 Highlight（跟著主題走）；做完：打勾、字變淡；其餘：一般。
        row.box->setBackgroundRole(current ? QPalette::Highlight : QPalette::Window);
        const QPalette::ColorRole text = current ? QPalette::HighlightedText : (done ? QPalette::PlaceholderText : QPalette::WindowText);
        for (QLabel* label : {row.number, row.title, row.shortcut, row.body, row.note}) label->setForegroundRole(text);
        row.number->setText(done && !current ? QStringLiteral("✓") : QString::number(i + 1));
        row.note->setText(note);
        row.note->setVisible(!note.isEmpty());
    }
}

// ---- 示範 ----

// 在背景解碼這一步的畫面（QImage 的讀取可以在別的執行緒；QPixmap 要回到 UI 執行緒再建）。
// 結果回來時這一步還是目前的示範就顯示；面板已經解構時 watcher 跟著消失，不會再收到。
void GuidancePanel::loadFrames(GuideStep step) {
    const std::size_t i = static_cast<std::size_t>(guideStepIndex(step));
    if (framesLoaded_[i] || loaders_[i] != nullptr) return;
    auto* watcher = new QFutureWatcher<QVector<QImage>>(this);
    loaders_[i] = watcher;
    connect(watcher, &QFutureWatcherBase::finished, this, [this, step, i, watcher] {
        const QVector<QImage> images = watcher->result();
        watcher->deleteLater();
        loaders_[i] = nullptr;
        framesLoaded_[i] = true;
        frames_[i].clear();
        for (const QImage& image : images) frames_[i].push_back(QPixmap::fromImage(image));
        if (demoStep_ == step) {
            showFrame(demoFrame_);
            updatePlayback();
        }
    });
    watcher->setFuture(QtConcurrent::run([step] { return decodeDemoFrames(step); }));
}

void GuidancePanel::showDemo(GuideStep step) {
    demoStep_ = step;
    // 面板看得到才解碼（第一次看那一步時）：dock 關著的時候換步驟不花時間；看得到時才在 showEvent 補載。
    if (isVisible()) loadFrames(step);
    showFrame(0);
    updatePlayback();
}

int GuidancePanel::demoFrameCount() const { return frames_[static_cast<std::size_t>(guideStepIndex(demoStep_))].size(); }

bool GuidancePanel::demoHasFrames() const { return demoFrameCount() > 0; }

bool GuidancePanel::demoLoading() const { return loaders_[static_cast<std::size_t>(guideStepIndex(demoStep_))] != nullptr; }

bool GuidancePanel::demoPlaying() const { return frameTimer_->isActive(); }

void GuidancePanel::setFrameIntervalMs(int ms) {
    frameTimer_->setInterval(std::max(1, ms));
    if (frameTimer_->isActive()) frameTimer_->start();
}

QString GuidancePanel::demoCaption() const { return demoCaption_->text(); }

void GuidancePanel::showFrame(int index) {
    const int count = demoFrameCount();
    demoFrame_ = count > 0 ? ((index % count) + count) % count : 0;
    demoTitle_->setText(uiText("ui.guide.demo.title", {{"step", qtTrId(guideStepTitleKey(demoStep_).constData())}}));
    if (count == 0) {
        demoImage_->clear();
        demoImage_->setFixedHeight(120);
        demoCaption_->setText(demoLoading() ? QString() : qtTrId("ui.guide.demo.missing"));  // 還在解碼就先空著
        demoIndex_->clear();
        return;
    }
    demoCaption_->setText(qtTrId(guideDemoCaptionKey(demoStep_, demoFrame_ + 1).constData()));
    demoIndex_->setText(uiText("ui.guide.demo.frame", {{"n", QString::number(demoFrame_ + 1)}, {"total", QString::number(count)}}));
    relayoutImage();
}

int GuidancePanel::availableWidth() const { return std::max(64, demoBox_->contentsRect().width() - 12); }

void GuidancePanel::relayoutImage() {
    const int count = demoFrameCount();
    if (count == 0) return;
    const std::size_t i = static_cast<std::size_t>(guideStepIndex(demoStep_));
    const QVector<QPixmap>& frames = frames_[i];
    // 畫面縮到面板的寬度；解析度夠的話不放大（保持清晰）。
    const int width = std::min(availableWidth(), frames.first().width());
    const ScaledFrames& scaled = scaled_[i];
    QPixmap shown;
    if (scaled.width == width && scaled.frames.size() == frames.size()) {
        shown = scaled.frames.at(demoFrame_);
    } else {
        // 平滑的版本在背景做（做好再換上）；現在先用快速縮放頂著，UI 執行緒不等。
        requestScaled(demoStep_, width);
        const QPixmap& frame = frames.at(demoFrame_);
        shown = frame.width() == width ? frame : frame.scaledToWidth(width, Qt::FastTransformation);
    }
    if (demoImage_->height() != shown.height()) demoImage_->setFixedHeight(shown.height());
    demoImage_->setPixmap(shown);
}

// 在背景把這一步的畫面平滑縮到 width（原圖從共用的解碼結果拿）。一次只有一個在做；做完時寬度已經變了就再做一次。
void GuidancePanel::requestScaled(GuideStep step, int width) {
    const std::size_t i = static_cast<std::size_t>(guideStepIndex(step));
    if (scalers_[i] != nullptr) return;
    auto* watcher = new QFutureWatcher<GuideScaledFrames>(this);
    scalers_[i] = watcher;
    connect(watcher, &QFutureWatcherBase::finished, this, [this, step, i, watcher] {
        const GuideScaledFrames result = watcher->result();
        watcher->deleteLater();
        scalers_[i] = nullptr;
        ScaledFrames& scaled = scaled_[i];
        scaled.width = result.width;
        scaled.frames.clear();
        for (const QImage& image : result.frames) scaled.frames.push_back(QPixmap::fromImage(image));
        if (demoStep_ == step) showFrame(demoFrame_);  // 寬度已經不同時 relayoutImage 會再要一次
    });
    watcher->setFuture(QtConcurrent::run([step, width] {
        GuideScaledFrames out;
        out.width = width;
        for (const QImage& image : decodeDemoFrames(step))
            out.frames.push_back(image.width() == width ? image : image.scaledToWidth(width, Qt::SmoothTransformation));
        return out;
    }));
}

void GuidancePanel::advanceFrame() { showFrame(demoFrame_ + 1); }

void GuidancePanel::updatePlayback() {
    // 看得到、而且這一步有畫面才播；單張不必播。
    const bool play = isVisible() && demoFrameCount() > 1;
    if (play && !frameTimer_->isActive()) frameTimer_->start();
    if (!play && frameTimer_->isActive()) frameTimer_->stop();
}

// ---- 讀 ----

QString GuidancePanel::rowTitle(GuideStep step) const { return rows_[static_cast<std::size_t>(guideStepIndex(step))].title->text(); }

QString GuidancePanel::rowNote(GuideStep step) const {
    const Row& row = rows_[static_cast<std::size_t>(guideStepIndex(step))];
    return row.note->isVisibleTo(const_cast<GuidancePanel*>(this)) ? row.note->text() : QString();
}

bool GuidancePanel::isRowCurrent(GuideStep step) const { return hasStatus_ && status_.current == step; }

bool GuidancePanel::isRowDone(GuideStep step) const { return hasStatus_ && status_.isDone(step); }

QSize GuidancePanel::sizeHint() const { return QSize(ThemeTokens::builtin().sizes.dockMinWidth, 480); }

QSize GuidancePanel::minimumSizeHint() const { return QSize(ThemeTokens::builtin().sizes.dockMinWidth, 160); }

// ---- 事件 ----

void GuidancePanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    const std::size_t i = static_cast<std::size_t>(guideStepIndex(demoStep_));
    if (!framesLoaded_[i]) {
        loadFrames(demoStep_);
        showFrame(demoFrame_);
    }
    updatePlayback();
}

void GuidancePanel::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    updatePlayback();
}

void GuidancePanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    relayoutImage();
}

bool GuidancePanel::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonRelease) {
        for (int i = 0; i < kGuideStepCount; ++i) {
            if (rows_[static_cast<std::size_t>(i)].box != watched) continue;
            if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
                showDemo(guideStepAt(i));
                emit stepClicked(guideStepAt(i));
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

}  // namespace aoi::ui
