// S06：ImageView 的所有權規則、ImageBuffer、subView 與複製計數（domain，只用標準庫）。
// 規則的編號見 src/domain/image_view.h 與 ADR-014。
#include <QtTest>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#include "domain/image_buffer.h"
#include "domain/image_copy.h"
#include "domain/image_depth.h"
#include "domain/image_view.h"

using namespace aoi;

class TstImageView : public QObject {
    Q_OBJECT

private slots:
    // ---- 像素格式 ----
    void everyFormatHasASizeAndAName_data() {
        QTest::addColumn<int>("format");
        QTest::addColumn<int>("bytes");
        QTest::addColumn<QString>("name");
        QTest::newRow("Gray8") << int(PixelFormat::Gray8) << 1 << "Gray8";
        QTest::newRow("Rgb24") << int(PixelFormat::Rgb24) << 3 << "Rgb24";
        QTest::newRow("Bgrx32") << int(PixelFormat::Bgrx32) << 4 << "Bgrx32";
        QTest::newRow("Bgr24") << int(PixelFormat::Bgr24) << 3 << "Bgr24";
        QTest::newRow("Gray16") << int(PixelFormat::Gray16) << 2 << "Gray16";
    }
    void everyFormatHasASizeAndAName() {
        QFETCH(int, format);
        QFETCH(int, bytes);
        QFETCH(QString, name);
        const auto f = static_cast<PixelFormat>(format);
        QCOMPARE(bytesPerPixel(f), bytes);
        QCOMPARE(QString::fromUtf8(toString(f).data(), static_cast<qsizetype>(toString(f).size())), name);
    }

    // S06 之前的三個格式，數值沒有變（S02–S05 的程式與測試依賴它們）。
    void theOldFormatsKeepTheirValues() {
        QCOMPARE(static_cast<int>(PixelFormat::Gray8), 0);
        QCOMPARE(static_cast<int>(PixelFormat::Rgb24), 1);
        QCOMPARE(static_cast<int>(PixelFormat::Bgrx32), 2);
    }

    void validityChecksTheStrideAgainstTheRow() {
        std::vector<std::uint8_t> pixels(64 * 10);
        ImageView v;
        v.data = pixels.data();
        v.width = 16;
        v.height = 10;
        v.format = PixelFormat::Bgrx32;
        v.strideBytes = 64;
        QVERIFY(v.valid());
        QVERIFY(v.packed());
        v.strideBytes = 63;
        QVERIFY(!v.valid());
        v.format = PixelFormat::Gray16;  // 16 × 2 ＝ 32 ≤ 63
        QVERIFY(v.valid());
        QVERIFY(!v.packed());
        // 寬很大時不會因為 int 溢位而誤判成有效
        v.width = 0x40000000;
        v.strideBytes = 0x7fffffff;
        v.format = PixelFormat::Bgrx32;
        QVERIFY(!v.valid());
    }

    // ---- ImageBuffer ----
    void aBufferIsPackedAlignedAndZeroed_data() {
        QTest::addColumn<int>("format");
        QTest::newRow("Gray8") << int(PixelFormat::Gray8);
        QTest::newRow("Rgb24") << int(PixelFormat::Rgb24);
        QTest::newRow("Bgrx32") << int(PixelFormat::Bgrx32);
        QTest::newRow("Bgr24") << int(PixelFormat::Bgr24);
        QTest::newRow("Gray16") << int(PixelFormat::Gray16);
    }
    void aBufferIsPackedAlignedAndZeroed() {
        QFETCH(int, format);
        const auto f = static_cast<PixelFormat>(format);
        const ImageBuffer b(37, 11, f);  // 奇數寬：看得出有沒有偷偷補齊
        QVERIFY(b.valid());
        QCOMPARE(b.strideBytes(), 37 * bytesPerPixel(f));
        QCOMPARE(b.sizeBytes(), static_cast<std::size_t>(37 * 11 * bytesPerPixel(f)));
        QCOMPARE(reinterpret_cast<std::uintptr_t>(b.data()) % ImageBuffer::kAlignment, std::uintptr_t{0});
        for (std::size_t i = 0; i < b.sizeBytes(); ++i) QCOMPARE(b.data()[i], std::uint8_t{0});
        const ImageView v = b.view(4.5);
        QVERIFY(v.valid());
        QVERIFY(v.packed());
        QVERIFY(v.format == f);
        QCOMPARE(v.umPerPixel, 4.5);
        QVERIFY(v.data == b.data());
        QCOMPARE(b.row(3), b.data() + 3 * b.strideBytes());
    }

    void invalidSizesGiveAnInvalidBufferWithoutThrowing_data() {
        QTest::addColumn<int>("width");
        QTest::addColumn<int>("height");
        QTest::newRow("zero width") << 0 << 10;
        QTest::newRow("negative height") << 10 << -1;
        QTest::newRow("side too long") << ImageBuffer::kMaxSide + 1 << 1;
        QTest::newRow("over 2 GiB") << 65535 << 65535;  // Bgrx32：16 GiB
    }
    void invalidSizesGiveAnInvalidBufferWithoutThrowing() {
        QFETCH(int, width);
        QFETCH(int, height);
        const ImageBuffer b(width, height, PixelFormat::Bgrx32);
        QVERIFY(!b.valid());
        QVERIFY(!b.view().valid());
        QVERIFY(b.view().owner == nullptr);
    }

    // 規則 2、3：view 共用擁有權；緩衝物件消失之後，像素活到最後一個 view 消失。
    void theViewKeepsThePixelsAliveAfterTheBufferIsGone() {
        ImageView v;
        {
            ImageBuffer b(8, 8, PixelFormat::Gray8);
            b.row(2)[5] = 99;
            v = b.view();
            QCOMPARE(v.owner.use_count(), 2L);  // 緩衝＋view
        }
        QCOMPARE(v.owner.use_count(), 1L);
        QCOMPARE(v.data[2 * v.strideBytes + 5], std::uint8_t{99});
        const ImageView copy = v;  // 複製 view 只加參照計數，不複製像素
        QVERIFY(copy.data == v.data);
        QCOMPARE(v.owner.use_count(), 2L);
    }

    // ---- subView（規則 4）----
    void aSubViewSharesThePixelsAndTheOwner() {
        ImageBuffer b(20, 10, PixelFormat::Bgr24);
        b.row(4)[3 * 7 + 1] = 42;  // (7, 4) 的 G
        const ImageView whole = b.view(2.0);
        const ImageView sub = subView(whole, 7, 4, 5, 3);
        QVERIFY(sub.valid());
        QCOMPARE(sub.width, 5);
        QCOMPARE(sub.height, 3);
        QCOMPARE(sub.strideBytes, whole.strideBytes);  // stride 不變：不是緊密排列了
        QVERIFY(!sub.packed());
        QVERIFY(sub.data == whole.data + 4 * whole.strideBytes + 7 * 3);
        QCOMPARE(sub.data[1], std::uint8_t{42});
        QVERIFY(sub.owner == whole.owner);
        QCOMPARE(sub.umPerPixel, 2.0);
        // 子影像的子影像
        const ImageView subSub = subView(sub, 1, 1, 2, 2);
        QVERIFY(subSub.data == whole.data + 5 * whole.strideBytes + 8 * 3);
    }

    void aSubViewOutsideTheImageIsInvalid_data() {
        QTest::addColumn<int>("x");
        QTest::addColumn<int>("y");
        QTest::addColumn<int>("w");
        QTest::addColumn<int>("h");
        QTest::addColumn<bool>("expectValid");
        QTest::newRow("whole image is fine") << 0 << 0 << 20 << 10 << true;
        QTest::newRow("last pixel is fine") << 19 << 9 << 1 << 1 << true;
        QTest::newRow("one column too far") << 1 << 0 << 20 << 10 << false;
        QTest::newRow("one row too far") << 0 << 1 << 20 << 10 << false;
        QTest::newRow("negative x") << -1 << 0 << 5 << 5 << false;
        QTest::newRow("zero width") << 3 << 3 << 0 << 2 << false;
        QTest::newRow("overflowing width") << 5 << 0 << 0x7fffffff << 2 << false;
    }
    void aSubViewOutsideTheImageIsInvalid() {
        QFETCH(int, x);
        QFETCH(int, y);
        QFETCH(int, w);
        QFETCH(int, h);
        QFETCH(bool, expectValid);
        const ImageBuffer b(20, 10, PixelFormat::Gray8);
        QCOMPARE(subView(b.view(), x, y, w, h).valid(), expectValid);
        QVERIFY(!subView(ImageView{}, 0, 0, 1, 1).valid());
    }

    // ---- 位元深度（12 bit 的影像；image_depth.h）----
    void theBitDepthOfGray16IsInferredFromTheLargestValue_data() {
        QTest::addColumn<int>("maxValue");
        QTest::addColumn<int>("bits");
        QTest::newRow("all zero") << 0 << 8;
        QTest::newRow("255") << 255 << 8;
        QTest::newRow("256") << 256 << 10;
        QTest::newRow("1023") << 1023 << 10;
        QTest::newRow("1024") << 1024 << 12;
        QTest::newRow("4095 (12 bit)") << 4095 << 12;
        QTest::newRow("4096") << 4096 << 14;
        QTest::newRow("16383") << 16383 << 14;
        QTest::newRow("16384") << 16384 << 16;
        QTest::newRow("65535") << 65535 << 16;
        // OpenCV 讀 12 bit 緊密打包的 TIFF 會左移 4 位：0xFFF → 0xFFF0，當成 16 bit 是對的（image_depth.h）
        QTest::newRow("12 bit shifted up") << 0xFFF0 << 16;
    }
    void theBitDepthOfGray16IsInferredFromTheLargestValue() {
        QFETCH(int, maxValue);
        QFETCH(int, bits);
        ImageBuffer b(7, 5, PixelFormat::Gray16);  // 奇數寬：列的起點不是 4 byte 對齊
        const auto v = static_cast<std::uint16_t>(maxValue);
        std::memcpy(b.row(4) + 2 * 6, &v, sizeof v);  // 最後一個像素：掃描要走到底
        const std::uint16_t small = 3;
        std::memcpy(b.row(0), &small, sizeof small);
        QCOMPARE(detectSignificantBits(b.view()), bits);
        // 子影像（stride 比一列大）也一樣
        ImageBuffer wide(20, 5, PixelFormat::Gray16);
        std::memcpy(wide.row(4) + 2 * 9, &v, sizeof v);
        QCOMPARE(detectSignificantBits(subView(wide.view(), 3, 1, 7, 4)), bits);
    }

    void onlyGray16HasAnInferredDepth() {
        QCOMPARE(detectSignificantBits(ImageBuffer(4, 4, PixelFormat::Gray8).view()), 0);
        QCOMPARE(detectSignificantBits(ImageBuffer(4, 4, PixelFormat::Bgrx32).view()), 0);
        QCOMPARE(detectSignificantBits(ImageView{}), 0);
        ImageView v = ImageBuffer(4, 4, PixelFormat::Gray16).view();
        QCOMPARE(effectiveBits(v), 16);  // 0 ＝ 格式的完整位元數
        v.significantBits = 12;
        QCOMPARE(effectiveBits(v), 12);
        QCOMPARE(effectiveBits(ImageBuffer(4, 4, PixelFormat::Rgb24).view()), 8);
        QCOMPARE(normalizedMax(12), 4095);
        QCOMPARE(normalizedMax(16), 65535);
        QCOMPARE(normalizedMax(8), 255);
        QCOMPARE(normalizedMax(0), 65535);
    }

    // ---- 複製計數（規則 6）----
    void theCopyCounterAddsCopiesAndBytes() {
        const ImageCopyCount before = imageCopyCount();
        countImageCopy(100);
        countImageCopy(28);
        const ImageCopyCount delta = imageCopyCount() - before;
        QCOMPARE(delta.copies, std::uint64_t{2});
        QCOMPARE(delta.bytes, std::uint64_t{128});
    }

    // 建立緩衝、交出 view、取子影像都是零複製：計數不動。
    void buffersViewsAndSubViewsDoNotCount() {
        const ImageCopyCount before = imageCopyCount();
        const ImageBuffer b(64, 64, PixelFormat::Bgrx32);
        const ImageView v = b.view();
        const ImageView sub = subView(v, 8, 8, 16, 16);
        ImageView copy = sub;
        QVERIFY(copy.valid());
        const ImageCopyCount delta = imageCopyCount() - before;
        QCOMPARE(delta.copies, std::uint64_t{0});
        QCOMPARE(delta.bytes, std::uint64_t{0});
    }

    void theCopyCounterIsThreadSafe() {
        const ImageCopyCount before = imageCopyCount();
        std::vector<std::thread> threads;
        for (int t = 0; t < 8; ++t) {
            threads.emplace_back([] {
                for (int i = 0; i < 10000; ++i) countImageCopy(3);
            });
        }
        for (std::thread& t : threads) t.join();
        const ImageCopyCount delta = imageCopyCount() - before;
        QCOMPARE(delta.copies, std::uint64_t{80000});
        QCOMPARE(delta.bytes, std::uint64_t{240000});
    }

    // 規則 1 的前提：多個執行緒同時讀同一塊像素（只讀）不需要鎖，結果一致。
    void manyThreadsReadTheSamePixels() {
        ImageBuffer b(256, 256, PixelFormat::Gray8);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x) b.row(y)[x] = static_cast<std::uint8_t>(x ^ y);
        const ImageView v = b.view();
        std::atomic<int> mismatches{0};
        std::vector<std::thread> threads;
        for (int t = 0; t < 6; ++t) {
            threads.emplace_back([v, &mismatches] {  // 每個執行緒自己一份 view（共用 owner）
                std::uint64_t sum = 0;
                for (int y = 0; y < v.height; ++y)
                    for (int x = 0; x < v.width; ++x) sum += v.data[y * v.strideBytes + x];
                if (sum != 256ull * 256ull * 255ull / 2ull) ++mismatches;
            });
        }
        for (std::thread& t : threads) t.join();
        QCOMPARE(mismatches.load(), 0);
    }
};

QTEST_APPLESS_MAIN(TstImageView)
#include "tst_image_view.moc"
