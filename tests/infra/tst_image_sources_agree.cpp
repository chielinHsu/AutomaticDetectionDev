// S06：IImageSource 的 Liskov 契約——同一個檔案，Qt 與 OpenCV 兩個來源給出相同語意的 ImageView
// （格式、大小、校正值、像素）。AOI_WITH_OPENCV=ON 才建置。已知的例外用 QEXPECT_FAIL 記下（行為改了會變成 XPASS）。
#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QtTest>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <opencv2/imgcodecs.hpp>

#include "InfraTestSupport.h"
#include "infrastructure/backend_opencv/OpenCvImageSource.h"
#include "infrastructure/image_qt/QtImageSource.h"

using namespace aoi;
using namespace aoi::infra;
using namespace aoi::test;

namespace {

enum Case {
    GrayPng,
    Gray16Png,
    Gray12Png,
    ColorPng,
    AlphaPng,
    PaletteColorPng,
    PaletteGrayPng,
    PaletteGrayBmp,
    IdentityGrayPalettePng,
    Color16Png,
    ColorBmp,
    GrayBmp,
    ColorJpeg,
    GrayJpeg,
    Pgm,
    ExifRotatedJpeg,
    Ppm,
    GrayAlphaPngFromOpenCv,
};

QImage qtImage(Case c) {
    switch (c) {
        case GrayPng:
        case GrayBmp:
        case GrayJpeg:
        case Pgm: return patternImage(61, 23, QImage::Format_Grayscale8);
        case Gray16Png:
        case Gray12Png: {
            QImage img(61, 23, QImage::Format_Grayscale16);
            for (int y = 0; y < img.height(); ++y) {
                auto* row = reinterpret_cast<quint16*>(img.scanLine(y));
                for (int x = 0; x < img.width(); ++x)
                    row[x] = static_cast<quint16>(c == Gray12Png ? (x * 67 + y * 13) % 4096 : x * 1000 + y * 37);
            }
            return img;
        }
        case ColorPng:
        case ColorBmp:
        case ColorJpeg:
        case Ppm: return patternImage(61, 23, QImage::Format_RGB32);
        case AlphaPng: {
            QImage img = patternImage(61, 23, QImage::Format_ARGB32);
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x) {
                    const QRgb p = img.pixel(x, y);
                    img.setPixel(x, y, qRgba(qRed(p), qGreen(p), qBlue(p), (x * 13) & 255));
                }
            return img;
        }
        case PaletteColorPng:
        case PaletteGrayPng:
        case PaletteGrayBmp:
        case IdentityGrayPalettePng: {
            QImage img(61, 23, QImage::Format_Indexed8);
            QList<QRgb> table;
            const int colors = c == IdentityGrayPalettePng ? 256 : 16;
            for (int i = 0; i < colors; ++i) {
                if (c == IdentityGrayPalettePng) table.append(qRgb(i, i, i));  // 索引 ＝ 灰階值
                else if (c == PaletteColorPng) table.append(qRgb(i * 16, 255 - i * 9, (i * 77) & 255));
                else table.append(qRgb(i * 17, i * 17, i * 17));
            }
            img.setColorTable(table);
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x) img.setPixel(x, y, static_cast<uint>((x + y) % 16));
            return img;
        }
        case Color16Png: {
            QImage img(61, 23, QImage::Format_RGBX64);
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                    img.setPixelColor(x, y, QColor::fromRgba64(static_cast<quint16>(x * 1071), static_cast<quint16>(y * 2801 + 5),
                                                               static_cast<quint16>((x * y * 97) & 0xFFFF)));
            return img;
        }
        case ExifRotatedJpeg: return patternImage(61, 23, QImage::Format_RGB32);
        case GrayAlphaPngFromOpenCv: break;
    }
    return {};
}

const char* suffixOf(Case c) {
    switch (c) {
        case ColorBmp:
        case GrayBmp:
        case PaletteGrayBmp: return "bmp";
        case ColorJpeg:
        case GrayJpeg:
        case ExifRotatedJpeg: return "jpg";
        case Pgm: return "pgm";
        case Ppm: return "ppm";
        default: return "png";
    }
}

bool writeGrayAlphaWithOpenCv(const QString& path) {
    cv::Mat m(23, 61, CV_8UC2);
    for (int y = 0; y < m.rows; ++y)
        for (int x = 0; x < m.cols; ++x) m.at<cv::Vec2b>(y, x) = cv::Vec2b(static_cast<uchar>(x * 4), static_cast<uchar>(y * 9));
    std::vector<uchar> bytes;
    try {
        if (!cv::imencode(".png", m, bytes)) return false;
    } catch (const cv::Exception&) {
        return false;
    }
    return writeBytes(path, QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size())));
}

}  // namespace

class TstImageSourcesAgree : public QObject {
    Q_OBJECT

private slots:
    void bothSourcesGiveTheSameImage_data() {
        QTest::addColumn<int>("which");
        QTest::addColumn<int>("tolerance");  // 每個 byte 允許的差（JPEG：兩邊的解碼器可能不是同一版）
        QTest::newRow("gray PNG") << int(GrayPng) << 0;
        QTest::newRow("16-bit gray PNG") << int(Gray16Png) << 0;
        QTest::newRow("12-bit gray in a 16-bit PNG") << int(Gray12Png) << 0;
        QTest::newRow("color PNG") << int(ColorPng) << 0;
        QTest::newRow("PNG with alpha") << int(AlphaPng) << 0;
        QTest::newRow("palette PNG, color") << int(PaletteColorPng) << 0;
        QTest::newRow("palette PNG, 16 grays") << int(PaletteGrayPng) << 0;
        QTest::newRow("palette BMP, 16 grays") << int(PaletteGrayBmp) << 0;
        QTest::newRow("palette PNG, 256 grays in order") << int(IdentityGrayPalettePng) << 0;
        QTest::newRow("16-bit color PNG") << int(Color16Png) << 0;
        QTest::newRow("color BMP") << int(ColorBmp) << 0;
        QTest::newRow("gray BMP") << int(GrayBmp) << 0;
        QTest::newRow("color JPEG") << int(ColorJpeg) << 2;
        QTest::newRow("gray JPEG") << int(GrayJpeg) << 2;
        QTest::newRow("PGM") << int(Pgm) << 0;
        QTest::newRow("JPEG with EXIF orientation 6 (not applied)") << int(ExifRotatedJpeg) << 2;
        QTest::newRow("PPM") << int(Ppm) << 0;
        QTest::newRow("gray + alpha PNG (OpenCV writes it)") << int(GrayAlphaPngFromOpenCv) << 0;
    }
    void bothSourcesGiveTheSameImage() {
        QFETCH(int, which);
        QFETCH(int, tolerance);
        const Case c = static_cast<Case>(which);
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("file.") + QString::fromLatin1(suffixOf(c)));
        if (c == GrayAlphaPngFromOpenCv) {
            if (!writeGrayAlphaWithOpenCv(path)) QSKIP("OpenCV cannot write a 2-channel PNG");
        } else if (c == ExifRotatedJpeg) {
            QByteArray jpeg;
            QBuffer buffer(&jpeg);
            buffer.open(QIODevice::WriteOnly);
            QVERIFY(qtImage(c).save(&buffer, "JPG", 95));
            QVERIFY(writeBytes(path, withExifOrientation(jpeg, 6)));
        } else {
            QVERIFY(qtImage(c).save(path, nullptr, c == ColorJpeg || c == GrayJpeg ? 95 : -1));
        }

        QtImageSource qt;
        OpenCvImageSource cv;
        const Result<ImageView> a = qt.load(utf8(path));
        const Result<ImageView> b = cv.load(utf8(path));
        QVERIFY2(a.ok(), a.ok() ? "" : a.error().message.c_str());
        QVERIFY2(b.ok(), b.ok() ? "" : b.error().message.c_str());
        const ImageView& x = a.value();
        const ImageView& y = b.value();
        const QImage decoded(path);
        qInfo("Qt: %s (QImage format %d before conversion, %d colors, gray table %d), OpenCV: %s", toString(x.format).data(),
              static_cast<int>(decoded.format()), static_cast<int>(decoded.colorCount()), decoded.isGrayscale() ? 1 : 0,
              toString(y.format).data());
        if (decoded.colorCount() > 0) {
            QString t;
            for (int i = 0; i < std::min<int>(4, static_cast<int>(decoded.colorCount())); ++i) t += QString::number(decoded.color(i), 16) + QLatin1Char(' ');
            qInfo("first colors: %s", qPrintable(t));
        }
        if (c == PaletteGrayBmp) {
            // 已知的例外（spec.md「已知的限制」、ADR-014）：索引色檔案的「灰階」兩邊定義不同。Qt 只把「索引 ＝ 灰階值」的色表當灰階，
            // OpenCV 的 BMP 解碼器只要每個顏色都是灰就給 8UC1（它的 PNG 解碼器則一律展開成彩色）。像素值相同，只有格式不同。
            QEXPECT_FAIL("", "BMP with a gray palette that is not 0..255 in order: Qt gives Bgrx32, OpenCV gives Gray8", Abort);
        }
        QVERIFY2(x.format == y.format, "the same file must give the same pixel format");
        QCOMPARE(x.width, y.width);
        QCOMPARE(x.height, y.height);
        QCOMPARE(x.umPerPixel, y.umPerPixel);
        QCOMPARE(x.significantBits, y.significantBits);  // 同一個推測函式：12 bit 的檔案兩邊都是 12
        const int channels = x.format == PixelFormat::Bgrx32 ? 3 : x.format == PixelFormat::Gray16 ? 2 : 1;  // Bgrx32 的 x 不比
        int worst = 0;
        for (int row = 0; row < x.height; ++row) {
            for (int col = 0; col < x.width; ++col) {
                for (int k = 0; k < channels; ++k) {
                    const int d = std::abs(byteAt(x, col, row, k) - byteAt(y, col, row, k));
                    worst = std::max(worst, d);
                }
            }
        }
        qInfo("largest byte difference: %d", worst);
        QVERIFY2(worst <= tolerance, qPrintable(QStringLiteral("largest byte difference %1 > %2").arg(worst).arg(tolerance)));
    }
};

QTEST_GUILESS_MAIN(TstImageSourcesAgree)
#include "tst_image_sources_agree.moc"
