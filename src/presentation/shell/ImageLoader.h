#pragma once

#include <QImage>
#include <QString>

namespace aoi::ui {

// 影像檔 → QImage。S02 暫用的 Qt 載入路徑；S06 會以 IImageSource 取代，那時畫布改收 ImageView。
struct LoadedImage {
    QImage image;
    double decodeMs = 0;  // 解碼（含轉成可快速繪製的格式）所花的時間
    QString error;        // 失敗時有值

    bool ok() const { return !image.isNull(); }
};

LoadedImage loadImageFile(const QString& path);

}  // namespace aoi::ui
