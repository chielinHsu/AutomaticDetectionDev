#include <QApplication>
#include <QGraphicsScene>
#include <QPainter>
#include <QTest>
#include <algorithm>

#include "TestSupport.h"
#include "presentation/canvas/BoxFrames.h"
#include "presentation/canvas/ImageCanvas.h"
#include "presentation/canvas/OverlayStyle.h"
#include "presentation/canvas/RoiGraphicsItem.h"
#include "presentation/canvas/RoiLayerItem.h"
#include "presentation/dev/FakeScene.h"
#include "presentation/theme/ThemeTokens.h"

using namespace aoi::ui;
using aoi::test::toView;

namespace {

const ThemeTokens& tokens() { return ThemeTokens::builtin(); }

// 把一個元件快照用 RoiLayerItem 畫到 QImage（可指定 devicePixelRatio 與縮放）。像素用來驗證實際畫出的粗細與線型。
QImage renderLayer(const SnapshotPtr& snapshot, const QSizeF& sceneSize, qreal dpr, double scale = 1.0) {
    OverlayStyle style(tokens());
    QGraphicsScene scene;  // 比 style 晚建立、先解構
    auto* layer = new RoiLayerItem(style);
    layer->setBounds(QRectF(QPointF(0, 0), sceneSize));
    layer->setSnapshot(snapshot);
    scene.addItem(layer);
    scene.setSceneRect(QRectF(QPointF(0, 0), sceneSize));
    const QSizeF logical = sceneSize * scale;
    QImage img((logical * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(dpr);
    img.fill(QColor(0x1A, 0x5C, 0x3A));
    QPainter p(&img);
    scene.render(&p, QRectF(QPointF(0, 0), logical), QRectF(QPointF(0, 0), sceneSize));
    return img;
}

SnapshotPtr oneBox(OverlayState state, const QRectF& rect, BoxKind kind = BoxKind::Body) {
    auto s = std::make_shared<ComponentsSnapshot>();
    ComponentView c;
    c.id = 1;
    c.state = state;
    c.outline = rect;
    c.boxes.push_back({rect, kind});
    s->components.push_back(c);
    return s;
}

int countColor(const QImage& img, QPoint from, QPoint step, int n, const QColor& color) {
    int hits = 0;
    for (int i = 0; i < n; ++i) {
        const QPoint p = from + step * i;
        if (img.rect().contains(p) && QColor(img.pixel(p)).rgb() == color.rgb()) ++hits;
    }
    return hits;
}

void setScale(ImageCanvas& c, double scale) {
    c.zoomAt(QPointF(c.viewport()->width() / 2.0, c.viewport()->height() / 2.0), scale / c.viewScale());
}

}  // namespace

class TstItems : public QObject {
    Q_OBJECT
private slots:
    // ---- 疊圖樣式 ----
    void boxPensFollowTheStateLanguage() {
        const ThemeTokens& t = tokens();
        for (int s = 0; s < kOverlayStateCount; ++s) {
            const auto state = static_cast<OverlayState>(s);
            const LineSpec& spec = t.line[static_cast<size_t>(s)];
            const StrokeColors& colors = t.overlay.state[static_cast<size_t>(s)];
            for (BoxKind kind : {BoxKind::Body, BoxKind::Pin, BoxKind::Window}) {
                for (bool selected : {false, true}) {
                    const double expected = (kind == BoxKind::Body ? spec.width : kind == BoxKind::Pin ? spec.width - 0.5 : 1.0) +
                                            (selected ? 1.0 : 0.0);
                    const QPen stroke = OverlayStyle::makeBoxPen(t, state, kind, selected, false, 1.0);
                    const QPen casing = OverlayStyle::makeBoxPen(t, state, kind, selected, true, 1.0);
                    QVERIFY(stroke.isCosmetic());
                    QVERIFY(casing.isCosmetic());
                    QCOMPARE(stroke.widthF(), expected);
                    QCOMPARE(casing.widthF(), expected + t.casingExtraWidth);
                    QCOMPARE(stroke.color(), colors.stroke);
                    QCOMPARE(casing.color(), colors.casing);
                    QCOMPARE(stroke.capStyle(), spec.cap);
                    if (spec.dashPx.isEmpty()) {
                        QCOMPARE(stroke.style(), Qt::SolidLine);
                    } else {
                        // tokens 的虛線長度是像素；QPen 的單位是線寬 → 像素 ÷ 線寬。
                        const QList<qreal> pattern = stroke.dashPattern();
                        QCOMPARE(pattern.size(), spec.dashPx.size());
                        for (qsizetype i = 0; i < pattern.size(); ++i) {
                            if (spec.dashPx.at(i) == 0) {
                                // 零長度的段 Qt 不畫，所以用極小的正值（見 OverlayStyle）；仍然要是正的。
                                QVERIFY(pattern.at(i) > 0 && pattern.at(i) < 0.01);
                            } else {
                                QVERIFY(qFuzzyCompare(pattern.at(i) * stroke.widthF(), spec.dashPx.at(i)));
                            }
                        }
                    }
                }
            }
        }
    }

    void penWidthScalesWithDevicePixelRatio() {
        // 實測：cosmetic 筆的寬度單位是裝置像素。DPR 2 時，裝置無關的 2 px 要畫 4 個裝置像素。
        const ThemeTokens& t = tokens();
        const QPen at1 = OverlayStyle::makeBoxPen(t, OverlayState::Found, BoxKind::Body, false, false, 1.0);
        const QPen at2 = OverlayStyle::makeBoxPen(t, OverlayState::Found, BoxKind::Body, false, false, 2.0);
        QCOMPARE(at2.widthF(), at1.widthF() * 2.0);
        // 虛線以線寬為單位，所以不隨 DPR 變：線寬加倍、單位長度加倍，實際長度正好是 DPR 倍。
        const QPen d1 = OverlayStyle::makeBoxPen(t, OverlayState::Inferred, BoxKind::Body, false, false, 1.0);
        const QPen d2 = OverlayStyle::makeBoxPen(t, OverlayState::Inferred, BoxKind::Body, false, false, 2.0);
        QCOMPARE(d1.dashPattern(), d2.dashPattern());

        OverlayStyle style(t, 1.0);
        style.setDevicePixelRatio(2.0);
        QCOMPARE(style.box(OverlayState::Found, BoxKind::Body, false).stroke.widthF(), at2.widthF());
    }

    void regionAndDefinitionStylesAreNotStateBoxes() {
        const ThemeTokens& t = tokens();
        OverlayStyle style(t);
        QCOMPARE(style.region().stroke.widthF(), t.overlay.regionWidth);
        QCOMPARE(style.region().stroke.style(), Qt::SolidLine);
        QVERIFY(std::abs(style.regionFill().color().alphaF() - t.overlay.regionFillOpacity) < 1e-3);  // QColor 的 alpha 是 16 bit
        QCOMPARE(style.definition(BoxKind::Body).stroke.widthF(), t.overlay.defWidthBody);
        QCOMPARE(style.definition(BoxKind::Pin).stroke.widthF(), t.overlay.defWidthPin);
        QCOMPARE(style.definition(BoxKind::Window).stroke.widthF(), 1.0);
    }

    // ---- 把手（類型編輯器用）：點擊區 24 px，小框把手往外推 ----
    void handleHitAreaIs24PxAroundTheCenter() {
        OverlayStyle style(tokens());
        RoiGraphicsItem item(style);
        item.setDefinition(QRectF(100, 100, 200, 100), BoxKind::Body);  // 螢幕 200×100：不算小框
        const double s = 1.0;
        const auto spots = item.handleSpots(s);
        QCOMPARE(static_cast<int>(spots.size()), 8);
        // 左上角把手的中心在框的左上角。距中心 10 px 命中、13 px 沒命中（點擊區半徑 12）。
        QCOMPARE(item.handleAt(QPointF(110, 100), s), RoiGraphicsItem::Handle::TopLeft);
        QCOMPARE(item.handleAt(QPointF(100, 110), s), RoiGraphicsItem::Handle::TopLeft);
        QCOMPARE(item.handleAt(QPointF(113, 100), s), RoiGraphicsItem::Handle::None);
        QCOMPARE(item.handleAt(QPointF(100, 113), s), RoiGraphicsItem::Handle::None);
        // 框的內部不是把手：留給拖曳移動。
        QCOMPARE(item.handleAt(QPointF(200, 150), s), RoiGraphicsItem::Handle::None);
        // 邊的把手。
        QCOMPARE(item.handleAt(QPointF(200, 100), s), RoiGraphicsItem::Handle::Top);
        QCOMPARE(item.handleAt(QPointF(300, 150), s), RoiGraphicsItem::Handle::Right);
    }

    void handleHitAreaStaysInScreenPixelsWhenZoomed() {
        OverlayStyle style(tokens());
        RoiGraphicsItem item(style);
        item.setDefinition(QRectF(0, 0, 400, 400), BoxKind::Body);
        // 縮放 0.5：螢幕上 24 px ＝ 場景 48 單位，半徑 24。距離 20 場景單位（螢幕 10 px）命中、26（13 px）沒命中。
        QCOMPARE(item.handleAt(QPointF(20, 0), 0.5), RoiGraphicsItem::Handle::TopLeft);
        QCOMPARE(item.handleAt(QPointF(26, 0), 0.5), RoiGraphicsItem::Handle::None);
        // 縮放 4：半徑 3 場景單位。
        QCOMPARE(item.handleAt(QPointF(2.5, 0), 4.0), RoiGraphicsItem::Handle::TopLeft);
        QCOMPARE(item.handleAt(QPointF(3.5, 0), 4.0), RoiGraphicsItem::Handle::None);
    }

    void smallBoxesGetFourCornerHandlesPushedOutside() {
        OverlayStyle style(tokens());
        RoiGraphicsItem item(style);
        item.setDefinition(QRectF(100, 100, 30, 20), BoxKind::Pin);  // 螢幕 30×20 < 72
        const auto spots = item.handleSpots(1.0);
        QCOMPARE(static_cast<int>(spots.size()), 4);
        auto find = [&](RoiGraphicsItem::Handle h) {
            for (const auto& sp : spots)
                if (sp.handle == h) return sp.center;
            return QPointF();
        };
        QCOMPARE(find(RoiGraphicsItem::Handle::TopLeft), QPointF(88, 88));        // 往外推 12 px
        QCOMPARE(find(RoiGraphicsItem::Handle::BottomRight), QPointF(142, 132));
        // 點擊區不互相重疊（相鄰兩個把手的中心距離 > 24），框內部沒有把手。
        QVERIFY(find(RoiGraphicsItem::Handle::TopRight).x() - find(RoiGraphicsItem::Handle::TopLeft).x() > 24.0);
        QCOMPARE(item.handleAt(QPointF(115, 110), 1.0), RoiGraphicsItem::Handle::None);
        QCOMPARE(item.handleAt(QPointF(90, 90), 1.0), RoiGraphicsItem::Handle::TopLeft);

        // 72 px 是分界：邊長剛好 72 螢幕像素就是 8 個把手。
        item.setDefinition(QRectF(0, 0, 72, 72), BoxKind::Body);
        QCOMPARE(static_cast<int>(item.handleSpots(1.0).size()), 8);
        item.setDefinition(QRectF(0, 0, 71, 72), BoxKind::Body);
        QCOMPARE(static_cast<int>(item.handleSpots(1.0).size()), 4);
    }

    void draftBoundsLeaveRoomForLabelsAndHandlesWhenZoomedOut() {
        // 縮得很小時，標籤與把手的螢幕尺寸換算成場景單位會變大；邊界要跟著長，否則會被裁掉。
        OverlayStyle style(tokens());
        RoiGraphicsItem item(style);
        item.setDraft(QRectF(100, 100, 60, 32), DraftMode::Sample);
        const QRectF at1 = item.boundingRect();
        item.setViewScale(0.05);
        const QRectF atFar = item.boundingRect();
        QVERIFY(atFar.width() > at1.width());
        QVERIFY(atFar.contains(QRectF(100, 100, 60, 32)));
    }

    // ---- 批次繪製的層級簡化與裁切 ----
    void layerLodSkipsTinyPinsAndWindowsButNeverTheBody() {
        aoi::dev::FakeSceneOptions o;
        o.width = 1200;
        o.height = 800;
        o.boxes = 41;  // 一顆 IC：1 主體 + 20 Pin + 20 窗
        auto snapshot = aoi::dev::makeFakeComponents(o);
        QCOMPARE(static_cast<int>(snapshot->boxCount()), 41);

        double minPin = 1e9, minWin = 1e9;
        for (const BoxView& b : snapshot->components.front().boxes) {
            const double side = std::min(b.rect.width(), b.rect.height());
            if (b.kind == BoxKind::Pin) minPin = std::min(minPin, side);
            if (b.kind == BoxKind::Window) minWin = std::min(minWin, side);
        }

        ImageCanvas canvas(tokens());
        canvas.resize(900, 600);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        auto board = aoi::dev::makeFakeBoard(o.width, o.height);
        canvas.setImage(board.toQImage());
        canvas.showComponents(snapshot);
        const QPointF center = snapshot->components.front().outline.center();

        // 小：Pin 與窗的短邊都 < 3 螢幕像素 → 40 個框略過，只畫主體。
        setScale(canvas, 2.5 / std::max(minPin, minWin));
        canvas.centerOn(center);
        canvas.viewport()->repaint();
        const auto small = canvas.layerItem()->lastPaintStats();
        QCOMPARE(small.boxesDrawn, 1);
        QCOMPARE(small.boxesSkippedByLod, 40);

        // 大：全部 ≥ 3 螢幕像素 → 41 個全畫。
        setScale(canvas, 3.5 / std::min(minPin, minWin));
        canvas.centerOn(center);
        canvas.viewport()->repaint();
        const auto big = canvas.layerItem()->lastPaintStats();
        QCOMPARE(big.boxesDrawn, 41);
        QCOMPARE(big.boxesSkippedByLod, 0);
    }

    void layerOnlyDrawsBoxesInsideTheExposedArea() {
        aoi::dev::FakeSceneOptions o;
        o.boxes = 5000;
        auto snapshot = aoi::dev::makeFakeComponents(o);
        ImageCanvas canvas(tokens());
        canvas.resize(1000, 700);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        auto board = aoi::dev::makeFakeBoard(o.width, o.height);
        canvas.setImage(board.toQImage());
        canvas.showComponents(snapshot);

        canvas.viewport()->repaint();  // 適配視窗：整張板子都看得到
        const auto fitted = canvas.layerItem()->lastPaintStats();
        QCOMPARE(fitted.boxesConsidered, 5000);  // 全部的框都在桶裡
        QVERIFY(fitted.boxesDrawn > 500);

        // 放大到 16 倍，對準左上角：只有極少數的框在畫面內。
        setScale(canvas, 16.0);
        canvas.centerOn(QPointF(30, 30));
        canvas.viewport()->repaint();
        const auto zoomed = canvas.layerItem()->lastPaintStats();
        QVERIFY2(zoomed.boxesDrawn < 60, qPrintable(QString::number(zoomed.boxesDrawn)));
        QVERIFY(zoomed.boxesDrawn < fitted.boxesDrawn);
    }

    void layerShowsLabelsOnlyWhenTheComponentIsWideEnough() {
        aoi::dev::FakeSceneOptions o;
        o.width = 1200;
        o.height = 800;
        o.boxes = 41;
        auto snapshot = aoi::dev::makeFakeComponents(o);
        const ComponentView& ic = snapshot->components.front();
        ImageCanvas canvas(tokens());
        canvas.resize(900, 600);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        canvas.setImage(aoi::dev::makeFakeBoard(o.width, o.height).toQImage());
        canvas.showComponents(snapshot);

        setScale(canvas, 40.0 / ic.outline.width());  // 外框 40 螢幕像素 < 48
        canvas.centerOn(ic.outline.center());
        canvas.viewport()->repaint();
        QCOMPARE(canvas.layerItem()->lastPaintStats().labelsDrawn, 0);
        setScale(canvas, 60.0 / ic.outline.width());  // 60 螢幕像素 ≥ 48
        canvas.centerOn(ic.outline.center());
        canvas.viewport()->repaint();
        QCOMPARE(canvas.layerItem()->lastPaintStats().labelsDrawn, 1);
    }

    // ---- 填色細矩形的畫法（BoxFrames）----
    void frameThicknessIsAnIntegerNumberOfPixels() {
        QCOMPARE(BoxFrames::thickness(1.0), 1);
        QCOMPARE(BoxFrames::thickness(1.5), 1);  // tokens 的 Pin ＝ 狀態寬度 − 0.5；整數像素表示不了，取小的
        QCOMPARE(BoxFrames::thickness(2.0), 2);
        QCOMPARE(BoxFrames::thickness(2.5), 2);
        QCOMPARE(BoxFrames::thickness(3.0), 3);
        QCOMPARE(BoxFrames::thickness(4.0), 4);  // 選取：+1
        QCOMPARE(BoxFrames::thickness(0.2), 1);  // 再細也至少 1 px
        OverlayStyle style(tokens());
        QCOMPARE(style.frameThickness(OverlayState::Found, BoxKind::Body, false), 2);
        QCOMPARE(style.frameThickness(OverlayState::Found, BoxKind::Pin, false), 1);
        QCOMPARE(style.frameThickness(OverlayState::Found, BoxKind::Window, false), 1);
        QCOMPARE(style.frameThickness(OverlayState::Sample, BoxKind::Body, false), 3);
        QCOMPARE(style.frameThickness(OverlayState::Found, BoxKind::Body, true), 3);  // 選取 +1
        QVERIFY(style.solid(OverlayState::Sample) && style.solid(OverlayState::Found));
        QVERIFY(!style.solid(OverlayState::Inferred) && !style.solid(OverlayState::Rejected));
    }

    void frameHasCasingOutsideAndInsideTheStroke() {
        // 主體 (20,20,100,60)、粗細 2：描邊在 y=20..21；外框在 y=19 與 y=22（各一條，比描邊每側多 1 px）。
        const QImage img = renderLayer(oneBox(OverlayState::Found, QRectF(20, 20, 100, 60)), QSizeF(140, 100), 1.0);
        const OverlayTokens& ov = tokens().overlay;
        const QColor stroke = ov.state[static_cast<size_t>(stateIndex(OverlayState::Found))].stroke;
        const QColor casing = ov.state[static_cast<size_t>(stateIndex(OverlayState::Found))].casing;
        auto at = [&](int x, int y) { return QColor(img.pixel(x, y)).rgb(); };
        QCOMPARE(at(70, 18), QColor(0x1A, 0x5C, 0x3A).rgb());  // 框外：板子的底色
        QCOMPARE(at(70, 19), casing.rgb());
        QCOMPARE(at(70, 20), stroke.rgb());
        QCOMPARE(at(70, 21), stroke.rgb());
        QCOMPARE(at(70, 22), casing.rgb());
        QCOMPARE(at(70, 23), QColor(0x1A, 0x5C, 0x3A).rgb());  // 框內：不填色，影像看得到
        // 左緣同理。
        QCOMPARE(at(19, 50), casing.rgb());
        QCOMPARE(at(20, 50), stroke.rgb());
        QCOMPARE(at(21, 50), stroke.rgb());
        QCOMPARE(at(22, 50), casing.rgb());
        // 整個框的內部沒有任何非底色的像素（中心點）。
        QCOMPARE(at(70, 50), QColor(0x1A, 0x5C, 0x3A).rgb());
    }

    void frameThicknessFollowsDevicePixelRatio() {
        // 裝置無關像素：DPR 2 時粗細 2 要畫成 4 個裝置像素（實測：cosmetic 筆在 DPR 2 只畫 2 個，所以舊畫法要乘 DPR）。
        const QColor stroke = tokens().overlay.state[static_cast<size_t>(stateIndex(OverlayState::Found))].stroke;
        for (qreal dpr : {1.0, 2.0}) {
            const QImage img = renderLayer(oneBox(OverlayState::Found, QRectF(20, 20, 100, 60)), QSizeF(140, 100), dpr);
            const int strokeRows = countColor(img, QPoint(static_cast<int>(70 * dpr), 0), QPoint(0, 1), img.height(), stroke);
            // 垂直穿過上緣與下緣：每一條描邊的厚度是 2 × DPR，上下各一條。
            QCOMPARE(strokeRows, static_cast<int>(2 * 2 * dpr));
        }
    }

    void dashedStatesStillDrawDashes() {
        // Inferred 是虛線（8 畫 4 空）：沿著上緣掃過，一部分是描邊色、一部分不是；Found 則是連續的。
        const OverlayTokens& ov = tokens().overlay;
        auto edgeStrokePixels = [&](OverlayState state) {
            const QImage img = renderLayer(oneBox(state, QRectF(20, 20, 160, 60)), QSizeF(200, 100), 1.0);
            const QColor stroke = ov.state[static_cast<size_t>(stateIndex(state))].stroke;
            int best = 0;
            for (int y = 18; y <= 23; ++y) best = std::max(best, countColor(img, QPoint(24, y), QPoint(1, 0), 152, stroke));
            return best;
        };
        QCOMPARE(edgeStrokePixels(OverlayState::Found), 152);  // 連續
        const int dashed = edgeStrokePixels(OverlayState::Inferred);
        QVERIFY2(dashed > 152 * 0.45 && dashed < 152 * 0.85, qPrintable(QString::number(dashed)));
        const int dotted = edgeStrokePixels(OverlayState::Rejected);
        QVERIFY2(dotted > 10 && dotted < 152 * 0.6, qPrintable(QString::number(dotted)));
    }

    // ---- 虛線與點線也用填色矩形（S04）----
    void dashSpecsFollowTheTokensAndTheCapStyle() {
        OverlayStyle style(tokens());
        // Inferred：8 畫 4 空、平端點 → 每 12 像素畫 8 個
        const BoxFrames::DashSpec inferred = style.dashSpec(OverlayState::Inferred, BoxKind::Body, false);
        QVERIFY(inferred.valid());
        QCOMPARE(inferred.visible, 8);
        QCOMPARE(inferred.period, 12);
        // Rejected：長度 0、間隔 4、圓端點 → 一個線寬大的點，每 4 像素一個
        const BoxFrames::DashSpec dotted = style.dashSpec(OverlayState::Rejected, BoxKind::Body, false);
        QCOMPARE(dotted.visible, 2);  // 主體線寬 2
        QCOMPARE(dotted.period, 4);
        QCOMPARE(style.dashSpec(OverlayState::Rejected, BoxKind::Pin, false).visible, 1);  // Pin 線寬 1
        // 實線沒有 dash
        QVERIFY(!style.dashSpec(OverlayState::Found, BoxKind::Body, false).valid());
        QVERIFY(!style.dashSpec(OverlayState::Sample, BoxKind::Body, false).valid());
        // 四個狀態都能用填色矩形畫
        for (OverlayState s : {OverlayState::Sample, OverlayState::Found, OverlayState::Inferred, OverlayState::Rejected})
            QVERIFY(style.frameable(s));
    }

    void aDashedFrameIsCutIntoPiecesAlongEachEdge() {
        BoxFrames::Batch batch;
        BoxFrames::appendDashed(batch, QRectF(0, 0, 20, 6), 2, {2, 4});
        // 上、下各 5 個（x ＝ 0、4、8、12、16，各 2 寬）；左、右夾在中間（內高 2）各 1 個。
        QCOMPARE(static_cast<int>(batch.stroke.size()), 12);
        QCOMPARE(batch.stroke.front(), QRect(0, 0, 2, 2));
        QCOMPARE(batch.stroke[4], QRect(16, 0, 2, 2));
        QCOMPARE(batch.stroke[5], QRect(0, 4, 2, 2));   // 下緣從 x ＝ 0 起算
        QCOMPARE(batch.stroke[10], QRect(0, 2, 2, 2));  // 左緣
        QCOMPARE(batch.stroke[11], QRect(18, 2, 2, 2));  // 右緣
        // 外框：每一段每側多 1 px
        QCOMPARE(static_cast<int>(batch.casing.size()), 12);
        for (std::size_t i = 0; i < batch.stroke.size(); ++i) QCOMPARE(batch.casing[i], batch.stroke[i].adjusted(-1, -1, 1, 1));
    }

    void theLastPieceOfAnEdgeIsClippedAndNothingLeavesTheBox() {
        BoxFrames::Batch batch;
        BoxFrames::appendDashed(batch, QRectF(10, 10, 15, 8), 2, {8, 12});  // 上緣：0..8 一段、12..15 被截成 3 寬
        QRect hull;
        for (const QRect& r : batch.stroke) hull = hull.united(r);
        QCOMPARE(hull, QRect(10, 10, 15, 8));  // 描邊沒有超出框
        QCOMPARE(batch.stroke[0], QRect(10, 10, 8, 2));
        QCOMPARE(batch.stroke[1], QRect(22, 10, 3, 2));
    }

    void anInvalidDashFallsBackToASolidFrame() {
        BoxFrames::Batch dashed, solid;
        BoxFrames::appendDashed(dashed, QRectF(0, 0, 20, 10), 2, {});
        BoxFrames::append(solid, QRectF(0, 0, 20, 10), 2);
        QCOMPARE(dashed.stroke, solid.stroke);
        QCOMPARE(dashed.casing, solid.casing);
    }

    void tinyDashedBoxesStayVisible() {
        BoxFrames::Batch batch;
        BoxFrames::appendDashed(batch, QRectF(10, 10, 2, 2), 2, {2, 4});
        QVERIFY(!batch.stroke.empty());
        QRect hull;
        for (const QRect& r : batch.stroke) hull = hull.united(r);
        QCOMPARE(hull, QRect(10, 10, 2, 2));
    }

    void aDottedBoxIsMadeOfSeparateDotsInTheStateColor() {
        // Rejected：沿著上緣每 4 像素一個 2 × 2 的點。取描邊所在的那兩列（y ＝ 20、21）檢查：有點、有間隔，沒有連成線。
        const QColor stroke = tokens().overlay.state[static_cast<size_t>(stateIndex(OverlayState::Rejected))].stroke;
        const QImage img = renderLayer(oneBox(OverlayState::Rejected, QRectF(20, 20, 80, 40)), QSizeF(120, 80), 1.0);
        int runs = 0;
        bool inRun = false;
        for (int x = 20; x < 100; ++x) {
            const bool on = QColor(img.pixel(x, 20)).rgb() == stroke.rgb();
            if (on && !inRun) ++runs;
            inRun = on;
        }
        QCOMPARE(runs, 20);  // 80 像素 ÷ 週期 4 ＝ 20 個點
        // 點的下一列（y ＝ 21）也是同樣的點：2 像素高。
        QCOMPARE(QColor(img.pixel(20, 21)).rgb(), stroke.rgb());
        QVERIFY(QColor(img.pixel(22, 21)).rgb() != stroke.rgb());  // 間隔
        // 框內沒有任何描邊色的像素（中心點）。
        QVERIFY(QColor(img.pixel(60, 40)).rgb() != stroke.rgb());
    }

    void aDashedBoxHasDashesOfTheRightLengthAndGap() {
        const QColor stroke = tokens().overlay.state[static_cast<size_t>(stateIndex(OverlayState::Inferred))].stroke;
        const QImage img = renderLayer(oneBox(OverlayState::Inferred, QRectF(20, 20, 120, 40)), QSizeF(160, 80), 1.0);
        std::vector<int> runLengths;
        int len = 0;
        for (int x = 20; x <= 140; ++x) {
            const bool on = x < 140 && QColor(img.pixel(x, 20)).rgb() == stroke.rgb();
            if (on) ++len;
            else if (len > 0) {
                runLengths.push_back(len);
                len = 0;
            }
        }
        QVERIFY(runLengths.size() >= 9);
        QCOMPARE(runLengths.front(), 8);  // 畫 8
        QCOMPARE(runLengths[1], 8);       // 每 12 像素重複：空 4
    }

    void tinyBoxesStayVisibleAsABlob() {
        // 螢幕上只有 2 × 2 px 的主體框也要看得到（Body 永遠畫）。
        BoxFrames::Batch batch;
        BoxFrames::append(batch, QRectF(10, 10, 2, 2), 2);
        QVERIFY(!batch.stroke.empty());
        QRegion region;
        for (const QRect& r : batch.stroke) region += r;
        QCOMPARE(region.boundingRect(), QRect(10, 10, 2, 2));
        BoxFrames::Batch zero;
        BoxFrames::append(zero, QRectF(10, 10, 0.2, 0.2), 1);  // 比 1 px 還小：至少 1 px
        QVERIFY(!zero.stroke.empty());
        QVERIFY(zero.stroke.front().width() >= 1 && zero.stroke.front().height() >= 1);
    }

    // ---- 點擊判定 ----
    void componentAtPicksTheSmallestContainingComponent() {
        auto snapshot = std::make_shared<ComponentsSnapshot>();
        ComponentView big;
        big.id = 1;
        big.outline = QRectF(0, 0, 100, 100);
        big.boxes.push_back({big.outline, BoxKind::Body});
        ComponentView small;
        small.id = 2;
        small.outline = QRectF(40, 40, 10, 10);
        small.boxes.push_back({small.outline, BoxKind::Body});
        snapshot->components = {big, small};

        OverlayStyle style(tokens());
        RoiLayerItem layer(style);
        layer.setSnapshot(snapshot);
        QCOMPARE(layer.componentAt(QPointF(45, 45)), 2);   // 兩個都包含 → 取面積小的
        QCOMPARE(layer.componentAt(QPointF(10, 10)), 1);
        QCOMPARE(layer.componentAt(QPointF(150, 10)), -1);
        layer.setSnapshot(nullptr);
        QCOMPARE(layer.componentAt(QPointF(10, 10)), -1);
    }

    void selectionSurvivesANewSnapshotAndIgnoresUnknownIds() {
        auto make = [](std::initializer_list<int> ids) {
            auto s = std::make_shared<ComponentsSnapshot>();
            for (int id : ids) {
                ComponentView c;
                c.id = id;
                c.outline = QRectF(id * 10, 0, 8, 8);
                c.boxes.push_back({c.outline, BoxKind::Body});
                s->components.push_back(c);
            }
            return s;
        };
        OverlayStyle style(tokens());
        RoiLayerItem layer(style);
        layer.setSnapshot(make({1, 2, 3}));
        layer.setSelection({2, 99});  // 99 不存在：不當機、不選
        QCOMPARE(layer.selection(), (std::vector<int>{2, 99}));
        layer.setSnapshot(make({2, 3}));  // 換快照後，仍存在的編號維持選取
        QCOMPARE(layer.componentCount(), 2);
        QCOMPARE(layer.boxCount(), 2);
    }
};

QTEST_MAIN(TstItems)
#include "tst_items.moc"
