#include "application/image/ImageSourceChain.h"

#include <optional>
#include <string>
#include <utility>

namespace aoi {

void ImageSourceChain::add(std::unique_ptr<IImageSource> source) {
    if (source) sources_.push_back(std::move(source));
}

bool ImageSourceChain::canRead(std::string_view path) const {
    for (const auto& source : sources_) {
        if (source->canRead(path)) return true;
    }
    return false;
}

Result<ImageView> ImageSourceChain::load(std::string_view path) {
    if (sources_.empty()) return makeError(ErrorCode::FormatInvalid, "No image decoder is available.");

    std::string reasons;
    bool tried = false;
    auto attempt = [&](IImageSource& source) -> std::optional<Result<ImageView>> {
        tried = true;
        Result<ImageView> r = source.load(path);
        if (r.ok() || r.error().code != ErrorCode::FormatInvalid) return r;
        if (!reasons.empty()) reasons += " | ";
        reasons += r.error().message;
        return std::nullopt;
    };

    for (const auto& source : sources_) {
        if (!source->canRead(path)) continue;
        if (auto r = attempt(*source)) return std::move(*r);
    }
    if (!tried) {
        if (auto r = attempt(*sources_.back())) return std::move(*r);
    }
    return makeError(ErrorCode::FormatInvalid, reasons.empty() ? std::string("No decoder could read the file.") : reasons);
}

}  // namespace aoi
