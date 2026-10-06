#include "infrastructure/image_qt/QtImageSource.h"

#include <QByteArray>
#include <QFileInfo>
#include <QImageReader>
#include <QString>
#include <algorithm>
#include <new>

#include "domain/image_depth.h"
#include "infrastructure/image_qt/QtImages.h"

namespace aoi::infra {
namespace {

// 檔名最後一段的副檔名（小寫 ASCII，不含點）；沒有就是空字串。
std::string suffixOf(std::string_view path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) return {};
    std::string s(path.substr(dot + 1));
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

std::string text(const QString& s) { return s.toUtf8().toStdString(); }

Error readerError(const QImageReader& reader, std::string_view path) {
    const std::string why = "Qt cannot read '" + std::string(path) + "': " + text(reader.errorString());
    switch (reader.error()) {
        case QImageReader::FileNotFoundError: return makeError(ErrorCode::NotFound, why);
        case QImageReader::DeviceError: return makeError(ErrorCode::IoFailed, why);
        case QImageReader::UnsupportedFormatError:
        case QImageReader::InvalidDataError:
        case QImageReader::UnknownError: break;
    }
    return makeError(ErrorCode::FormatInvalid, why);
}

}  // namespace

QtImageSource::QtImageSource() {
    // Qt 預設只允許 256 MB 的解碼配置；25 MP 的 RGB32 是 100 MB，上限抬到 1 GB 當防呆（全程式共用的設定）。
    QImageReader::setAllocationLimit(1024);
    for (const QByteArray& f : QImageReader::supportedImageFormats()) suffixes_.push_back(f.toLower().toStdString());
    std::sort(suffixes_.begin(), suffixes_.end());
    suffixes_.erase(std::unique(suffixes_.begin(), suffixes_.end()), suffixes_.end());
}

bool QtImageSource::canRead(std::string_view path) const {
    const std::string s = suffixOf(path);
    return !s.empty() && std::binary_search(suffixes_.begin(), suffixes_.end(), s);
}

Result<ImageView> QtImageSource::load(std::string_view path) {
    try {
        const QString file = QString::fromUtf8(path.data(), static_cast<qsizetype>(path.size()));
        const QFileInfo info(file);
        if (file.isEmpty() || !info.exists()) {
            return makeError(ErrorCode::NotFound, "The image file does not exist: '" + std::string(path) + "'.");
        }
        if (!info.isFile()) return makeError(ErrorCode::IoFailed, "The image path is not a file: '" + std::string(path) + "'.");

        QImageReader reader(file);
        reader.setAutoTransform(false);         // 不套用 EXIF 的旋轉：座標要對到感測器的像素
        reader.setDecideFormatFromContent(true);  // 副檔名錯了也讀得到（和 OpenCV 的 imdecode 一樣看內容）
        QImage image = reader.read();
        if (image.isNull()) return readerError(reader, path);

        image = toCanonicalQImage(std::move(image));
        if (image.isNull()) return makeError(ErrorCode::IoFailed, "Not enough memory to convert '" + std::string(path) + "'.");
        ImageView view = fromQImage(image);
        if (!view.valid()) return makeError(ErrorCode::IoFailed, "Not enough memory for '" + std::string(path) + "'.");
        view.significantBits = detectSignificantBits(view);  // 12 bit 的影像常存成 16 bit、值只到 4095（image_depth.h）
        return view;
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::IoFailed, "Not enough memory to decode '" + std::string(path) + "'.");
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while decoding '" + std::string(path) + "'.");
    }
}

}  // namespace aoi::infra
