// S06：組裝根的 makeImageSource()（產品真正用的影像來源）：Qt 在前、OpenCV 在後（ADR-014）。
// AOI_WITH_OPENCV=ON 與 OFF 各建置一次，兩種情況都要對。
#include <QImage>
#include <QtTest>

#include "InfraTestSupport.h"
#include "QtTestPrinters.h"
#include "app/CompositionRoot.h"

using namespace aoi;
using namespace aoi::test;

namespace {

// 手寫的最小 TIFF：2 × 2、8 bit 灰階、未壓縮、小端。像素 10、20／30、40。
QByteArray tinyTiff() {
    QByteArray b;
    auto u16 = [&](int v) { b.append(static_cast<char>(v & 0xFF)).append(static_cast<char>((v >> 8) & 0xFF)); };
    auto u32 = [&](int v) { u16(v & 0xFFFF); u16((v >> 16) & 0xFFFF); };
    auto entry = [&](int tag, int type, int count, int value) {
        u16(tag);
        u16(type);
        u32(count);
        if (type == 3) {  // SHORT：值放在前兩個 byte
            u16(value);
            u16(0);
        } else {
            u32(value);
        }
    };
    b.append("II*", 3).append('\0');
    u32(8);  // 第一個 IFD 的位置
    const int entries = 8;
    const int dataOffset = 8 + 2 + entries * 12 + 4;
    u16(entries);
    entry(256, 3, 1, 2);           // ImageWidth
    entry(257, 3, 1, 2);           // ImageLength
    entry(258, 3, 1, 8);           // BitsPerSample
    entry(259, 3, 1, 1);           // Compression：無
    entry(262, 3, 1, 1);           // Photometric：黑是 0
    entry(273, 4, 1, dataOffset);  // StripOffsets
    entry(278, 3, 1, 2);           // RowsPerStrip
    entry(279, 4, 1, 4);           // StripByteCounts
    u32(0);                        // 沒有下一個 IFD
    b.append(static_cast<char>(10)).append(static_cast<char>(20)).append(static_cast<char>(30)).append(static_cast<char>(40));
    return b;
}

}  // namespace

class TstImageSourceComposition : public QObject {
    Q_OBJECT

private slots:
    void commonFilesComeFromQtWithoutCopies() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("board.png"));
        QVERIFY(patternImage(80, 60, QImage::Format_RGB32).save(path));
        const std::unique_ptr<IImageSource> source = app::makeImageSource();
        QVERIFY(source->canRead(utf8(path)));
        const CopyWatch watch;
        const Result<ImageView> r = source->load(utf8(path));
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        QVERIFY(r.value().format == PixelFormat::Bgrx32);
        QCOMPARE(watch.copies(), std::uint64_t{0});  // Qt 先問：常見格式零複製
    }

    void tiffNeedsOpenCv() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("tiny.tif"));
        QVERIFY(writeBytes(path, tinyTiff()));
        const std::unique_ptr<IImageSource> source = app::makeImageSource();
        const Result<ImageView> r = source->load(utf8(path));
#ifdef AOI_WITH_OPENCV
        QVERIFY(source->canRead(utf8(path)));
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        QVERIFY(r.value().format == PixelFormat::Gray8);
        QCOMPARE(r.value().width, 2);
        QCOMPARE(byteAt(r.value(), 0, 0, 0), 10);
        QCOMPARE(byteAt(r.value(), 1, 1, 0), 40);
#else
        // 沒有 OpenCV：這台機器的 Qt 沒有 TIFF 外掛，認不得 .tif；交給最後一個來源（Qt）看內容也解不開 → FormatInvalid。
        QVERIFY(!source->canRead(utf8(path)));
        QVERIFY(!r.ok());
        QCOMPARE_CODE(r.error().code, ErrorCode::FormatInvalid);
#endif
    }

    void aMissingFileIsNotFound() {
        const std::unique_ptr<IImageSource> source = app::makeImageSource();
        QCOMPARE_CODE(source->load("no-such-file.png").error().code, ErrorCode::NotFound);
        QCOMPARE_CODE(source->load("no-such-file.tif").error().code, ErrorCode::NotFound);
    }
};

QTEST_GUILESS_MAIN(TstImageSourceComposition)
#include "tst_image_source_composition.moc"
