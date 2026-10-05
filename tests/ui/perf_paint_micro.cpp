// 純 QPainter 的微基準（不用專案程式碼）：一幀的時間花在哪裡？
// 這是 docs/steps/S02-ui-shell/perf.md 裡「為什麼框改成整數像素的填色細矩形」的證據；結果只印出來，沒有預算。
// 執行：build\release\perf_paint_micro.exe -o 檔案.txt,txt（Qt Test 在這台機器不寫 stdout）。
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QTest>
#include <algorithm>
#include <vector>

#include "MeasurementHygiene.h"

namespace {

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

template <class F>
double timeMedianMs(F f, int runs = 25, int warmup = 5) {
    std::vector<double> t;
    for (int i = 0; i < runs + warmup; ++i) {
        QElapsedTimer e;
        e.start();
        f();
        if (i >= warmup) t.push_back(static_cast<double>(e.nsecsElapsed()) / 1e6);
    }
    return median(t);
}

QPen cosmetic(const QColor& c, double w) {
    QPen p(c);
    p.setCosmetic(true);
    p.setWidthF(w);
    p.setJoinStyle(Qt::MiterJoin);
    return p;
}

void report(const char* what, double ms) { qInfo().noquote() << QStringLiteral("%1  %2 ms").arg(QLatin1String(what), -58).arg(ms, 8, 'f', 3); }

}  // namespace

class PerfPaintMicro : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { qInfo().noquote() << "measurement hygiene:" << aoi::test::applyMeasurementHygiene(); }

    void imageIntoViewport() {
        QImage target(1000, 700, QImage::Format_RGB32);
        QImage src(2592, 1944, QImage::Format_RGB32);  // 5 MP
        src.fill(0xFF1A5C3A);
        for (double lod : {0.35, 0.5, 1.0, 2.0}) {
            for (bool smooth : {false, true}) {
                const double ms = timeMedianMs([&] {
                    QPainter p(&target);
                    p.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
                    p.scale(lod, lod);
                    const QRectF s(0, 0, std::min(2592.0, 1000 / lod), std::min(1944.0, 700 / lod));
                    p.drawImage(s, src, s);
                });
                report(qPrintable(QStringLiteral("5 MP image into 1000x700, lod %1, %2").arg(lod).arg(smooth ? "smooth" : "nearest")), ms);
            }
        }
    }

    void twoThousandSixHundredFrames() {
        QImage target(1000, 700, QImage::Format_RGB32);
        std::vector<QRectF> rects;
        for (int i = 0; i < 2600; ++i) rects.emplace_back((i % 50) * 19.0, (i / 50) * 13.0, 15, 9);

        report("A  drawRects, cosmetic pens width 4 + 2 (the first design)", timeMedianMs([&] {
                   QPainter p(&target);
                   p.setBrush(Qt::NoBrush);
                   p.setPen(cosmetic(Qt::black, 4));
                   p.drawRects(rects.data(), static_cast<int>(rects.size()));
                   p.setPen(cosmetic(Qt::green, 2));
                   p.drawRects(rects.data(), static_cast<int>(rects.size()));
               }));
        report("B  one QPainterPath, two strokes", timeMedianMs([&] {
                   QPainter p(&target);
                   p.setBrush(Qt::NoBrush);
                   QPainterPath path;
                   for (const QRectF& r : rects) path.addRect(r);
                   p.setPen(cosmetic(Qt::black, 4));
                   p.drawPath(path);
                   p.setPen(cosmetic(Qt::green, 2));
                   p.drawPath(path);
               }));
        // C：整數像素的填色細矩形（BoxFrames 的畫法）
        std::vector<QRect> casing, stroke;
        auto frame = [](std::vector<QRect>& v, int x, int y, int w, int h, int t) {
            v.emplace_back(x, y, w, t);
            v.emplace_back(x, y + h - t, w, t);
            v.emplace_back(x, y + t, t, h - 2 * t);
            v.emplace_back(x + w - t, y + t, t, h - 2 * t);
        };
        for (const QRectF& r : rects) {
            const int x = qRound(r.x()), y = qRound(r.y()), w = qRound(r.width()), h = qRound(r.height());
            frame(stroke, x, y, w, h, 2);
            frame(casing, x - 1, y - 1, w + 2, h + 2, 4);
        }
        report("C  integer-pixel filled rects, drawRects(QRect*) (BoxFrames)", timeMedianMs([&] {
                   QPainter p(&target);
                   p.setPen(Qt::NoPen);
                   p.setBrush(Qt::black);
                   p.drawRects(casing.data(), static_cast<int>(casing.size()));
                   p.setBrush(Qt::green);
                   p.drawRects(stroke.data(), static_cast<int>(stroke.size()));
               }));
        report("D  same rects, QRectF with antialiasing on", timeMedianMs([&] {
                   std::vector<QRectF> c2(casing.begin(), casing.end()), s2(stroke.begin(), stroke.end());
                   QPainter p(&target);
                   p.setRenderHint(QPainter::Antialiasing, true);
                   p.setPen(Qt::NoPen);
                   p.setBrush(Qt::black);
                   p.drawRects(c2.data(), static_cast<int>(c2.size()));
                   p.setBrush(Qt::green);
                   p.drawRects(s2.data(), static_cast<int>(s2.size()));
               }));
        QVERIFY(true);
    }

    // ADR-012 的證據：5 832 個框適配視窗時的大小（8 × 5 px），畫成點線或虛線。
    // cosmetic 的圓端點點線筆要走筆畫轉路徑；用填色矩形切成小段只要掃描線填充。
    void dashedAndDottedFrames() {
        QImage target(870, 600, QImage::Format_RGB32);
        const int n = 5832;
        std::vector<QRectF> boxes;
        for (int i = 0; i < n; ++i) boxes.emplace_back(10 + (i % 72) * 11.0, 10 + (i / 72) * 7.0, 8.0, 5.0);

        QPen dotted = cosmetic(QColor(255, 120, 110), 2);
        dotted.setCapStyle(Qt::RoundCap);
        dotted.setDashPattern({0.001, 2.0});  // 零長度的線段 Qt 不畫，所以用極小的正值（OverlayStyle 同）
        QPen dottedCasing = cosmetic(Qt::black, 4);
        dottedCasing.setCapStyle(Qt::RoundCap);
        dottedCasing.setDashPattern({0.001, 1.0});
        QPen dashed = cosmetic(QColor(210, 153, 34), 2);
        dashed.setDashPattern({4.0, 2.0});

        const double dottedPen = timeMedianMs([&] {
            QPainter p(&target);
            p.setBrush(Qt::NoBrush);
            p.setPen(dottedCasing);
            p.drawRects(boxes.data(), n);
            p.setPen(dotted);
            p.drawRects(boxes.data(), n);
        }, 15, 3);
        report("E  cosmetic dotted pens (round cap), casing + stroke", dottedPen);
        const double dashedPen = timeMedianMs([&] {
            QPainter p(&target);
            p.setBrush(Qt::NoBrush);
            p.setPen(dashed);
            p.drawRects(boxes.data(), n);
            p.drawRects(boxes.data(), n);
        }, 15, 3);
        report("F  cosmetic dashed pens (flat cap), casing + stroke", dashedPen);

        // 填色矩形：每個框的四條邊依「點 2 px、週期 4 px」切成小方塊（BoxFrames::appendDashed 的做法）。
        std::vector<QRect> dots, dotCasing;
        auto edge = [](std::vector<QRect>& stroke, std::vector<QRect>& casing, int x, int y, int len, bool horizontal) {
            for (int o = 0; o < len; o += 4) {
                const int l = std::min(2, len - o);
                const QRect r = horizontal ? QRect(x + o, y, l, 2) : QRect(x, y + o, 2, l);
                stroke.push_back(r);
                casing.push_back(r.adjusted(-1, -1, 1, 1));
            }
        };
        for (const QRectF& b : boxes) {
            const int x = qRound(b.left()), y = qRound(b.top()), w = qRound(b.width()), h = qRound(b.height());
            edge(dots, dotCasing, x, y, w, true);
            edge(dots, dotCasing, x, y + h - 2, w, true);
            edge(dots, dotCasing, x, y + 2, h - 4, false);
            edge(dots, dotCasing, x + w - 2, y + 2, h - 4, false);
        }
        report("G  integer-pixel filled rects for the same dots", timeMedianMs([&] {
                   QPainter p(&target);
                   p.setPen(Qt::NoPen);
                   p.setBrush(Qt::black);
                   p.drawRects(dotCasing.data(), static_cast<int>(dotCasing.size()));
                   p.setBrush(QColor(255, 120, 110));
                   p.drawRects(dots.data(), static_cast<int>(dots.size()));
               }, 15, 3));
        qInfo().noquote() << QStringLiteral("rects per box: %1 stroke + %2 casing").arg(double(dots.size()) / n, 0, 'f', 1).arg(double(dotCasing.size()) / n, 0, 'f', 1);
        QVERIFY(true);
    }
};

QTEST_MAIN(PerfPaintMicro)
#include "perf_paint_micro.moc"
