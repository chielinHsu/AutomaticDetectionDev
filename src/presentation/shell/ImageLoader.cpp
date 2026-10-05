#include "presentation/shell/ImageLoader.h"

#include <QElapsedTimer>
#include <QImageReader>

namespace aoi::ui {

LoadedImage loadImageFile(const QString& path) {
    // Qt 預設只允許 256 MB 的解碼配置；5 MP、25 MP 的 RGB 影像是 20 MB、100 MB，上限抬到 1 GB 當防呆。
    QImageReader::setAllocationLimit(1024);

    LoadedImage out;
    QElapsedTimer timer;
    timer.start();
    QImageReader reader(path);
    reader.setAutoTransform(false);
    QImage image = reader.read();
    if (image.isNull()) {
        out.error = reader.errorString();
        return out;
    }
    // 8 bit 灰階常被解成索引色影像，繪製時要查調色盤；轉成灰階格式一次，之後每幀都省。
    if (image.format() == QImage::Format_Indexed8 && image.isGrayscale())
        image.convertTo(QImage::Format_Grayscale8);
    out.decodeMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
    out.image = std::move(image);
    return out;
}

}  // namespace aoi::ui
