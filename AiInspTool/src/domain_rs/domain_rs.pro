# domain 演算法的 Rust 實作（ADR-031）：只在 AOI_DOMAIN_IMPL=rust 時由 src/src.pro 納入，排在 domain 之前。
# 不編 C++：建置時呼叫 cargo，產出 <建置>/cargo/<debug|release>/aoi_domain_rs.lib（aoi_rust.pri 的 AOI_RUST_LIB）；
# 連到 domain 的程式在 aoi_target.pri 連結它。每次建置都呼叫 cargo（沒改的時候約 0.1 秒），要不要重新編由 cargo 自己判斷。
# Rust 的原始碼列在 DISTFILES：Qt Creator 的專案樹看得到（不是 qmake 的編譯輸入）。

TEMPLATE = aux
include($$AOI_QMAKE/aoi_options.pri)
!equals(AOI_DOMAIN_IMPL, rust): error("domain_rs.pro is only used with AOI_DOMAIN_IMPL=rust.")

aoi_cargo_build.target = aoi_cargo_build
aoi_cargo_build.commands = $$shell_quote($$system_path($$AOI_CARGO)) build --lib $$AOI_RUST_PROFILE_ARGS \
    --manifest-path $$shell_quote($$system_path($$AOI_RUST_MANIFEST)) \
    --target-dir $$shell_quote($$system_path($$AOI_RUST_TARGET_DIR))
first.depends = aoi_cargo_build
QMAKE_EXTRA_TARGETS += aoi_cargo_build first

DISTFILES += \
    $$AOI_ROOT/rust/aoi_domain_rs/Cargo.toml \
    $$AOI_ROOT/rust/aoi_domain_rs/rustfmt.toml \
    $$AOI_ROOT/rust/aoi_domain_rs/src/lib.rs \
    $$AOI_ROOT/rust/aoi_domain_rs/src/ffi.rs \
    $$AOI_ROOT/rust/aoi_domain_rs/src/flat_map.rs \
    $$AOI_ROOT/rust/aoi_domain_rs/src/geometry.rs \
    $$AOI_ROOT/rust/aoi_domain_rs/src/nms.rs
