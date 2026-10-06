#include "domain/image_buffer.h"

#include <cstring>
#include <new>

namespace aoi {
namespace {

struct AlignedDelete {
    void operator()(std::uint8_t* p) const noexcept { ::operator delete(p, std::align_val_t{ImageBuffer::kAlignment}); }
};

}  // namespace

ImageBuffer::ImageBuffer(int width, int height, PixelFormat format) {
    if (width <= 0 || height <= 0 || width > kMaxSide || height > kMaxSide) return;
    const std::size_t stride = static_cast<std::size_t>(width) * static_cast<std::size_t>(bytesPerPixel(format));
    if (stride == 0) return;
    const std::size_t bytes = stride * static_cast<std::size_t>(height);
    if (bytes > kMaxBytes) return;

    void* raw = ::operator new(bytes, std::align_val_t{kAlignment}, std::nothrow);
    if (raw == nullptr) return;
    auto* p = static_cast<std::uint8_t*>(raw);
    std::memset(p, 0, bytes);
    try {
        pixels_ = std::shared_ptr<std::uint8_t>(p, AlignedDelete{});  // 控制區塊配置失敗時，shared_ptr 會用 deleter 釋放 p
    } catch (const std::bad_alloc&) {
        return;
    }
    width_ = width;
    height_ = height;
    strideBytes_ = static_cast<int>(stride);
    format_ = format;
}

ImageView ImageBuffer::view(double umPerPixel) const {
    ImageView v;
    if (!valid()) return v;
    v.data = pixels_.get();
    v.width = width_;
    v.height = height_;
    v.strideBytes = strideBytes_;
    v.format = format_;
    v.owner = pixels_;
    v.umPerPixel = umPerPixel;
    return v;
}

}  // namespace aoi
