// AOI_DOMAIN_IMPL=rust：轉呼叫 Rust 的實作（rust/aoi_domain_rs/src/repeatability.rs，C ABI 在 domain/ffi/aoi_domain_rs.h；ADR-031）。
// 契約和 C++ 的實作相同（repeatability.h）；Rust 不配置記憶體，回報錯誤只會是 bug（panic、null 指標），丟 std::logic_error。
// valueOf、defaultMetrics 在 repeatability_types.cpp（兩種建置共用）。
#include "domain/repeatability.h"

#include <cstddef>
#include <limits>

#include "domain/ffi/aoi_domain_rs.h"
#include "domain/lattice/lattice_rs_bridge.h"

namespace aoi::repeatability {

namespace {

// 值直接交出 std::span 的位址（零複製）：double 要是 IEEE 754 的 64 bit（和 Rust 的 f64 相同）。
static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == 8, "double must be IEEE 754 binary64 (Rust f64)");
// AoiRsStatistics 和 Rust 的 #[repr(C)] FfiStatistics 逐欄相同（Rust 那一側用 offset_of! 檢查同樣的數字，rust/aoi_domain_rs/src/ffi_repeatability.rs）。
static_assert(sizeof(AoiRsStatistics) == 88, "AoiRsStatistics must match #[repr(C)]");
static_assert(offsetof(AoiRsStatistics, count) == 0 && offsetof(AoiRsStatistics, expected) == 8);
static_assert(offsetof(AoiRsStatistics, has_minimum) == 16 && offsetof(AoiRsStatistics, has_maximum) == 20);
static_assert(offsetof(AoiRsStatistics, has_average) == 24 && offsetof(AoiRsStatistics, has_range) == 28);
static_assert(offsetof(AoiRsStatistics, has_deviation) == 32 && offsetof(AoiRsStatistics, has_pt) == 36);
static_assert(offsetof(AoiRsStatistics, minimum) == 40 && offsetof(AoiRsStatistics, maximum) == 48);
static_assert(offsetof(AoiRsStatistics, average) == 56 && offsetof(AoiRsStatistics, range) == 64);
static_assert(offsetof(AoiRsStatistics, deviation) == 72 && offsetof(AoiRsStatistics, pt) == 80);
static_assert(sizeof(std::size_t) == sizeof(size_t));

std::optional<double> optionalOf(int32_t has, double value) { return has != 0 ? std::optional<double>(value) : std::nullopt; }

}  // namespace

Statistics summarize(std::span<const double> values, std::size_t expected, std::optional<double> tolerance) {
    rs::checkAbi();
    AoiRsStatistics s{};
    rs::check(aoi_rs_repeatability_summarize(values.data(), values.size(), expected, tolerance ? 1 : 0, tolerance.value_or(0.0), &s),
              "aoi_rs_repeatability_summarize");
    Statistics out;
    out.count = s.count;
    out.expected = s.expected;
    out.minimum = optionalOf(s.has_minimum, s.minimum);
    out.maximum = optionalOf(s.has_maximum, s.maximum);
    out.average = optionalOf(s.has_average, s.average);
    out.range = optionalOf(s.has_range, s.range);
    out.deviation = optionalOf(s.has_deviation, s.deviation);
    out.pt = optionalOf(s.has_pt, s.pt);
    return out;
}

}  // namespace aoi::repeatability
