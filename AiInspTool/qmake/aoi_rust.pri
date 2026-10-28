# Rust：domain 演算法的 Rust 實作（rust/aoi_domain_rs；AOI_DOMAIN_IMPL=rust（R05.5 起是預設）時由 aoi_options.pri include；ADR-031）。
#
# 建置：子專案 src/domain_rs/domain_rs.pro 在 domain 之前呼叫 cargo，產出 staticlib（aoi_domain_rs.lib）；
# 連到 domain 的每個程式（AOI_EXT_domain = rust，aoi_target.pri）都連結它與下面的系統程式庫。
# 找 cargo 的順序：qmake 變數 AOI_CARGO、同名的環境變數、%CARGO_HOME%/bin、%USERPROFILE%/.cargo/bin（rustup 的預設）。
# 用完整路徑：Qt Creator 在安裝 Rust 之前就開著時，它的 PATH 沒有 cargo，照樣建置得起來。
# 結果：AOI_CARGO、AOI_RUST_MANIFEST、AOI_RUST_TARGET_DIR、AOI_RUST_PROFILE_ARGS、AOI_RUST_LIB、AOI_RUST_SYSLIBS。

isEmpty(AOI_CARGO): AOI_CARGO = $$(AOI_CARGO)
aoi_cargo_home = $$(CARGO_HOME)
isEmpty(AOI_CARGO):!isEmpty(aoi_cargo_home):exists($$aoi_cargo_home/bin/cargo.exe): AOI_CARGO = $$aoi_cargo_home/bin/cargo.exe
isEmpty(AOI_CARGO): AOI_CARGO = $$(USERPROFILE)/.cargo/bin/cargo.exe
AOI_CARGO = $$clean_path($$AOI_CARGO)
!exists($$AOI_CARGO) {
    error("AOI_DOMAIN_IMPL=rust, but cargo was not found at '$$AOI_CARGO'. \
Install Rust with rustup (toolchain stable-x86_64-pc-windows-msvc), or point to cargo: qmake AiInspTool.pro AOI_CARGO=C:/path/to/cargo.exe \
(or the environment variable AOI_CARGO), or build the C++ implementation: qmake AiInspTool.pro AOI_DOMAIN_IMPL=cpp.")
}

AOI_RUST_MANIFEST = $$AOI_ROOT/rust/aoi_domain_rs/Cargo.toml
# 放在建置資料夾裡：Debug、Release、Profile 與每一種選項的組合互不干擾，刪掉建置資料夾就乾淨。
AOI_RUST_TARGET_DIR = $$AOI_BUILD_ROOT/cargo
# --locked：Cargo.lock 會被改的時候失敗（入版控的那一份就是建置用的）；--offline：不連網路（沒有外部依賴，ADR-031）。
# Debug 用 dev profile（沒有最佳化、可以除錯）；Release、Profile 用 --release。
CONFIG(debug, debug|release) {
    AOI_RUST_PROFILE_DIR = debug
    AOI_RUST_PROFILE_ARGS = --locked --offline
} else {
    AOI_RUST_PROFILE_DIR = release
    AOI_RUST_PROFILE_ARGS = --locked --offline --release
}
AOI_RUST_LIB = $$AOI_RUST_TARGET_DIR/$$AOI_RUST_PROFILE_DIR/aoi_domain_rs.lib

# `cargo rustc --lib --crate-type staticlib -- --print native-static-libs` 列出的系統程式庫（rustc 1.99.1 實測），
# 不含最後的 /defaultlib:msvcrt（Debug 的處理見 aoi_target.pri）。換 Rust 版本時可能會變：
# tools/check-rust.mjs（test.ps1 的 rust_manifest）比對這一行和 rustc 實際印出的清單，不同就失敗。
AOI_RUST_SYSLIBS = kernel32.lib ntdll.lib userenv.lib ws2_32.lib dbghelp.lib
