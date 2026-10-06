#include "domain/image_copy.h"

#include <atomic>

namespace aoi {
namespace {

// 兩個計數各自是 atomic：讀數時兩者可能差一次正在進行的複製，計數的用途（前後差）不受影響。
std::atomic<std::uint64_t> gCopies{0};
std::atomic<std::uint64_t> gBytes{0};

}  // namespace

void countImageCopy(std::size_t bytes) noexcept {
    gCopies.fetch_add(1, std::memory_order_relaxed);
    gBytes.fetch_add(static_cast<std::uint64_t>(bytes), std::memory_order_relaxed);
}

ImageCopyCount imageCopyCount() noexcept {
    return {gCopies.load(std::memory_order_relaxed), gBytes.load(std::memory_order_relaxed)};
}

}  // namespace aoi
