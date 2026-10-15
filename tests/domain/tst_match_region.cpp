// 比對區（ADR-026）：遮罩 ⇄ runs、類型座標（µm）⇄ 影像像素的換算、Otsu 擷取最近的亮塊、外擴。
#include <QtTest>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>

#include "domain/feature_extract.h"
#include "domain/match_region.h"

using namespace aoi;

namespace {

// Gray8 的影像：底色 background，幾個亮的矩形。
struct Gray8Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;

    Gray8Image(int w, int h, std::uint8_t background) : width(w), height(h), pixels(static_cast<std::size_t>(w) * h, background) {}
    void fill(int x, int y, int w, int h, std::uint8_t v) {
        for (int j = y; j < y + h; ++j)
            for (int i = x; i < x + w; ++i) pixels[static_cast<std::size_t>(j) * width + i] = v;
    }
    ImageView view() const {
        ImageView v;
        v.data = pixels.data();
        v.width = width;
        v.height = height;
        v.strideBytes = width;
        v.format = PixelFormat::Gray8;
        return v;
    }
};

PixelMask maskOf(int x, int y, int w, int h, const std::vector<int>& ones) {
    PixelMask m;
    m.x = x;
    m.y = y;
    m.width = w;
    m.height = h;
    m.bits.assign(static_cast<std::size_t>(w) * h, 0);
    for (int k : ones) m.bits[static_cast<std::size_t>(k)] = 1;
    return m;
}

}  // namespace

class TstMatchRegion : public QObject {
    Q_OBJECT

private slots:
    void runsRoundTripExactly() {
        // 3×3：中間一列全滿、四個角各一點。
        const PixelMask m = maskOf(10, 20, 3, 3, {0, 2, 3, 4, 5, 6, 8});
        const MatchMask runs = runsFromMask(m);
        QCOMPARE(runs.width, 3);
        QCOMPARE(runs.height, 3);
        QCOMPARE(static_cast<int>(runs.runs.size()), 5);  // (0,0..1) (0,2..3) (1,0..3) (2,0..1) (2,2..3)
        const PixelMask back = maskFromRuns(runs, 10, 20);
        QCOMPARE(back.bits, m.bits);
        QCOMPARE(back.count(), std::size_t{7});
        QVERIFY(back.at(10, 20));
        QVERIFY(!back.at(11, 20));
        QVERIFY(!back.at(9, 20));  // 遮罩外
    }

    void invalidRunsAreIgnored() {
        MatchMask runs;
        runs.width = 4;
        runs.height = 2;
        runs.runs = {{0, 1, 3}, {5, 0, 4}, {1, -2, 9}};  // 第 2 段超出列；第 3 段裁到格子內
        const PixelMask m = maskFromRuns(runs, 0, 0);
        QCOMPARE(m.count(), std::size_t{2 + 4});
    }

    void defaultSettingsGiveTheWholeBody() {
        const MatchSpec spec = matchSpecFor(MatchSettings{}, {100.0, 50.0}, 2.5);
        QVERIFY(spec.isDefault());
        MatchSettings edges;
        edges.mode = MatchMode::Edges;
        const MatchSpec e = matchSpecFor(edges, {100.0, 50.0}, 2.5);
        QVERIFY(!e.isDefault());
        QVERIFY(!e.rect);
        QVERIFY(e.mask.empty());
    }

    void aRectKeepsItsOffsetFromTheBodyCenter() {
        // 主體中心 (100, 50)，比對區偏右下 (12, −7)、20 × 10。
        const Rect region{102.0, 38.0, 20.0, 10.0};
        for (double um : {1.0, 3.5}) {
            const MatchSettings s = matchSettingsFromPixels(region, nullptr, MatchMode::Intensity, {100.0, 50.0}, um);
            QVERIFY(s.region);
            QCOMPARE(s.region->centerXUm, 12.0 * um);
            QCOMPARE(s.region->centerYUm, -7.0 * um);
            const MatchSpec same = matchSpecFor(s, {100.0, 50.0}, um);
            QVERIFY(same.rect);
            QCOMPARE(same.rect->x, region.x);
            QCOMPARE(same.rect->y, region.y);
            QCOMPARE(same.rect->w, region.w);
            // 樣本移動：比對區跟著主體中心走。
            const MatchSpec moved = matchSpecFor(s, {300.5, 80.0}, um);
            QCOMPARE(moved.rect->x, region.x + 200.5);
            QCOMPARE(moved.rect->y, region.y + 30.0);
        }
    }

    void aMaskSurvivesTheRoundTrip() {
        const PixelMask m = maskOf(95, 45, 4, 3, {1, 2, 4, 5, 6, 7, 9, 10});
        const MatchSettings s = matchSettingsFromPixels({}, &m, MatchMode::Edges, {100.0, 50.0}, 2.0);
        QVERIFY(s.region);
        QVERIFY(!s.region->mask.empty());
        QCOMPARE(s.mode, MatchMode::Edges);
        const MatchSpec spec = matchSpecFor(s, {100.0, 50.0}, 2.0);
        QCOMPARE(spec.mask.x, m.x);
        QCOMPARE(spec.mask.y, m.y);
        QCOMPARE(spec.mask.width, m.width);
        QCOMPARE(spec.mask.height, m.height);
        QCOMPARE(spec.mask.bits, m.bits);
        QVERIFY(spec.rect);
        QCOMPARE(spec.rect->w, 4.0);
        QVERIFY(s == matchSettingsFromPixels({}, &m, MatchMode::Edges, {100.0, 50.0}, 2.0));
    }

    void pickTakesTheBlobUnderTheClick() {
        Gray8Image img(60, 40, 20);
        img.fill(10, 10, 8, 5, 200);   // A
        img.fill(30, 12, 6, 6, 210);   // B
        img.fill(55, 30, 3, 3, 250);   // C：在 within 外
        const Rect within{0.0, 0.0, 50.0, 40.0};
        const std::optional<BlobPick> a = pickBrightBlob(img.view(), within, {12.5, 11.5});
        QVERIFY(a);
        QCOMPARE(a->blob.x, 10);
        QCOMPARE(a->blob.y, 10);
        QCOMPARE(a->blob.width, 8);
        QCOMPARE(a->blob.height, 5);
        QCOMPARE(a->blob.count(), std::size_t{40});
        QVERIFY(a->threshold8 >= 20 && a->threshold8 < 200);
    }

    void aClickOnTheBackgroundTakesTheNearestBlob() {
        Gray8Image img(60, 40, 20);
        img.fill(10, 10, 8, 5, 200);
        img.fill(30, 12, 6, 6, 210);
        const std::optional<BlobPick> b = pickBrightBlob(img.view(), {0.0, 0.0, 50.0, 40.0}, {27.0, 15.0});
        QVERIFY(b);
        QCOMPARE(b->blob.x, 30);
        QCOMPARE(b->blob.width, 6);
        QCOMPARE(b->blob.count(), std::size_t{36});
    }

    void aFlatAreaHasNoBlob() {
        Gray8Image img(30, 30, 77);
        QVERIFY(!pickBrightBlob(img.view(), {0.0, 0.0, 30.0, 30.0}, {5.0, 5.0}));
        QVERIFY(!pickBrightBlob(img.view(), {100.0, 100.0, 5.0, 5.0}, {5.0, 5.0}));  // 在影像外
    }

    void twelveBitGray16UsesTheSignificantBits() {
        // 12 bit 的值放在 16 bit 容器：亮塊 3000、底 300。換算錯（當 16 bit）時亮塊只有約 12，Otsu 仍分得開；
        // 這裡驗的是門檻落在 8 bit 的尺度上（300 → 約 19、3000 → 約 187）。
        const int w = 20;
        const int h = 20;
        std::vector<std::uint16_t> px(static_cast<std::size_t>(w) * h, 300);
        for (int y = 5; y < 9; ++y)
            for (int x = 6; x < 12; ++x) px[static_cast<std::size_t>(y) * w + x] = 3000;
        ImageView v;
        v.data = reinterpret_cast<const std::uint8_t*>(px.data());
        v.width = w;
        v.height = h;
        v.strideBytes = w * 2;
        v.format = PixelFormat::Gray16;
        v.significantBits = 12;
        const std::optional<BlobPick> p = pickBrightBlob(v, {0.0, 0.0, 20.0, 20.0}, {7.0, 6.0});
        QVERIFY(p);
        QCOMPARE(p->blob.count(), std::size_t{24});
        QVERIFY(p->threshold8 >= 19 && p->threshold8 < 187);
    }

    void growUsesADiscAndIsClipped() {
        const PixelMask one = maskOf(10, 10, 1, 1, {0});
        const PixelMask grown = growMask(one, 2, {0.0, 0.0, 100.0, 100.0});
        QCOMPARE(grown.count(), std::size_t{13});  // dx² + dy² ≤ 4
        QCOMPARE(grown.x, 8);
        QCOMPARE(grown.width, 5);
        const PixelMask clipped = growMask(one, 2, {10.0, 10.0, 50.0, 50.0});
        QCOMPARE(clipped.x, 10);
        QCOMPARE(clipped.y, 10);
        QCOMPARE(clipped.count(), std::size_t{6});  // 右下的四分之一（含軸）：(0..2,0)、(0..1,1)、(0,2)
        const PixelMask zero = growMask(one, 0, {0.0, 0.0, 100.0, 100.0});
        QCOMPARE(zero.bits, one.bits);
        QVERIFY(growMask(one, 3, {50.0, 50.0, 5.0, 5.0}).empty());  // 完全在裁切範圍外
    }

    void pickingInsideABodyIsFast() {
        // 主體框約 148 × 148（真實影像的端子）：在 UI 執行緒上做，要遠小於一幀。
        Gray8Image img(400, 400, 30);
        for (int y = 0; y < 400; y += 37)
            for (int x = 0; x < 400; x += 37) img.fill(x + 5, y + 5, 25, 18, 220);
        const auto t0 = std::chrono::steady_clock::now();
        const std::optional<BlobPick> p = pickBrightBlob(img.view(), {100.0, 100.0, 148.0, 148.0}, {160.0, 160.0});
        QVERIFY(p);
        const PixelMask g = growMask(p->blob, 10, {100.0, 100.0, 148.0, 148.0});
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        QVERIFY(!g.empty());
        qInfo("pickBrightBlob + growMask(10) on 148x148: %.3f ms", ms);
        QVERIFY2(ms < 50.0, qPrintable(QString::number(ms)));
    }
};

QTEST_APPLESS_MAIN(TstMatchRegion)
#include "tst_match_region.moc"
