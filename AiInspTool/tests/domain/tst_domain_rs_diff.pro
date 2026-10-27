# ADR-031 的差異測試：Rust 實作（經 domain 的公開標頭）對 C++ 實作（aoi::legacy），固定 seed 的隨機輸入逐筆比對。
# 只在 AOI_DOMAIN_IMPL=rust 時建置（tests/tests.pro）。

TARGET = tst_domain_rs_diff
AOI_LINK_EXTRA = domain_legacy
include(domain_tests.pri)
SOURCES += $$AOI_ROOT/tests/domain/tst_domain_rs_diff.cpp
