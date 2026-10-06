// S06：合成板子產生器（tests/support）。可重現（同 seed 同輸出）、標準答案與影像一致、各個效果真的有作用。
#include <QDir>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
#include <cmath>

#include "InfraTestSupport.h"
#include "infrastructure/image_qt/QtImageSource.h"
#include "synthetic_board.h"
#include "synthetic_board_io.h"

using namespace aoi;
using namespace aoi::test;

namespace {

int grayAt(const ImageView& v, double x, double y) {
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    return byteAt(v, ix, iy, 0);
}

SyntheticBoardSpec everyEffect() {
    SyntheticBoardSpec s;
    s.positionJitterPx = 1.5;
    s.missingFraction = 0.2;
    s.glareFraction = 0.2;
    s.blurRadius = 1;
    s.noiseAmplitude = 6;
    s.tiltDeg = 2.5;
    s.seed = 77;
    return s;
}

}  // namespace

class TstSyntheticBoard : public QObject {
    Q_OBJECT

private slots:
    void theSameSeedGivesTheSameBoard() {
        const SyntheticBoard a = makeSyntheticBoard(everyEffect());
        const SyntheticBoard b = makeSyntheticBoard(everyEffect());
        QVERIFY(a.image.valid());
        QVERIFY(a.image.data != b.image.data);  // 兩塊各自的緩衝
        QCOMPARE(pixelHash(a.image), pixelHash(b.image));
        QCOMPARE(groundTruthJson(a), groundTruthJson(b));
        SyntheticBoardSpec other = everyEffect();
        other.seed = 78;
        QVERIFY(pixelHash(makeSyntheticBoard(other).image) != pixelHash(a.image));
    }

    // 沒有傾斜時不需要三角函數：像素雜湊在任何編譯器與平台都相同，這裡鎖住它（改了產生器，這個數字就會變）。
    void anUntiltedBoardHasAFixedHash() {
        SyntheticBoardSpec s = everyEffect();
        s.tiltDeg = 0.0;
        const SyntheticBoard board = makeSyntheticBoard(s);
        const QString hash = QString::number(pixelHash(board.image), 16);
        qInfo("pixel hash of the untilted reference board: %s", qPrintable(hash));
        QCOMPARE(hash, QStringLiteral(AOI_SYNTHETIC_REFERENCE_HASH));
    }

    // 改缺件的比例不改元件的位置（亂數的順序固定，見標頭）。
    void changingOneEffectDoesNotMoveTheParts() {
        SyntheticBoardSpec s = everyEffect();
        const SyntheticBoard a = makeSyntheticBoard(s);
        s.missingFraction = 0.6;
        s.glareFraction = 0.0;
        const SyntheticBoard b = makeSyntheticBoard(s);
        QCOMPARE(a.parts.size(), b.parts.size());
        for (std::size_t i = 0; i < a.parts.size(); ++i) {
            QCOMPARE(a.parts[i].pose.x, b.parts[i].pose.x);
            QCOMPARE(a.parts[i].pose.y, b.parts[i].pose.y);
        }
        QVERIFY(b.presentCount() < a.presentCount());
    }

    // 標準答案的位置就是影像上的元件：主體中心是 bodyLevel，電極是 padLevel，缺件的格子是底色。
    void theGroundTruthMatchesThePixels() {
        SyntheticBoardSpec s;
        s.missingFraction = 0.3;
        s.positionJitterPx = 2.0;
        s.tiltDeg = 4.0;
        s.seed = 5;
        const SyntheticBoard board = makeSyntheticBoard(s);
        QCOMPARE(board.parts.size(), std::size_t{60});  // 6 × 10 都在影像內
        int missing = 0;
        for (const SyntheticPart& p : board.parts) {
            const int center = grayAt(board.image, p.pose.x, p.pose.y);
            // 電極的中心：元件座標 (±(寬/2 − 寬×0.11), 0)，轉成影像座標
            const Point2D padOffset = rotate({p.widthPx * 0.5 - p.widthPx * 0.11, 0.0}, p.pose.angleDeg);
            const int pad = grayAt(board.image, p.pose.x + padOffset.x, p.pose.y + padOffset.y);
            if (p.present) {
                QCOMPARE(center, s.bodyLevel);
                QCOMPARE(pad, s.padLevel);
            } else {
                ++missing;
                QCOMPARE(center, s.backgroundLevel);
                QCOMPARE(pad, s.backgroundLevel);
            }
        }
        QVERIFY(missing > 5 && missing < 35);  // 約 30%
        // 格子之間是底色
        const SyntheticPart& first = board.parts[0];
        QCOMPARE(grayAt(board.image, first.pose.x + s.pitchXPx / 2.0, first.pose.y + s.pitchYPx / 2.0), s.backgroundLevel);
    }

    void theArrayFollowsThePitchAndTheTilt() {
        SyntheticBoardSpec s;
        s.tiltDeg = 10.0;
        const SyntheticBoard board = makeSyntheticBoard(s);
        const SyntheticPart* a = nullptr;
        const SyntheticPart* b = nullptr;
        for (const SyntheticPart& p : board.parts) {
            if (p.row == 1 && p.col == 1) a = &p;
            if (p.row == 1 && p.col == 2) b = &p;
        }
        QVERIFY(a && b);
        const double dx = b->pose.x - a->pose.x, dy = b->pose.y - a->pose.y;
        QVERIFY(std::abs(std::hypot(dx, dy) - s.pitchXPx) < 1e-9);
        // 螢幕上逆時針為正：轉 +10° 之後，右邊的鄰居在畫面上變高（y 變小）
        QVERIFY(dy < 0.0);
        QVERIFY(std::abs(radiansToDegrees(std::atan2(-dy, dx)) - 10.0) < 1e-9);
        QCOMPARE(a->pose.angleDeg, 10.0);
    }

    void glareSaturatesAndNoiseAndBlurChangeThePixels() {
        SyntheticBoardSpec clean;
        clean.seed = 9;
        SyntheticBoardSpec glare = clean;
        glare.glareFraction = 1.0;
        const SyntheticBoard g = makeSyntheticBoard(glare);
        int saturated = 0;
        for (int y = 0; y < g.image.height; ++y)
            for (int x = 0; x < g.image.width; ++x) saturated += byteAt(g.image, x, y, 0) == 255 ? 1 : 0;
        QVERIFY(saturated > 1000);
        for (const SyntheticPart& p : g.parts) QVERIFY(p.glare == p.present);

        SyntheticBoardSpec noisy = clean;
        noisy.noiseAmplitude = 10;
        const SyntheticBoard n = makeSyntheticBoard(noisy);
        const SyntheticBoard c = makeSyntheticBoard(clean);
        int changed = 0, maxDiff = 0;
        for (int y = 0; y < c.image.height; y += 3) {
            for (int x = 0; x < c.image.width; x += 3) {
                const int d = std::abs(byteAt(n.image, x, y, 0) - byteAt(c.image, x, y, 0));
                changed += d > 0 ? 1 : 0;
                maxDiff = std::max(maxDiff, d);
            }
        }
        QVERIFY(changed > 1000);
        QVERIFY(maxDiff <= 10);

        SyntheticBoardSpec blurred = clean;
        blurred.blurRadius = 2;
        const SyntheticBoard bl = makeSyntheticBoard(blurred);
        // 元件的左邊緣：清楚的影像是一步跳上去，模糊之後中間有過渡的灰階
        const SyntheticPart& p = c.parts[0];
        const int edgeY = static_cast<int>(p.pose.y);
        const int edgeX = static_cast<int>(p.pose.x - p.widthPx / 2.0) - 1;
        QCOMPARE(byteAt(c.image, edgeX, edgeY, 0), clean.backgroundLevel);
        const int b = byteAt(bl.image, edgeX, edgeY, 0);
        QVERIFY2(b > clean.backgroundLevel && b < clean.padLevel, qPrintable(QString::number(b)));
    }

    void partsOutsideTheImageAreLeftOut() {
        SyntheticBoardSpec s;
        s.width = 300;
        s.height = 200;
        s.originXPx = 15.0;  // 第一欄的主體（寬 40）超出左邊
        const SyntheticBoard board = makeSyntheticBoard(s);
        for (const SyntheticPart& p : board.parts) {
            QVERIFY(p.col >= 1);
            QVERIFY(p.pose.x + p.widthPx / 2.0 <= s.width);
        }
        QVERIFY(!board.parts.empty());
    }

    void aColorBoardRepeatsTheGrayValue() {
        SyntheticBoardSpec s = everyEffect();
        s.format = PixelFormat::Bgrx32;
        const SyntheticBoard color = makeSyntheticBoard(s);
        s.format = PixelFormat::Gray8;
        const SyntheticBoard gray = makeSyntheticBoard(s);
        QVERIFY(color.image.format == PixelFormat::Bgrx32);
        for (int y = 0; y < gray.image.height; y += 7) {
            for (int x = 0; x < gray.image.width; x += 7) {
                const int v = byteAt(gray.image, x, y, 0);
                QCOMPARE(byteAt(color.image, x, y, 0), v);
                QCOMPARE(byteAt(color.image, x, y, 1), v);
                QCOMPARE(byteAt(color.image, x, y, 2), v);
            }
        }
    }

    // 12 bit：Gray16、值 ＝ 四捨五入 灰階 × 4095 ／ 255，significantBits 是 12；存成 PNG 再讀回來，來源也推測出 12。
    void aTwelveBitBoardMatchesTheGrayBoard() {
        SyntheticBoardSpec s = everyEffect();
        const SyntheticBoard gray = makeSyntheticBoard(s);
        s.format = PixelFormat::Gray16;
        s.grayBits = 12;
        const SyntheticBoard deep = makeSyntheticBoard(s);
        QVERIFY(deep.image.valid());
        QVERIFY(deep.image.format == PixelFormat::Gray16);
        QCOMPARE(deep.image.significantBits, 12);
        for (int y = 0; y < gray.image.height; y += 5)
            for (int x = 0; x < gray.image.width; x += 5)
                QCOMPARE(gray16At(deep.image, x, y), (byteAt(gray.image, x, y, 0) * 4095 + 127) / 255);
        QTemporaryDir dir = makeTempDir();
        QCOMPARE(saveSyntheticBoard(deep, dir.path(), QStringLiteral("deep")), QString());
        const Result<ImageView> loaded = infra::QtImageSource().load(utf8(dir.filePath(QStringLiteral("deep.png"))));
        QVERIFY(loaded.ok());
        QCOMPARE(loaded.value().significantBits, 12);
        QCOMPARE(pixelHash(loaded.value()), pixelHash(deep.image));
    }

    void invalidSpecsGiveNoImage() {
        SyntheticBoardSpec s;
        s.width = 0;
        QVERIFY(!makeSyntheticBoard(s).image.valid());
        s = SyntheticBoardSpec{};
        s.missingFraction = 1.5;
        QVERIFY(!makeSyntheticBoard(s).image.valid());
        s = SyntheticBoardSpec{};
        s.format = PixelFormat::Rgb24;
        QVERIFY(!makeSyntheticBoard(s).image.valid());
        s = SyntheticBoardSpec{};
        s.format = PixelFormat::Gray16;
        s.grayBits = 17;
        QVERIFY(!makeSyntheticBoard(s).image.valid());
    }

    void theGroundTruthIsValidJsonWithEveryPart() {
        const SyntheticBoard board = makeSyntheticBoard(everyEffect());
        const std::string text = groundTruthJson(board);
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(text), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        const QJsonObject root = doc.object();
        QCOMPARE(root.value(QStringLiteral("generator")).toString(), QStringLiteral("aoi-synthetic-board"));
        const QJsonArray parts = root.value(QStringLiteral("parts")).toArray();
        QCOMPARE(static_cast<std::size_t>(parts.size()), board.parts.size());
        const QJsonObject p3 = parts.at(3).toObject();
        QCOMPARE(p3.value(QStringLiteral("x")).toDouble(), board.parts[3].pose.x);  // 最短的往返表示：數字完全相同
        QCOMPARE(p3.value(QStringLiteral("present")).toBool(), board.parts[3].present);
        QCOMPARE(root.value(QStringLiteral("counts")).toObject().value(QStringLiteral("present")).toInt(),
                 static_cast<int>(board.presentCount()));
        QCOMPARE(root.value(QStringLiteral("spec")).toObject().value(QStringLiteral("seed")).toInt(), 77);
        QCOMPARE(root.value(QStringLiteral("image")).toObject().value(QStringLiteral("pixelHash")).toString(),
                 QString::asprintf("%016llx", static_cast<unsigned long long>(pixelHash(board.image))));
    }

    // 存成 PNG＋JSON，再經產品的影像來源讀回來：像素完全相同（PNG 無損）。
    void savedBoardsLoadBackIdentically() {
        QTemporaryDir dir = makeTempDir();
        const SyntheticBoard board = makeSyntheticBoard(everyEffect());
        QCOMPARE(saveSyntheticBoard(board, dir.path(), QStringLiteral("board-77")), QString());
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("board-77.json"))));
        infra::QtImageSource source;
        const Result<ImageView> loaded = source.load(utf8(dir.filePath(QStringLiteral("board-77.png"))));
        QVERIFY(loaded.ok());
        QVERIFY(loaded.value().format == PixelFormat::Gray8);
        QCOMPARE(pixelHash(loaded.value()), pixelHash(board.image));
    }
};

QTEST_GUILESS_MAIN(TstSyntheticBoard)
#include "tst_synthetic_board.moc"
