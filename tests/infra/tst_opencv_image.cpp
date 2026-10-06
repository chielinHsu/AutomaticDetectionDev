// S06：infrastructure/backend_opencv 的影像部分（AOI_WITH_OPENCV=ON 才建置）。cv::Mat ⇄ ImageView 的零複製、
// OpenCvImageSource 的契約（正規格式、TIFF 與 16 bit、複製計數、中文路徑、錯誤碼、可重入）。
#include <QFileInfo>
#include <QtTest>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <thread>
#include <vector>

#include "InfraTestSupport.h"
#include "QtTestPrinters.h"
#include "domain/image_buffer.h"
#include "infrastructure/backend_opencv/OpenCvImageSource.h"
#include "infrastructure/backend_opencv/OpenCvImages.h"

using namespace aoi;
using namespace aoi::infra;
using namespace aoi::test;

namespace {

// 有紋理的 Mat：每個元素都不同。
cv::Mat patternMat(int rows, int cols, int type) {
    cv::Mat m(rows, cols, type);
    const int channels = m.channels();
    const bool wide = m.depth() == CV_16U;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            for (int c = 0; c < channels; ++c) {
                const int v = (x * 7 + y * 3 + c * 50) & 255;
                if (wide) m.ptr<std::uint16_t>(y)[x * channels + c] = static_cast<std::uint16_t>(v * 257 + c * 13 + x);
                else m.ptr<std::uint8_t>(y)[x * channels + c] = static_cast<std::uint8_t>(v);
            }
        }
    }
    return m;
}

// cv::imwrite 在 Windows 用 ANSI 字碼頁開檔；測試和產品一樣自己用 UTF-8 路徑寫。
bool writeEncoded(const QString& path, const cv::Mat& m, const char* ext, const std::vector<int>& params = {}) {
    std::vector<uchar> bytes;
    try {
        if (!cv::imencode(ext, m, bytes, params)) return false;
    } catch (const cv::Exception&) {
        return false;  // 例如 EXR：OpenCV 4.x 要設 OPENCV_IO_ENABLE_OPENEXR 才開放
    }
    const std::string u8 = utf8(path);
    std::ofstream out(std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(u8.data()), u8.size())), std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

}  // namespace

class TstOpenCvImage : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { qInfo("OpenCV %s", CV_VERSION); }

    // ---- cv::Mat → ImageView ----
    void ownedMatsBecomeViewsWithoutCopying_data() {
        QTest::addColumn<int>("type");
        QTest::addColumn<int>("format");
        QTest::newRow("8UC1") << CV_8UC1 << int(PixelFormat::Gray8);
        QTest::newRow("16UC1") << CV_16UC1 << int(PixelFormat::Gray16);
        QTest::newRow("8UC3") << CV_8UC3 << int(PixelFormat::Bgr24);
        QTest::newRow("8UC4") << CV_8UC4 << int(PixelFormat::Bgrx32);
    }
    void ownedMatsBecomeViewsWithoutCopying() {
        QFETCH(int, type);
        QFETCH(int, format);
        const cv::Mat m = patternMat(13, 21, type);
        const CopyWatch watch;
        const ImageView v = fromMat(m, 2.5);
        QCOMPARE(watch.copies(), std::uint64_t{0});
        QVERIFY(v.valid());
        QVERIFY(v.data == m.data);
        QVERIFY(v.format == static_cast<PixelFormat>(format));
        QCOMPARE(v.strideBytes, static_cast<int>(m.step[0]));
        QCOMPARE(v.width, 21);
        QCOMPARE(v.umPerPixel, 2.5);
    }

    void aRegionOfAMatKeepsTheParentStride() {
        const cv::Mat parent = patternMat(40, 60, CV_8UC3);
        const cv::Mat roi = parent(cv::Rect(5, 7, 10, 4));
        const ImageView v = fromMat(roi);
        QVERIFY(v.valid());
        QVERIFY(v.data == roi.data);
        QCOMPARE(v.strideBytes, static_cast<int>(parent.step[0]));
        QVERIFY(!v.packed());
        QCOMPARE(byteAt(v, 0, 0, 0), static_cast<int>(parent.at<cv::Vec3b>(7, 5)[0]));
    }

    void unsupportedMatsGiveAnInvalidView_data() {
        QTest::addColumn<int>("type");
        QTest::newRow("32FC1") << CV_32FC1;
        QTest::newRow("8SC1") << CV_8SC1;
        QTest::newRow("8UC2") << CV_8UC2;
        QTest::newRow("16UC3") << CV_16UC3;
    }
    void unsupportedMatsGiveAnInvalidView() {
        QFETCH(int, type);
        QVERIFY(!fromMat(cv::Mat(4, 4, type, cv::Scalar(0))).valid());
        QVERIFY(!fromMat(cv::Mat()).valid());
        const int sizes[] = {2, 2, 2};
        QVERIFY(!fromMat(cv::Mat(3, sizes, CV_8UC1, cv::Scalar(0))).valid());  // 三維
    }

    void theViewKeepsTheMatAliveAfterTheMatIsGone() {
        ImageView v;
        {
            cv::Mat m(8, 8, CV_8UC1, cv::Scalar(42));
            v = fromMat(m);
        }
        QCOMPARE(byteAt(v, 3, 3, 0), 42);
        QVERIFY(v.owner != nullptr);
    }

    // ---- ImageView → cv::Mat ----
    void viewsBecomeMatsWithoutCopying_data() {
        QTest::addColumn<int>("format");
        QTest::addColumn<int>("type");
        QTest::newRow("Gray8") << int(PixelFormat::Gray8) << CV_8UC1;
        QTest::newRow("Gray16") << int(PixelFormat::Gray16) << CV_16UC1;
        QTest::newRow("Rgb24") << int(PixelFormat::Rgb24) << CV_8UC3;
        QTest::newRow("Bgr24") << int(PixelFormat::Bgr24) << CV_8UC3;
        QTest::newRow("Bgrx32") << int(PixelFormat::Bgrx32) << CV_8UC4;
    }
    void viewsBecomeMatsWithoutCopying() {
        QFETCH(int, format);
        QFETCH(int, type);
        ImageBuffer buffer(30, 10, static_cast<PixelFormat>(format));
        buffer.row(2)[5] = 99;
        const ImageView v = subView(buffer.view(), 1, 1, 20, 8);  // 子影像：stride 比一列大
        const CopyWatch watch;
        const MatView borrowed = borrowAsMat(v);
        QCOMPARE(watch.copies(), std::uint64_t{0});
        QVERIFY(borrowed.mat.data == v.data);
        QCOMPARE(borrowed.mat.type(), type);
        QCOMPARE(borrowed.mat.step[0], static_cast<std::size_t>(v.strideBytes));
        QCOMPARE(borrowed.mat.cols, 20);
        QVERIFY(borrowed.owner == v.owner);
        QCOMPARE(static_cast<int>(borrowed.mat.ptr<std::uint8_t>(1)[5 - bytesPerPixel(v.format)]), 99);  // (1,1) 起點的偏移
    }

    // 借來的 Mat 不擁有像素：不能再變成 view（會懸空），fromMat 拒絕它。
    void aBorrowedMatCannotBecomeAView() {
        ImageBuffer buffer(8, 8, PixelFormat::Gray8);
        const MatView borrowed = borrowAsMat(buffer.view());
        QVERIFY(!borrowed.mat.empty());
        QVERIFY(!fromMat(borrowed.mat).valid());
        QVERIFY(borrowAsMat(ImageView{}).mat.empty());
    }

    // ---- OpenCvImageSource ----
    void loadingGivesCanonicalFormats_data() {
        QTest::addColumn<QString>("file");
        QTest::addColumn<int>("type");
        QTest::addColumn<int>("expected");
        QTest::addColumn<int>("copies");
        QTest::newRow("gray TIFF") << "g.tif" << CV_8UC1 << int(PixelFormat::Gray8) << 0;
        QTest::newRow("16-bit gray TIFF") << "g16.tiff" << CV_16UC1 << int(PixelFormat::Gray16) << 0;
        QTest::newRow("color TIFF") << "c.tif" << CV_8UC3 << int(PixelFormat::Bgrx32) << 1;
        QTest::newRow("16-bit color TIFF") << "c16.tif" << CV_16UC3 << int(PixelFormat::Bgrx32) << 2;
        QTest::newRow("BGRA TIFF") << "a.tif" << CV_8UC4 << int(PixelFormat::Bgrx32) << 0;
        QTest::newRow("gray PNG") << "g.png" << CV_8UC1 << int(PixelFormat::Gray8) << 0;
        QTest::newRow("16-bit gray PNG") << "g16.png" << CV_16UC1 << int(PixelFormat::Gray16) << 0;
        QTest::newRow("color PNG") << "c.png" << CV_8UC3 << int(PixelFormat::Bgrx32) << 1;
        QTest::newRow("16-bit color PNG (BGRA)") << "c16.png" << CV_16UC4 << int(PixelFormat::Bgrx32) << 1;
        QTest::newRow("color BMP") << "c.bmp" << CV_8UC3 << int(PixelFormat::Bgrx32) << 1;
        QTest::newRow("gray BMP") << "g.bmp" << CV_8UC1 << int(PixelFormat::Gray8) << 0;
        QTest::newRow("PGM") << "g.pgm" << CV_8UC1 << int(PixelFormat::Gray8) << 0;
    }
    void loadingGivesCanonicalFormats() {
        QFETCH(QString, file);
        QFETCH(int, type);
        QFETCH(int, expected);
        QFETCH(int, copies);
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(file);
        const cv::Mat original = patternMat(23, 61, type);
        QVERIFY(writeEncoded(path, original, ("." + QFileInfo(file).suffix().toStdString()).c_str()));

        OpenCvImageSource source;
        QVERIFY(source.canRead(utf8(path)));
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
        // 內容：灰階逐值相同；彩色比 B、G、R（16 bit 彩色比四捨五入 v / 257）
        for (int y = 0; y < 23; y += 4) {
            for (int x = 0; x < 61; x += 6) {
                if (original.channels() == 1) {
                    if (v.format == PixelFormat::Gray16) QCOMPARE(gray16At(v, x, y), static_cast<int>(original.at<std::uint16_t>(y, x)));
                    else QCOMPARE(byteAt(v, x, y, 0), static_cast<int>(original.at<std::uint8_t>(y, x)));
                    continue;
                }
                for (int c = 0; c < 3; ++c) {
                    const int want = original.depth() == CV_16U
                                         ? static_cast<int>(std::lround(original.ptr<std::uint16_t>(y)[x * original.channels() + c] / 257.0))
                                         : static_cast<int>(original.ptr<std::uint8_t>(y)[x * original.channels() + c]);
                    QCOMPARE(byteAt(v, x, y, c), want);
                }
            }
        }
    }

    // 12 bit：存在 16 bit 的 TIFF、值只到 4095 → 12（值照原樣）。
    void aTwelveBitTiffReportsTwelveBits() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("deep.tif"));
        cv::Mat m(20, 40, CV_16UC1);
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 40; ++x) m.at<std::uint16_t>(y, x) = static_cast<std::uint16_t>((x * 37 + y * 101) % 4096);
        m.at<std::uint16_t>(19, 39) = 4095;
        QVERIFY(writeEncoded(path, m, ".tif"));
        const Result<ImageView> r = OpenCvImageSource().load(utf8(path));
        QVERIFY(r.ok());
        QVERIFY(r.value().format == PixelFormat::Gray16);
        QCOMPARE(r.value().significantBits, 12);
        QCOMPARE(gray16At(r.value(), 39, 19), 4095);
    }

    // 事實（S06 實測）：OpenCV 4.9.0 讀 BitsPerSample ＝ 12 的緊密打包 TIFF，給 16UC1、值左移 4 位（0x123 → 0x1230）。
    // 值放滿 16 bit 的範圍，所以有效位元數是 16，亮度的換算（值 ÷ 65535）仍然正確（image_depth.h）。
    void aPackedTwelveBitTiffComesBackShiftedUp() {
        QByteArray b;
        auto u16 = [&](int v) { b.append(static_cast<char>(v & 0xFF)).append(static_cast<char>((v >> 8) & 0xFF)); };
        auto u32 = [&](int v) { u16(v & 0xFFFF); u16((v >> 16) & 0xFFFF); };
        auto entry = [&](int tag, int type, int count, int value) {
            u16(tag);
            u16(type);
            u32(count);
            if (type == 3) {
                u16(value);
                u16(0);
            } else {
                u32(value);
            }
        };
        b.append("II*", 3).append(static_cast<char>(0));
        u32(8);
        const int entries = 8;
        u16(entries);
        entry(256, 3, 1, 4);
        entry(257, 3, 1, 1);
        entry(258, 3, 1, 12);
        entry(259, 3, 1, 1);
        entry(262, 3, 1, 1);
        entry(273, 4, 1, 8 + 2 + entries * 12 + 4);
        entry(278, 3, 1, 1);
        entry(279, 4, 1, 6);
        u32(0);
        for (const int byte : {0x12, 0x3A, 0xBC, 0xFF, 0xF0, 0x01}) b.append(static_cast<char>(byte));  // 0x123 0xABC 0xFFF 0x001
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("packed12.tif"));
        QVERIFY(writeBytes(path, b));
        const Result<ImageView> r = OpenCvImageSource().load(utf8(path));
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        QVERIFY(r.value().format == PixelFormat::Gray16);
        QCOMPARE(gray16At(r.value(), 0, 0), 0x1230);
        QCOMPARE(gray16At(r.value(), 1, 0), 0xABC0);
        QCOMPARE(gray16At(r.value(), 2, 0), 0xFFF0);
        QCOMPARE(gray16At(r.value(), 3, 0), 0x0010);
        QCOMPARE(r.value().significantBits, 16);
    }

    void aJpegLoadsAsBgrx() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("c.jpg"));
        QVERIFY(writeEncoded(path, patternMat(48, 64, CV_8UC3), ".jpg", {cv::IMWRITE_JPEG_QUALITY, 95}));
        const Result<ImageView> r = OpenCvImageSource().load(utf8(path));
        QVERIFY(r.ok());
        QVERIFY(r.value().format == PixelFormat::Bgrx32);
    }

    // 浮點的影像（EXR）解得開，但不是 8／16 bit 整數：FormatInvalid，而且不在 canRead 的清單裡。
    void floatImagesAreNotSupported() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("f.exr"));
        cv::Mat f(8, 8, CV_32FC1, cv::Scalar(0.5));
        if (!writeEncoded(path, f, ".exr")) QSKIP("this OpenCV build cannot write EXR");
        OpenCvImageSource source;
        QVERIFY(!source.canRead(utf8(path)));
        const Result<ImageView> r = source.load(utf8(path));
        QVERIFY(!r.ok());
        QCOMPARE_CODE(r.error().code, ErrorCode::FormatInvalid);
    }

    void aPathWithChineseCharactersLoads() {
        QTemporaryDir dir = makeTempDir();
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("影像 測試")));
        const QString path = dir.filePath(QStringLiteral("影像 測試/板子 一號.tif"));
        QVERIFY(writeEncoded(path, patternMat(8, 12, CV_16UC1), ".tif"));
        const Result<ImageView> r = OpenCvImageSource().load(utf8(path));
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        QVERIFY(r.value().format == PixelFormat::Gray16);
    }

    void fileProblemsGiveTheContractErrorCodes() {
        QTemporaryDir dir = makeTempDir();
        OpenCvImageSource source;
        QCOMPARE_CODE(source.load(utf8(dir.filePath(QStringLiteral("missing.tif")))).error().code, ErrorCode::NotFound);
        QCOMPARE_CODE(source.load("").error().code, ErrorCode::NotFound);
        QCOMPARE_CODE(source.load(utf8(dir.path())).error().code, ErrorCode::IoFailed);
        const QString garbage = dir.filePath(QStringLiteral("garbage.tif"));
        QVERIFY(writeBytes(garbage, "II*\0garbage garbage garbage"));
        QCOMPARE_CODE(source.load(utf8(garbage)).error().code, ErrorCode::FormatInvalid);
        const QString empty = dir.filePath(QStringLiteral("empty.png"));
        QVERIFY(writeBytes(empty, QByteArray()));
        QCOMPARE_CODE(source.load(utf8(empty)).error().code, ErrorCode::FormatInvalid);
    }

    void aTruncatedFileDoesNotCrash() {
        QTemporaryDir dir = makeTempDir();
        std::vector<uchar> bytes;
        QVERIFY(cv::imencode(".png", patternMat(100, 200, CV_8UC3), bytes));
        OpenCvImageSource source;
        for (const double fraction : {0.1, 0.5, 0.9}) {
            const QString path = dir.filePath(QStringLiteral("cut-%1.png").arg(fraction));
            QVERIFY(writeBytes(path, QByteArray(reinterpret_cast<const char*>(bytes.data()),
                                                static_cast<qsizetype>(static_cast<double>(bytes.size()) * fraction))));
            const Result<ImageView> r = source.load(utf8(path));
            if (r.ok()) {
                qInfo("truncated PNG at %.0f%%: OpenCV returned an image of %dx%d", fraction * 100, r.value().width, r.value().height);
            } else {
                qInfo("truncated PNG at %.0f%%: %s", fraction * 100, r.error().message.c_str());
                QCOMPARE_CODE(r.error().code, ErrorCode::FormatInvalid);
            }
        }
    }

    void canReadKnowsTheSuffixes() {
        OpenCvImageSource source;
        for (const char* p : {"a.tif", "A.TIFF", "b.png", "c.bmp", "d.jpg", "e.pgm"}) QVERIFY2(source.canRead(p), p);
        for (const char* p : {"a.txt", "a.exr", "noext", "dir.tif/x"}) QVERIFY2(!source.canRead(p), p);
    }

    void loadIsReentrant() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("shared.tif"));
        const cv::Mat original = patternMat(200, 300, CV_8UC3);
        QVERIFY(writeEncoded(path, original, ".tif"));
        OpenCvImageSource source;
        const std::string p = utf8(path);
        std::atomic<int> failures{0};
        std::vector<std::thread> threads;
        for (int t = 0; t < 6; ++t) {
            threads.emplace_back([&] {
                for (int i = 0; i < 5; ++i) {
                    const Result<ImageView> r = source.load(p);
                    if (!r.ok() || byteAt(r.value(), 10, 10, 0) != original.at<cv::Vec3b>(10, 10)[0]) ++failures;
                }
            });
        }
        for (std::thread& t : threads) t.join();
        QCOMPARE(failures.load(), 0);
    }
};

QTEST_GUILESS_MAIN(TstOpenCvImage)
#include "tst_opencv_image.moc"
