#include <QApplication>
#include <QFocusEvent>
#include <QSignalSpy>
#include <QTest>
#include <cmath>
#include <memory>

#include "TestSupport.h"
#include "domain/image_view.h"
#include "presentation/canvas/ImageCanvas.h"
#include "presentation/dev/FakeScene.h"
#include "presentation/theme/ThemeTokens.h"

using namespace aoi::ui;
using namespace aoi::test;

namespace {

const ThemeTokens& tokens() { return ThemeTokens::builtin(); }

struct Fixture {
    ImageCanvas canvas{tokens()};
    aoi::dev::FakeBoard board;

    Fixture(int w = 1200, int h = 800, QSize viewSize = QSize(1000, 700)) {
        canvas.resize(viewSize);
        canvas.show();
        Q_UNUSED(QTest::qWaitForWindowExposed(&canvas));
        board = aoi::dev::makeFakeBoard(w, h);
        canvas.setImage(board.toQImage());
    }
    QPointF viewCenter() const { return QPointF(canvas.viewport()->width() / 2.0, canvas.viewport()->height() / 2.0); }
};

}  // namespace

class TstCanvas : public QObject {
    Q_OBJECT
private slots:
    void fitCentersTheImageWithTheTokenMargin() {
        Fixture f;
        QVERIFY(f.canvas.isFitted());
        const double vw = f.canvas.viewport()->width(), vh = f.canvas.viewport()->height();
        const double margin = tokens().interaction.fitMarginPx;
        const double expected = std::min((vw - 2 * margin) / 1200.0, (vh - 2 * margin) / 800.0);
        QVERIFY2(std::abs(f.canvas.viewScale() - expected) < 1e-9, "fit scale");
        // 影像中心在視埠中心（捲動是整數像素，誤差 ≤ 1 px）。
        const QPointF imageCenter = toView(f.canvas, QPointF(600, 400));
        QVERIFY(std::abs(imageCenter.x() - vw / 2.0) <= 1.0);
        QVERIFY(std::abs(imageCenter.y() - vh / 2.0) <= 1.0);
        // 影像整個在邊界以內。
        const QPointF tl = toView(f.canvas, QPointF(0, 0)), br = toView(f.canvas, QPointF(1200, 800));
        QVERIFY(tl.x() >= margin - 1 && tl.y() >= margin - 1);
        QVERIFY(br.x() <= vw - margin + 1 && br.y() <= vh - margin + 1);
    }

    void wheelZoomsAboutTheCursorAndTheImagePointDoesNotMove() {
        Fixture f;
        const QList<QPointF> cursors = {QPointF(500, 350), QPointF(120, 90), QPointF(880, 610), QPointF(500, 20),
                                        QPointF(3, 690)};
        for (const QPointF& cursor : cursors) {
            f.canvas.fit();
            for (int notches : {1, 3, -2, 5, -4, 2}) {
                const QPointF before = toScene(f.canvas, cursor);
                const double scaleBefore = f.canvas.viewScale();
                sendWheel(f.canvas.viewport(), cursor, notches);
                const QPointF after = toView(f.canvas, before);
                QVERIFY2(std::abs(after.x() - cursor.x()) <= 1.0 && std::abs(after.y() - cursor.y()) <= 1.0,
                         qPrintable(QStringLiteral("drift at cursor (%1,%2), notches %3: now (%4,%5)")
                                        .arg(cursor.x()).arg(cursor.y()).arg(notches).arg(after.x()).arg(after.y())));
                const double ratio = f.canvas.viewScale() / scaleBefore;
                QVERIFY2(std::abs(ratio - std::pow(1.1, notches)) < 1e-9, "each notch is a factor of 1.1");
            }
        }
        QVERIFY(!f.canvas.isFitted());
    }

    void zoomIsClampedToTheTokenRange() {
        Fixture f;
        for (int i = 0; i < 80; ++i) sendWheel(f.canvas.viewport(), f.viewCenter(), 1);
        QCOMPARE(f.canvas.zoomPercent(), tokens().interaction.zoomMaxPercent);
        for (int i = 0; i < 200; ++i) sendWheel(f.canvas.viewport(), f.viewCenter(), -1);
        QCOMPARE(f.canvas.zoomPercent(), tokens().interaction.zoomMinPercent);
    }

    void zoomChangedIsReportedInPercent() {
        Fixture f;
        QSignalSpy spy(&f.canvas, &ImageCanvas::zoomChanged);
        sendWheel(f.canvas.viewport(), f.viewCenter(), 2);
        QVERIFY(spy.count() >= 1);
        QCOMPARE(spy.last().at(0).toDouble(), f.canvas.zoomPercent());
    }

    void middleButtonDragPansByTheDragDelta() {
        Fixture f;
        sendWheel(f.canvas.viewport(), f.viewCenter(), 6);  // 放大，才有平移空間
        const QPointF p = QPointF(600, 400);
        const QPointF before = toView(f.canvas, p);
        press(f.canvas.viewport(), QPointF(500, 300), Qt::MiddleButton);
        move(f.canvas.viewport(), QPointF(530, 320), Qt::MiddleButton);
        move(f.canvas.viewport(), QPointF(560, 340), Qt::MiddleButton);
        release(f.canvas.viewport(), QPointF(560, 340), Qt::MiddleButton);
        const QPointF after = toView(f.canvas, p);
        QVERIFY2(std::abs((after.x() - before.x()) - 60.0) <= 1.0, qPrintable(QString::number(after.x() - before.x())));
        QVERIFY2(std::abs((after.y() - before.y()) - 40.0) <= 1.0, qPrintable(QString::number(after.y() - before.y())));
        QVERIFY(!f.canvas.isFitted());
    }

    void spaceAndLeftDragPansToo() {
        Fixture f;
        sendWheel(f.canvas.viewport(), f.viewCenter(), 6);
        const QPointF p(600, 400);
        const QPointF before = toView(f.canvas, p);
        sendKey(&f.canvas, Qt::Key_Space, QEvent::KeyPress);
        QCOMPARE(f.canvas.viewport()->cursor().shape(), Qt::OpenHandCursor);
        press(f.canvas.viewport(), QPointF(500, 300));
        QCOMPARE(f.canvas.viewport()->cursor().shape(), Qt::ClosedHandCursor);
        move(f.canvas.viewport(), QPointF(470, 330));
        release(f.canvas.viewport(), QPointF(470, 330));
        sendKey(&f.canvas, Qt::Key_Space, QEvent::KeyRelease);
        const QPointF after = toView(f.canvas, p);
        QVERIFY(std::abs((after.x() - before.x()) + 30.0) <= 1.0);
        QVERIFY(std::abs((after.y() - before.y()) - 30.0) <= 1.0);
        QCOMPARE(f.canvas.viewport()->cursor().shape(), Qt::ArrowCursor);  // 選取工具的游標
    }

    void spaceDragDoesNotDrawBoxesWithTheSampleTool() {
        Fixture f;
        RecordingController rec;
        f.canvas.setController(&rec);
        f.canvas.setInteraction(ToolId::Sample);
        sendKey(&f.canvas, Qt::Key_Space, QEvent::KeyPress);
        press(f.canvas.viewport(), QPointF(300, 300));
        move(f.canvas.viewport(), QPointF(400, 380));
        release(f.canvas.viewport(), QPointF(400, 380));
        sendKey(&f.canvas, Qt::Key_Space, QEvent::KeyRelease);
        QVERIFY(rec.regions.empty());
        QVERIFY(!f.canvas.draftItem()->isVisible());
    }

    void losingFocusWhileSpaceIsHeldResetsTheCursor() {
        Fixture f;
        sendKey(&f.canvas, Qt::Key_Space, QEvent::KeyPress);
        QCOMPARE(f.canvas.viewport()->cursor().shape(), Qt::OpenHandCursor);
        QFocusEvent out(QEvent::FocusOut, Qt::OtherFocusReason);
        QApplication::sendEvent(&f.canvas, &out);
        QCOMPARE(f.canvas.viewport()->cursor().shape(), Qt::ArrowCursor);
    }

    void fitKeepsTheViewFittedAcrossResizeUntilTheUserZooms() {
        Fixture f;
        f.canvas.resize(800, 500);
        QTest::qWait(10);
        QVERIFY(f.canvas.isFitted());
        const double vw = f.canvas.viewport()->width(), vh = f.canvas.viewport()->height();
        const double expected = std::min((vw - 48) / 1200.0, (vh - 48) / 800.0);
        QVERIFY(std::abs(f.canvas.viewScale() - expected) < 1e-9);

        sendWheel(f.canvas.viewport(), f.viewCenter(), 3);
        const double zoomed = f.canvas.viewScale();
        f.canvas.resize(900, 600);
        QTest::qWait(10);
        QVERIFY(!f.canvas.isFitted());
        QCOMPARE(f.canvas.viewScale(), zoomed);  // 使用者縮放過，縮放視窗不改變縮放

        f.canvas.fit();
        QVERIFY(f.canvas.isFitted());
    }

    // ---- 適配寬度 ----
    void fitWidthFillsTheWidthAndKeepsTheCentreRow() {
        Fixture f(1200, 1200);  // 正方形影像、橫長的視窗（1000 × 700）：適配視窗只用到高度，左右留白
        const double vw = f.canvas.viewport()->width(), vh = f.canvas.viewport()->height();
        const double margin = tokens().interaction.fitMarginPx;
        f.canvas.fitWidth();
        QVERIFY(f.canvas.fitMode() == ImageCanvas::FitMode::Width);
        QVERIFY(!f.canvas.isFitted());  // isFitted 只代表「適配視窗」
        QVERIFY2(std::abs(f.canvas.viewScale() - (vw - 2 * margin) / 1200.0) < 1e-9, "fit-width scale");
        // 左右各留一個邊界（捲動是整數像素，誤差 ≤ 1 px）。
        QVERIFY(std::abs(toView(f.canvas, QPointF(0, 600)).x() - margin) <= 1.0);
        QVERIFY(std::abs(toView(f.canvas, QPointF(1200, 600)).x() - (vw - margin)) <= 1.0);
        // 從適配視窗切過來：原本在中央的影像中線還在中央，上下超出視窗。
        QVERIFY(std::abs(toView(f.canvas, QPointF(600, 600)).y() - vh / 2.0) <= 1.0);
        QVERIFY(toView(f.canvas, QPointF(600, 0)).y() < 0);
        QVERIFY(toView(f.canvas, QPointF(600, 1200)).y() > vh);
    }

    void fitWidthNeverShowsSpaceAboveTheTopOrBelowTheBottom() {
        Fixture f(1200, 1200);
        const double vh = f.canvas.viewport()->height();
        const double margin = tokens().interaction.fitMarginPx;
        // 正在看影像最上方（在左上角放大）時適配寬度：上緣停在上邊界，不會露出影像上面的空白。
        for (int i = 0; i < 8; ++i) sendWheel(f.canvas.viewport(), QPointF(5, 5), 1);
        f.canvas.fitWidth();
        QVERIFY2(std::abs(toView(f.canvas, QPointF(600, 0)).y() - margin) <= 1.0,
                 qPrintable(QString::number(toView(f.canvas, QPointF(600, 0)).y())));
        // 最下方也一樣：下緣停在下邊界。
        f.canvas.fit();
        for (int i = 0; i < 8; ++i) sendWheel(f.canvas.viewport(), QPointF(5, vh - 5), 1);
        f.canvas.fitWidth();
        QVERIFY2(std::abs(toView(f.canvas, QPointF(600, 1200)).y() - (vh - margin)) <= 1.0,
                 qPrintable(QString::number(toView(f.canvas, QPointF(600, 1200)).y())));
    }

    void fitWidthCentresAnImageThatIsWiderThanTheWindowVertically() {
        Fixture f(1200, 300);  // 比視窗扁：寬度撐滿之後，高度還放得下
        const double vw = f.canvas.viewport()->width(), vh = f.canvas.viewport()->height();
        const double margin = tokens().interaction.fitMarginPx;
        sendWheel(f.canvas.viewport(), QPointF(100, 100), 4);  // 先縮放到偏離中心的地方
        f.canvas.fitWidth();
        QVERIFY(std::abs(f.canvas.viewScale() - (vw - 2 * margin) / 1200.0) < 1e-9);
        QVERIFY(std::abs(toView(f.canvas, QPointF(600, 150)).y() - vh / 2.0) <= 1.0);
    }

    void fitWidthFollowsResizeWithoutDriftingUntilTheUserZooms() {
        Fixture f(1200, 1200);
        const double margin = tokens().interaction.fitMarginPx;
        f.canvas.fitWidth();
        const double row = toScene(f.canvas, f.viewCenter()).y();  // 視窗中央的影像列（影像中線）
        // 拉大縮小幾次：縮放跟著寬度走，中央那一列不能每次 resize 就偏一點。
        for (const QSize size : {QSize(800, 500), QSize(1100, 650), QSize(900, 560), QSize(1000, 700)}) {
            f.canvas.resize(size);
            QTest::qWait(10);
            QVERIFY(f.canvas.fitMode() == ImageCanvas::FitMode::Width);
            const double vw = f.canvas.viewport()->width();
            QVERIFY(std::abs(f.canvas.viewScale() - (vw - 2 * margin) / 1200.0) < 1e-9);
            const double now = toScene(f.canvas, f.viewCenter()).y();
            QVERIFY2(std::abs(now - row) <= 2.0, qPrintable(QStringLiteral("centre row %1, was %2").arg(now).arg(row)));
        }
        sendWheel(f.canvas.viewport(), f.viewCenter(), 2);
        QVERIFY(f.canvas.fitMode() == ImageCanvas::FitMode::None);
        const double zoomed = f.canvas.viewScale();
        f.canvas.resize(900, 600);
        QTest::qWait(10);
        QCOMPARE(f.canvas.viewScale(), zoomed);  // 使用者縮放過，縮放視窗不改變縮放
    }

    void panningEndsFitWidthToo() {
        Fixture f(1200, 1200);
        f.canvas.fitWidth();
        f.canvas.panBy(QPoint(0, 40));
        QVERIFY(f.canvas.fitMode() == ImageCanvas::FitMode::None);
    }

    void loadingAnImageGoesBackToFitWindow() {
        Fixture f(1200, 1200);
        f.canvas.fitWidth();
        f.canvas.setImage(f.board.toQImage());
        QVERIFY(f.canvas.fitMode() == ImageCanvas::FitMode::Window);
        QVERIFY(f.canvas.isFitted());
    }

    void fitWidthIsClampedToTheZoomRange() {
        Fixture f(20, 20);  // 很小的影像：撐滿寬度要放大約 4 760%，限制在 3 200%
        f.canvas.fitWidth();
        QCOMPARE(f.canvas.zoomPercent(), tokens().interaction.zoomMaxPercent);
    }

    void cursorMovesReportImageCoordinates() {
        Fixture f;
        QSignalSpy spy(&f.canvas, &ImageCanvas::cursorMoved);
        const QPointF scenePoint(300, 200);
        move(f.canvas.viewport(), toView(f.canvas, scenePoint), Qt::NoButton);
        QCOMPARE(spy.count(), 1);
        const QPointF reported = spy.at(0).at(0).toPointF();
        QVERIFY(std::abs(reported.x() - 300) < 1e-6 && std::abs(reported.y() - 200) < 1e-6);
        QCOMPARE(spy.at(0).at(1).toBool(), true);
        move(f.canvas.viewport(), QPointF(2, 2), Qt::NoButton);  // 影像外（邊界 24 px 以內）
        QCOMPARE(spy.at(1).at(1).toBool(), false);
    }

    void noImageMeansNoImageInteractions() {
        ImageCanvas canvas(tokens());
        canvas.resize(600, 400);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        RecordingController rec;
        canvas.setController(&rec);
        canvas.setInteraction(ToolId::Sample);
        press(canvas.viewport(), QPointF(100, 100));
        move(canvas.viewport(), QPointF(200, 200));
        release(canvas.viewport(), QPointF(200, 200));
        QVERIFY(rec.regions.empty());
        QVERIFY(!canvas.hasImage());
        canvas.fit();  // 沒有影像時不當機
        canvas.fitWidth();
        sendWheel(canvas.viewport(), QPointF(100, 100), 3);
    }

    // ---- 零複製 ----
    void imageViewIsWrappedWithoutCopying() {
        Fixture f(320, 240);
        const aoi::ImageView view = f.board.view();
        const QImage wrapped = ImageItem::wrap(view);
        QVERIFY(!wrapped.isNull());
        QCOMPARE(static_cast<const void*>(wrapped.constBits()), static_cast<const void*>(f.board.pixels->data()));
        QCOMPARE(wrapped.bytesPerLine(), static_cast<qsizetype>(view.strideBytes));
        QCOMPARE(wrapped.width(), 320);
        QCOMPARE(wrapped.height(), 240);
        // 複製一份 QImage（隱式共享）仍然是同一塊記憶體。
        const QImage again = wrapped;
        QCOMPARE(static_cast<const void*>(again.constBits()), static_cast<const void*>(f.board.pixels->data()));
    }

    void wrappedBufferLivesAsLongAsAnyQImageUsesIt() {
        std::weak_ptr<const void> watch;
        QImage keeper;
        {
            aoi::dev::FakeBoard board = aoi::dev::makeFakeBoard(64, 48);
            watch = board.view().owner;
            keeper = ImageItem::wrap(board.view());
        }  // board（也就是 pixels 的另一個持有者）離開了
        QVERIFY2(!watch.expired(), "the QImage must keep the buffer alive");
        QCOMPARE(keeper.width(), 64);
        QVERIFY(qGray(keeper.pixel(10, 10)) > 0);  // 還讀得到
        keeper = QImage();
        QVERIFY2(watch.expired(), "the buffer must be released with the last QImage");
    }

    void canvasHoldsTheBufferWhileItShowsIt() {
        std::weak_ptr<const void> watch;
        ImageCanvas canvas(tokens());
        canvas.resize(400, 300);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        {
            aoi::dev::FakeBoard board = aoi::dev::makeFakeBoard(200, 100);
            watch = board.view().owner;
            canvas.showImage(board.view());
        }
        QVERIFY(!watch.expired());
        QVERIFY(canvas.hasImage());
        canvas.setImage(QImage());
        QVERIFY(watch.expired());
        QVERIFY(!canvas.hasImage());
    }

    void invalidViewsAreRejected() {
        aoi::ImageView view;
        QVERIFY(ImageItem::wrap(view).isNull());
        std::vector<std::uint8_t> bytes(100);
        view.data = bytes.data();
        view.width = 10;
        view.height = 10;
        view.strideBytes = 10;  // 小於 width × 4
        view.format = aoi::PixelFormat::Bgrx32;
        QVERIFY(!view.valid());
        QVERIFY(ImageItem::wrap(view).isNull());
    }
};

QTEST_MAIN(TstCanvas)
#include "tst_canvas.moc"
