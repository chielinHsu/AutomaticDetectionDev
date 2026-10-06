// S06：infrastructure/image_qt。QImage ⇄ ImageView 的零複製（指標相等＋複製次數 ＝ 0）、要複製的格式有計數、
// QtImageSource 的契約（正規格式、錯誤碼、中文路徑、可重入）。S04 的 ImageViewAdapter 測試搬到這裡並擴充。
#include <QBuffer>
#include <QColor>
#include <QImageReader>
#include <QImageWriter>
#include <QtTest>
#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

#include "InfraTestSupport.h"
#include "QtTestPrinters.h"
#include "domain/image_buffer.h"
#include "infrastructure/image_qt/QtImageSource.h"
#include "infrastructure/image_qt/QtImages.h"
#include "presentation/canvas/ImageItem.h"

using namespace aoi;
using namespace aoi::infra;
using namespace aoi::test;

class TstImageQt : public QObject {
    Q_OBJECT

private slots:
    // ---- QImage → ImageView：零複製的格式 ----
    void wrappableFormatsAreNotCopied_data() {
        QTest::addColumn<int>("qtFormat");
        QTest::addColumn<int>("format");
        QTest::newRow("Grayscale8") << int(QImage::Format_Grayscale8) << int(PixelFormat::Gray8);
        QTest::newRow("Grayscale16") << int(QImage::Format_Grayscale16) << int(PixelFormat::Gray16);
        QTest::newRow("RGB32") << int(QImage::Format_RGB32) << int(PixelFormat::Bgrx32);
        QTest::newRow("ARGB32") << int(QImage::Format_ARGB32) << int(PixelFormat::Bgrx32);
        QTest::newRow("RGB888") << int(QImage::Format_RGB888) << int(PixelFormat::Rgb24);
        QTest::newRow("BGR888") << int(QImage::Format_BGR888) << int(PixelFormat::Bgr24);
    }
    void wrappableFormatsAreNotCopied() {
        QFETCH(int, qtFormat);
        QFETCH(int, format);
        const QImage image = patternImage(33, 17, static_cast<QImage::Format>(qtFormat));  // 奇數寬：Qt 的列有填充
        const CopyWatch watch;
        const ImageView view = fromQImage(image, 3.5);
        QCOMPARE(watch.copies(), std::uint64_t{0});
        QVERIFY(view.valid());
        QVERIFY(view.data == image.constBits());  // 零複製：指到同一塊像素
        QVERIFY(view.format == static_cast<PixelFormat>(format));
        QCOMPARE(view.width, 33);
        QCOMPARE(view.height, 17);
        QCOMPARE(view.strideBytes, static_cast<int>(image.bytesPerLine()));
        QCOMPARE(view.umPerPixel, 3.5);
        QVERIFY(view.owner != nullptr);
    }

    void bgrxKeepsTheMemoryOrderBlueGreenRed() {
        QImage image(4, 4, QImage::Format_RGB32);
        image.fill(QColor(10, 20, 30));
        const ImageView view = fromQImage(image);
        QCOMPARE(byteAt(view, 1, 1, 0), 30);  // B
        QCOMPARE(byteAt(view, 1, 1, 1), 20);  // G
        QCOMPARE(byteAt(view, 1, 1, 2), 10);  // R
        QImage rgb(4, 4, QImage::Format_RGB888);
        rgb.fill(QColor(10, 20, 30));
        QCOMPARE(byteAt(fromQImage(rgb), 0, 0, 0), 10);  // Rgb24：R 在前
        QImage bgr(4, 4, QImage::Format_BGR888);
        bgr.fill(QColor(10, 20, 30));
        QCOMPARE(byteAt(fromQImage(bgr), 0, 0, 0), 30);  // Bgr24：B 在前
    }

    // ---- 要複製的格式：恰好一次，而且計數 ----
    void otherFormatsAreConvertedOnceAndCounted_data() {
        QTest::addColumn<int>("qtFormat");
        QTest::addColumn<int>("format");
        QTest::newRow("RGB16") << int(QImage::Format_RGB16) << int(PixelFormat::Bgrx32);
        QTest::newRow("RGBX64 (16 bit color)") << int(QImage::Format_RGBX64) << int(PixelFormat::Bgrx32);
        QTest::newRow("ARGB32 premultiplied") << int(QImage::Format_ARGB32_Premultiplied) << int(PixelFormat::Bgrx32);
        QTest::newRow("RGBA8888") << int(QImage::Format_RGBA8888) << int(PixelFormat::Bgrx32);
    }
    void otherFormatsAreConvertedOnceAndCounted() {
        QFETCH(int, qtFormat);
        QFETCH(int, format);
        QImage image(20, 10, static_cast<QImage::Format>(qtFormat));
        image.fill(QColor(255, 0, 0));
        const CopyWatch watch;
        const ImageView view = fromQImage(image);
        QCOMPARE(watch.copies(), std::uint64_t{1});
        QCOMPARE(watch.bytes(), std::uint64_t{20 * 10 * 4});
        QVERIFY(view.valid());
        QVERIFY(view.data != image.constBits());
        QVERIFY(view.format == static_cast<PixelFormat>(format));
        QCOMPARE(byteAt(view, 3, 3, 2), 255);  // R
        QCOMPARE(byteAt(view, 3, 3, 0), 0);    // B
    }

    void grayIndexedImagesBecomeGray8() {
        QImage gray(16, 4, QImage::Format_Indexed8);
        QList<QRgb> table;
        for (int i = 0; i < 256; ++i) table.append(qRgb(i, i, i));
        gray.setColorTable(table);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 16; ++x) gray.setPixel(x, y, static_cast<uint>(x * 16));
        const CopyWatch watch;
        const ImageView view = fromQImage(gray);
        QCOMPARE(watch.copies(), std::uint64_t{1});
        QVERIFY(view.format == PixelFormat::Gray8);
        QCOMPARE(byteAt(view, 5, 2, 0), 80);

        QImage color = gray;  // 同樣的索引，彩色的色表 → Bgrx32
        table[80] = qRgb(200, 10, 20);
        color.setColorTable(table);
        const ImageView colored = fromQImage(color);
        QVERIFY(colored.format == PixelFormat::Bgrx32);
        QCOMPARE(byteAt(colored, 5, 2, 2), 200);  // R
    }

    void aNullImageGivesAnInvalidView() {
        QVERIFY(!fromQImage(QImage()).valid());
        QVERIFY(wrapAsQImage(ImageView{}).isNull());
    }

    // ---- 生命週期（ImageView 規則 2、3、5）----
    void theViewKeepsThePixelsAliveAfterTheImageIsGone() {
        ImageView view;
        {
            QImage image(32, 32, QImage::Format_RGB32);
            image.fill(QColor(1, 2, 3));
            view = fromQImage(image);
        }
        QVERIFY(view.owner != nullptr);
        QCOMPARE(byteAt(view, 7, 7, 0), 3);
        QCOMPARE(byteAt(view, 7, 7, 1), 2);
    }

    // 隱式共享：別人改原本的 QImage，Qt 先讓它分離，view 看到的像素不變。
    void changingTheOriginalImageDoesNotChangeTheView() {
        QImage image(8, 8, QImage::Format_Grayscale8);
        image.fill(10);
        const ImageView view = fromQImage(image);
        image.setPixel(0, 0, 0xFFFFFFFF);  // 非 const 的寫入：image 分離出自己的一份
        image.fill(99);
        QCOMPARE(byteAt(view, 0, 0, 0), 10);
        QCOMPARE(byteAt(view, 4, 4, 0), 10);
    }

    // ---- ImageView → QImage ----
    void wrappingAViewAsQImageDoesNotCopy_data() {
        QTest::addColumn<int>("format");
        QTest::addColumn<int>("qtFormat");
        QTest::newRow("Gray8") << int(PixelFormat::Gray8) << int(QImage::Format_Grayscale8);
        QTest::newRow("Gray16") << int(PixelFormat::Gray16) << int(QImage::Format_Grayscale16);
        QTest::newRow("Rgb24") << int(PixelFormat::Rgb24) << int(QImage::Format_RGB888);
        QTest::newRow("Bgr24") << int(PixelFormat::Bgr24) << int(QImage::Format_BGR888);
        QTest::newRow("Bgrx32") << int(PixelFormat::Bgrx32) << int(QImage::Format_RGB32);
    }
    void wrappingAViewAsQImageDoesNotCopy() {
        QFETCH(int, format);
        QFETCH(int, qtFormat);
        ImageBuffer buffer(21, 9, static_cast<PixelFormat>(format));
        buffer.row(4)[2] = 77;
        const ImageView view = buffer.view();
        const CopyWatch watch;
        const QImage image = wrapAsQImage(view);
        QCOMPARE(watch.copies(), std::uint64_t{0});
        QVERIFY(!image.isNull());
        QVERIFY(image.constBits() == view.data);
        QCOMPARE(static_cast<int>(image.format()), qtFormat);
        QCOMPARE(image.bytesPerLine(), static_cast<qsizetype>(view.strideBytes));
        QCOMPARE(static_cast<int>(image.constScanLine(4)[2]), 77);
        // 畫布用的 ImageItem::wrap（presentation）與這裡的格式對照表一致。
        const QImage shown = ui::ImageItem::wrap(view);
        QCOMPARE(shown.format(), image.format());
        QVERIFY(shown.constBits() == view.data);
        // 往返：QImage → view 指回同一塊像素
        QVERIFY(fromQImage(image).data == view.data);
        QCOMPARE(watch.copies(), std::uint64_t{0});
    }

    void aWrappedImageKeepsTheOwnerAliveAndWritesDetach() {
        QImage image;
        std::weak_ptr<const void> weak;
        {
            ImageBuffer buffer(8, 8, PixelFormat::Gray8);
            const ImageView view = buffer.view();
            weak = view.owner;
            image = wrapAsQImage(view);
        }
        QVERIFY(!weak.expired());  // QImage 活著，像素就活著
        const uchar* shared = image.constBits();
        image.setPixel(1, 1, 200);  // 寫入：唯讀的外部緩衝先深複製
        QVERIFY(image.constBits() != shared);
        image = QImage();
        QVERIFY(weak.expired());  // 最後一個參照放掉了
    }

    void toCanonicalOnlyCopiesNonCanonicalFormats() {
        const CopyWatch watch;
        QImage gray(10, 10, QImage::Format_Grayscale8);
        QCOMPARE(toCanonicalQImage(gray).constBits(), gray.constBits());
        QImage argb(10, 10, QImage::Format_ARGB32);
        QCOMPARE(toCanonicalQImage(argb).constBits(), argb.constBits());
        QCOMPARE(watch.copies(), std::uint64_t{0});
        QImage rgb(10, 10, QImage::Format_RGB888);
        rgb.fill(QColor(1, 2, 3));
        const QImage canonical = toCanonicalQImage(rgb);
        QCOMPARE(canonical.format(), QImage::Format_RGB32);
        QCOMPARE(watch.copies(), std::uint64_t{1});
    }

    // ---- QtImageSource ----
    void theSourceKnowsTheCommonSuffixesCaseInsensitively() {
        QtImageSource source;
        QString list;
        for (const std::string& s : source.suffixes()) list += QString::fromStdString(s) + QLatin1Char(' ');
        qInfo("QtImageSource reads: %s", qPrintable(list));  // 記在 log：這台機器的 Qt 有哪些外掛
        for (const char* path : {"a.png", "A.PNG", "b.bmp", "c.jpg", "d.JPEG", "dir.x/e.png"}) QVERIFY2(source.canRead(path), path);
        for (const char* path : {"a.txt", "noext", "dir.png/file", ""}) QVERIFY2(!source.canRead(path), path);
        const bool tiff = std::find(source.suffixes().begin(), source.suffixes().end(), "tif") != source.suffixes().end();
        QCOMPARE(source.canRead("x.tif"), tiff);
    }

    void loadingGivesCanonicalFormatsAndTheSamePixels_data() {
        QTest::addColumn<QString>("file");
        QTest::addColumn<int>("saveFormat");
        QTest::addColumn<int>("expected");
        QTest::addColumn<int>("copies");
        QTest::newRow("gray PNG") << "g.png" << int(QImage::Format_Grayscale8) << int(PixelFormat::Gray8) << 0;
        QTest::newRow("16-bit gray PNG") << "g16.png" << int(QImage::Format_Grayscale16) << int(PixelFormat::Gray16) << 0;
        QTest::newRow("color PNG") << "c.png" << int(QImage::Format_RGB32) << int(PixelFormat::Bgrx32) << 0;
        QTest::newRow("PNG with alpha") << "a.png" << int(QImage::Format_ARGB32) << int(PixelFormat::Bgrx32) << 0;
        QTest::newRow("color BMP") << "c.bmp" << int(QImage::Format_RGB32) << int(PixelFormat::Bgrx32) << 0;
        // 8 bit 灰階的 BMP 是索引色檔：Qt 解成 Indexed8，要轉一次（記在 perf.md 的複製表）
        QTest::newRow("gray BMP") << "g.bmp" << int(QImage::Format_Grayscale8) << int(PixelFormat::Gray8) << 1;
    }
    void loadingGivesCanonicalFormatsAndTheSamePixels() {
        QFETCH(QString, file);
        QFETCH(int, saveFormat);
        QFETCH(int, expected);
        QFETCH(int, copies);
        QTemporaryDir dir = makeTempDir();
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(file);
        const QImage original = patternImage(61, 23, static_cast<QImage::Format>(saveFormat));
        QVERIFY(original.save(path));

        QtImageSource source;
        const CopyWatch watch;
        const Result<ImageView> r = source.load(utf8(path));
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        QCOMPARE(watch.copies(), static_cast<std::uint64_t>(copies));
        const ImageView& v = r.value();
        QVERIFY(v.format == static_cast<PixelFormat>(expected));
        QCOMPARE(v.width, 61);
        QCOMPARE(v.height, 23);
        QCOMPARE(v.umPerPixel, 0.0);
        QVERIFY(v.owner != nullptr);
        for (int y = 0; y < v.height; y += 5) {
            for (int x = 0; x < v.width; x += 7) {
                if (v.format == PixelFormat::Gray16) {
                    QCOMPARE(gray16At(v, x, y), static_cast<int>(reinterpret_cast<const quint16*>(original.constScanLine(y))[x]));
                } else if (v.format == PixelFormat::Gray8) {
                    QCOMPARE(byteAt(v, x, y, 0), static_cast<int>(original.constScanLine(y)[x]));
                } else {
                    const QRgb p = original.pixel(x, y);
                    QCOMPARE(byteAt(v, x, y, 0), qBlue(p));
                    QCOMPARE(byteAt(v, x, y, 1), qGreen(p));
                    QCOMPARE(byteAt(v, x, y, 2), qRed(p));
                }
            }
        }
    }

    // 12 bit 的影像存成 16 bit 的 PNG、值只到 4095：來源推測出 12；值放滿 16 bit 的是 16。都是 0 次複製。
    void twelveBitImagesKeepTheirValuesAndReportTheirDepth_data() {
        QTest::addColumn<int>("maxValue");
        QTest::addColumn<int>("bits");
        QTest::newRow("12 bit") << 4095 << 12;
        QTest::newRow("10 bit") << 1023 << 10;
        QTest::newRow("16 bit") << 65535 << 16;
    }
    void twelveBitImagesKeepTheirValuesAndReportTheirDepth() {
        QFETCH(int, maxValue);
        QFETCH(int, bits);
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("deep.png"));
        QImage img(40, 20, QImage::Format_Grayscale16);
        for (int y = 0; y < 20; ++y) {
            auto* row = reinterpret_cast<quint16*>(img.scanLine(y));
            for (int x = 0; x < 40; ++x) row[x] = static_cast<quint16>((x * 37 + y * 101) % (maxValue + 1));
        }
        reinterpret_cast<quint16*>(img.scanLine(19))[39] = static_cast<quint16>(maxValue);
        QVERIFY(img.save(path));
        QtImageSource source;
        const CopyWatch watch;
        const Result<ImageView> r = source.load(utf8(path));
        QVERIFY(r.ok());
        QCOMPARE(watch.copies(), std::uint64_t{0});
        QVERIFY(r.value().format == PixelFormat::Gray16);
        QCOMPARE(r.value().significantBits, bits);
        QCOMPARE(gray16At(r.value(), 39, 19), maxValue);  // 值照原樣（低位對齊），不放大
        QCOMPARE(gray16At(r.value(), 5, 3), (5 * 37 + 3 * 101) % (maxValue + 1));
    }

    // 畫布顯示：有效位元數不到 16 的 Gray16 照原樣畫會幾乎全黑，轉成 8 bit（右移），算一次複製；16 bit 的零複製。
    void theCanvasShowsTwelveBitImagesAtFullBrightness() {
        ImageBuffer b(8, 2, PixelFormat::Gray16);
        const std::uint16_t values[] = {0, 16, 2048, 4095};
        for (int i = 0; i < 4; ++i) std::memcpy(b.row(0) + 2 * i, &values[i], 2);
        ImageView view = b.view();
        view.significantBits = 12;
        const CopyWatch watch;
        const QImage shown = ui::ImageItem::wrap(view);
        QCOMPARE(watch.copies(), std::uint64_t{1});
        QCOMPARE(shown.format(), QImage::Format_Grayscale8);
        QCOMPARE(static_cast<int>(shown.constScanLine(0)[1]), 1);    // 16 >> 4
        QCOMPARE(static_cast<int>(shown.constScanLine(0)[2]), 128);  // 2048 >> 4
        QCOMPARE(static_cast<int>(shown.constScanLine(0)[3]), 255);  // 4095 >> 4：全白
        view.significantBits = 16;
        const CopyWatch none;
        QVERIFY(ui::ImageItem::wrap(view).constBits() == view.data);
        QCOMPARE(none.copies(), std::uint64_t{0});
    }

    void aJpegLoadsWithoutCopies() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("c.jpg"));
        QVERIFY(patternImage(64, 48, QImage::Format_RGB32).save(path, "JPG", 95));
        QtImageSource source;
        const CopyWatch watch;
        const Result<ImageView> r = source.load(utf8(path));
        QVERIFY(r.ok());
        QVERIFY(r.value().format == PixelFormat::Bgrx32);
        QCOMPARE(watch.copies(), std::uint64_t{0});
    }

    // 不套用 EXIF 的旋轉（ADR-014）：座標要對到感測器的像素。先證明這個檔案的方向標記有效（Qt 開啟自動旋轉時寬高對調），
    // 再確認來源讀出原本的寬高與像素。（S06 的變異 M05 在這個測試之前存活。）
    void theExifOrientationIsNotApplied() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("rotated.jpg"));
        QByteArray jpeg;
        {
            QBuffer buffer(&jpeg);
            buffer.open(QIODevice::WriteOnly);
            QVERIFY(patternImage(40, 20, QImage::Format_RGB32).save(&buffer, "JPG", 95));
        }
        QVERIFY(writeBytes(path, withExifOrientation(jpeg, 6)));
        QImageReader rotating(path);
        rotating.setAutoTransform(true);
        QCOMPARE(rotating.read().size(), QSize(20, 40));  // 方向標記確實有效
        const Result<ImageView> r = QtImageSource().load(utf8(path));
        QVERIFY(r.ok());
        QCOMPARE(r.value().width, 40);
        QCOMPARE(r.value().height, 20);
    }

    // 檔案裡的 DPI 不可信（常是 72、96 的預設值）：校正值一律是 0（IImageSource 的契約）。
    void theCalibrationIsNeverTakenFromTheFile() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("dpi.png"));
        QImage image = patternImage(10, 10, QImage::Format_RGB32);
        image.setDotsPerMeterX(100000);  // 10 µm／像素
        image.setDotsPerMeterY(100000);
        QVERIFY(image.save(path));
        QCOMPARE(QImageReader(path).read().dotsPerMeterX(), 100000);  // 檔案裡確實有
        QtImageSource source;
        QCOMPARE(source.load(utf8(path)).value().umPerPixel, 0.0);
    }

    void aPathWithChineseCharactersLoads() {
        QTemporaryDir dir = makeTempDir();
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("影像 測試")));
        const QString path = dir.filePath(QStringLiteral("影像 測試/板子 一號.png"));
        QVERIFY(patternImage(12, 8, QImage::Format_Grayscale8).save(path));
        QtImageSource source;
        const Result<ImageView> r = source.load(utf8(path));
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        QCOMPARE(r.value().width, 12);
    }

    // 格式由內容決定：PNG 的內容、.jpg 的檔名也讀得到。
    void theContentDecidesTheFormat() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("really-a-png.jpg"));
        QVERIFY(patternImage(9, 9, QImage::Format_Grayscale8).save(path, "PNG"));
        QtImageSource source;
        const Result<ImageView> r = source.load(utf8(path));
        QVERIFY(r.ok());
        QVERIFY(r.value().format == PixelFormat::Gray8);
    }

    void fileProblemsGiveTheContractErrorCodes() {
        QTemporaryDir dir = makeTempDir();
        QtImageSource source;
        QCOMPARE_CODE(source.load(utf8(dir.filePath(QStringLiteral("missing.png")))).error().code, ErrorCode::NotFound);
        QCOMPARE_CODE(source.load("").error().code, ErrorCode::NotFound);
        QCOMPARE_CODE(source.load(utf8(dir.path())).error().code, ErrorCode::IoFailed);  // 資料夾

        const QString garbage = dir.filePath(QStringLiteral("garbage.png"));
        QVERIFY(writeBytes(garbage, "this is not a png"));
        const Result<ImageView> bad = source.load(utf8(garbage));
        QCOMPARE_CODE(bad.error().code, ErrorCode::FormatInvalid);
        QVERIFY(!bad.error().message.empty());

        const QString empty = dir.filePath(QStringLiteral("empty.png"));
        QVERIFY(writeBytes(empty, QByteArray()));
        QCOMPARE_CODE(source.load(utf8(empty)).error().code, ErrorCode::FormatInvalid);
    }

    // 截斷的檔案：不當機、不丟例外；回傳錯誤，或（解碼器能讀到的部分）一張完整大小的影像。記下實際的行為。
    void aTruncatedFileDoesNotCrash() {
        QTemporaryDir dir = makeTempDir();
        QByteArray png;
        {
            QBuffer buffer(&png);
            buffer.open(QIODevice::WriteOnly);
            QVERIFY(patternImage(200, 100, QImage::Format_RGB32).save(&buffer, "PNG"));
        }
        QtImageSource source;
        for (const double fraction : {0.1, 0.5, 0.9}) {
            const QString path = dir.filePath(QStringLiteral("cut-%1.png").arg(fraction));
            QVERIFY(writeBytes(path, png.left(static_cast<qsizetype>(png.size() * fraction))));
            const Result<ImageView> r = source.load(utf8(path));
            if (r.ok()) {
                qInfo("truncated PNG at %.0f%%: Qt returned an image of %dx%d", fraction * 100, r.value().width, r.value().height);
                QCOMPARE(r.value().width, 200);
            } else {
                qInfo("truncated PNG at %.0f%%: %s", fraction * 100, r.error().message.c_str());
                QCOMPARE_CODE(r.error().code, ErrorCode::FormatInvalid);
            }
        }
    }

    // 可重入：幾個執行緒同時讀同一個檔案。
    void loadIsReentrant() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("shared.png"));
        QVERIFY(patternImage(300, 200, QImage::Format_RGB32).save(path));
        QtImageSource source;
        std::atomic<int> failures{0};
        std::vector<std::thread> threads;
        const std::string p = utf8(path);
        for (int t = 0; t < 6; ++t) {
            threads.emplace_back([&] {
                for (int i = 0; i < 5; ++i) {
                    const Result<ImageView> r = source.load(p);
                    if (!r.ok() || r.value().width != 300 || byteAt(r.value(), 10, 10, 2) != ((10 * 7 + 10) & 255)) ++failures;
                }
            });
        }
        for (std::thread& t : threads) t.join();
        QCOMPARE(failures.load(), 0);
    }
};

QTEST_GUILESS_MAIN(TstImageQt)
#include "tst_image_qt.moc"
