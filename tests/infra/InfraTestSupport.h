#pragma once

#include <QDir>
#include <QFile>
#include <QImage>
#include <QString>
#include <QTemporaryDir>
#include <QtTest>
#include <cstdint>
#include <cstring>
#include <string>

#include "domain/image_copy.h"
#include "domain/image_view.h"

// tests/infra 共用的小工具（S06）。
namespace aoi::test {

// 在目前的資料夾（建置目錄）底下開一個暫存資料夾：路徑短，不會碰到 Windows 的 260 字元上限。
inline QTemporaryDir makeTempDir() { return QTemporaryDir(QDir::current().filePath(QStringLiteral("aoi-infra-XXXXXX"))); }

inline std::string utf8(const QString& s) { return s.toUtf8().toStdString(); }

inline bool writeBytes(const QString& path, const QByteArray& bytes) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    return f.write(bytes) == bytes.size();
}

inline QByteArray readBytes(const QString& path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// 計數器的讀數差：建立時記下，copies() 是之後的複製次數。
class CopyWatch {
public:
    CopyWatch() : before_(imageCopyCount()) {}
    std::uint64_t copies() const { return (imageCopyCount() - before_).copies; }
    std::uint64_t bytes() const { return (imageCopyCount() - before_).bytes; }

private:
    ImageCopyCount before_;
};

// 在 JPEG 的 SOI 後面插入一個 EXIF（APP1）區段，只有一個欄位：Orientation（0x0112）＝ orientation。
// 6 ＝「要順時針轉 90° 才是正的」：套用旋轉的讀取器會把 W × H 讀成 H × W。
inline QByteArray withExifOrientation(const QByteArray& jpeg, int orientation) {
    QByteArray tiff;
    auto u16 = [&](int v) { tiff.append(static_cast<char>(v & 0xFF)).append(static_cast<char>((v >> 8) & 0xFF)); };
    auto u32 = [&](int v) { u16(v & 0xFFFF); u16((v >> 16) & 0xFFFF); };
    tiff.append("II*", 3).append(static_cast<char>(0));
    u32(8);
    u16(1);           // 一個欄位
    u16(0x0112);      // Orientation
    u16(3);           // SHORT
    u32(1);
    u16(orientation);
    u16(0);
    u32(0);           // 沒有下一個 IFD
    QByteArray app1("Exif", 4);
    app1.append(static_cast<char>(0)).append(static_cast<char>(0)).append(tiff);
    const int length = static_cast<int>(app1.size()) + 2;
    QByteArray segment;
    segment.append(static_cast<char>(0xFF)).append(static_cast<char>(0xE1));
    segment.append(static_cast<char>((length >> 8) & 0xFF)).append(static_cast<char>(length & 0xFF));
    segment.append(app1);
    return jpeg.left(2) + segment + jpeg.mid(2);  // SOI 之後
}

// 一個有紋理的測試影像（每個像素都不同，看得出通道與列有沒有錯位）。
inline QImage patternImage(int w, int h, QImage::Format format) {
    QImage img(w, h, QImage::Format_ARGB32);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) img.setPixel(x, y, qRgba((x * 7 + y) & 255, (x + y * 3) & 255, (x * y + 11) & 255, 255));
    return img.convertToFormat(format);
}

// ImageView 的 (x, y) 第 c 個 byte（Bgrx32、Bgr24、Rgb24 的通道，Gray8 的值）；Gray16 用 gray16At 讀完整的 16 bit。
inline int byteAt(const ImageView& v, int x, int y, int c) {
    return v.data[static_cast<std::size_t>(y) * static_cast<std::size_t>(v.strideBytes) +
                  static_cast<std::size_t>(x) * static_cast<std::size_t>(bytesPerPixel(v.format)) + static_cast<std::size_t>(c)];
}
inline int gray16At(const ImageView& v, int x, int y) {
    std::uint16_t value = 0;
    std::memcpy(&value, v.data + static_cast<std::size_t>(y) * static_cast<std::size_t>(v.strideBytes) + static_cast<std::size_t>(x) * 2, 2);
    return value;
}

}  // namespace aoi::test
