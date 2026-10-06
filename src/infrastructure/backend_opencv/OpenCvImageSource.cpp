#include "infrastructure/backend_opencv/OpenCvImageSource.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <new>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <system_error>
#include <vector>

#include "domain/image_copy.h"
#include "domain/image_depth.h"
#include "infrastructure/backend_opencv/OpenCvImages.h"

namespace aoi::infra {
namespace {

// 這個 OpenCV 建置確定解得開、而且解出來是 8／16 bit 整數的格式。EXR、HDR 解出浮點數，不在清單裡。
constexpr std::array<std::string_view, 14> kSuffixes{"bmp", "dib", "jpe", "jpeg", "jpg", "pbm", "pgm", "png",
                                                     "pnm", "ppm", "pxm", "ras", "tif", "tiff"};

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

std::filesystem::path fsPath(std::string_view utf8) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

std::string quoted(std::string_view path) { return "'" + std::string(path) + "'"; }

// 轉換的結果寫進新的緩衝：記一次複製（image_copy.h）。
void countMat(const cv::Mat& m) { countImageCopy(m.total() * m.elemSize()); }

// 解碼結果 → 正規格式（Gray8、Gray16、Bgrx32）。不支援的型別回傳空的 Mat。
cv::Mat toCanonical(const cv::Mat& decoded) {
    const int channels = decoded.channels();
    cv::Mat eight = decoded;
    if (decoded.depth() == CV_16U && channels != 1) {  // 16 bit 彩色 → 8 bit（四捨五入 v / 257，和 Qt 的轉換相同）
        decoded.convertTo(eight, CV_8U, 1.0 / 257.0);
        countMat(eight);
    } else if (decoded.depth() != CV_8U && decoded.depth() != CV_16U) {
        return {};
    }
    switch (channels) {
        case 1: return eight;  // Gray8 或 Gray16
        case 2: {              // 灰階＋alpha：和 Qt 一樣當成彩色（Qt 解成 ARGB32）
            cv::Mat gray;
            cv::extractChannel(eight, gray, 0);
            countMat(gray);
            cv::Mat bgrx;
            cv::cvtColor(gray, bgrx, cv::COLOR_GRAY2BGRA);
            countMat(bgrx);
            return bgrx;
        }
        case 3: {
            cv::Mat bgrx;
            cv::cvtColor(eight, bgrx, cv::COLOR_BGR2BGRA);
            countMat(bgrx);
            return bgrx;
        }
        case 4: return eight;
        default: return {};
    }
}

}  // namespace

bool OpenCvImageSource::canRead(std::string_view path) const {
    const std::string s = suffixOf(path);
    return std::find(kSuffixes.begin(), kSuffixes.end(), s) != kSuffixes.end();
}

Result<ImageView> OpenCvImageSource::load(std::string_view path) {
    try {
        if (path.empty()) return makeError(ErrorCode::NotFound, "The image file does not exist: ''.");
        const std::filesystem::path file = fsPath(path);
        std::error_code ec;
        const auto status = std::filesystem::status(file, ec);
        if (ec || !std::filesystem::exists(status)) {
            return makeError(ErrorCode::NotFound, "The image file does not exist: " + quoted(path) + ".");
        }
        if (!std::filesystem::is_regular_file(status)) {
            return makeError(ErrorCode::IoFailed, "The image path is not a file: " + quoted(path) + ".");
        }
        const std::uintmax_t size = std::filesystem::file_size(file, ec);
        if (ec) return makeError(ErrorCode::IoFailed, "Cannot read the size of " + quoted(path) + ".");
        if (size == 0) return makeError(ErrorCode::FormatInvalid, "OpenCV cannot decode " + quoted(path) + ": the file is empty.");

        std::vector<uchar> bytes(static_cast<std::size_t>(size));
        std::ifstream in(file, std::ios::binary);
        if (!in) return makeError(ErrorCode::IoFailed, "Cannot open " + quoted(path) + ".");
        in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (in.gcount() != static_cast<std::streamsize>(bytes.size())) return makeError(ErrorCode::IoFailed, "Cannot read " + quoted(path) + ".");

        cv::Mat decoded;
        try {
            decoded = cv::imdecode(bytes, cv::IMREAD_UNCHANGED);  // 保留位元深度與 alpha，不套用 EXIF 的旋轉
        } catch (const cv::Exception& e) {
            return makeError(ErrorCode::FormatInvalid, "OpenCV cannot decode " + quoted(path) + ": " + e.what());
        }
        if (decoded.empty()) return makeError(ErrorCode::FormatInvalid, "OpenCV cannot decode " + quoted(path) + ".");
        bytes = {};  // 編碼的資料不再需要

        const cv::Mat canonical = toCanonical(decoded);
        if (canonical.empty()) {
            return makeError(ErrorCode::FormatInvalid, "OpenCV decoded " + quoted(path) +
                                                           ", but its pixel type is not supported (only 8 and 16 bit integers).");
        }
        ImageView view = fromMat(canonical);
        if (!view.valid()) return makeError(ErrorCode::Internal, "The decoded image of " + quoted(path) + " cannot be wrapped.");
        view.significantBits = detectSignificantBits(view);  // 和 Qt 的來源同一個函式：同一個檔案給同一個值
        return view;
    } catch (const std::bad_alloc&) {
        return makeError(ErrorCode::IoFailed, "Not enough memory to decode " + quoted(path) + ".");
    } catch (const cv::Exception& e) {
        return makeError(ErrorCode::Internal, std::string("OpenCV failed while converting ") + quoted(path) + ": " + e.what());
    } catch (...) {
        return makeError(ErrorCode::Internal, "Unexpected exception while decoding " + quoted(path) + ".");
    }
}

}  // namespace aoi::infra
