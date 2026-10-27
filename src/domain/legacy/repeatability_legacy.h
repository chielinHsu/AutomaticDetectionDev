#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include "domain/repeatability.h"

// domain 層：只用 C++ 標準庫。
//
// 重複性的樣本統計的 C++ 實作（ADR-029 起的那一份，R05.4 從 repeatability.cpp 原樣搬過來，ADR-031）。用在兩個地方：
//   - AOI_DOMAIN_IMPL=cpp 的建置：aoi_domain 編進它，repeatability.cpp 轉呼叫這裡；
//   - AOI_DOMAIN_IMPL=rust 的建置：只有差異測試用的 aoi_domain_legacy 程式庫編進它（tests/domain/tst_domain_rs_diff 拿它當對照組）。
// 說明在 repeatability.h；這裡不要改行為（要改，Rust 那一份 rust/aoi_domain_rs/src/repeatability.rs 與差異測試一起改）。
// valueOf、defaultMetrics 是型別的存取與預設的指標表，兩種建置共用 repeatability_types.cpp，不在這裡。
namespace aoi::legacy {

repeatability::Statistics summarize(std::span<const double> values, std::size_t expected, std::optional<double> tolerance);

}  // namespace aoi::legacy
