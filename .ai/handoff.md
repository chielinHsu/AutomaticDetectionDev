# Agent Handoff

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-27
- 分支：`refactor/rust-domain` 已以 `--no-ff` 合併到 `main`，tag `r05.1-done`（只在本機，沒有 push）。

## 本次交付：R05.1 建置整合與 NMS 試點（關卡通過，[ADR-031](../docs/adr/031-rust-domain-hybrid.md) 的「後續」）

- `rust/aoi_domain_rs`：零外部依賴的 crate（`staticlib`＋`rlib`，edition 2024，`Cargo.lock` 入版控）；`nms.rs`、`flat_map.rs`、`geometry.rs`、`ffi.rs`（C ABI、`catch_unwind`、`try_reserve`）。
- C 標頭 `src/domain/ffi/aoi_domain_rs.h`；`non_max_suppression.h` 只改私有部分（`OverlapIndex` 改 pimpl、不可複製可移動）；C++ 實作搬到 `src/domain/lattice/legacy/`（`aoi::legacy`）；`non_max_suppression.cpp`（cpp）與 `non_max_suppression_rs.cpp`（rust）是包裝。
- qmake：`AOI_DOMAIN_IMPL=cpp|rust`（預設 cpp）、`aoi_rust.pri`（找 cargo、系統程式庫）、`AiInspTool/src/domain_rs/domain_rs.pro`（呼叫 cargo）、`aoi_deps.pri` 的 `domain_rs`、`domain_legacy`、`aoi_target.pri` 連結 Rust 的 lib。
- 測試：`tests/domain/tst_domain_rs_diff`（只在 rust 建置）；`perf_lattice` 多 `NMS-IDX-10000`、`NMS-PAIR-20000` 與結果檔的 `domainImpl`；`test.ps1 -DomainImpl Cpp|Rust` 與 4 項 Rust 檢查（`tools/check-rust.mjs`、cargo test／clippy／fmt）；`env.ps1` 把 cargo 加進 PATH。

## 實際驗證

- `test.ps1`：Release＋Rust 66／66、Release＋Cpp 65／65、Debug＋Rust 66／66、Debug＋Cpp 65／65、Profile＋Rust 66／66；建置記錄 0 個 `LNK4` 警告。跳過的項目都是環境（重複性資料、截圖、EXR 等），HALCON 有授權、沒有跳過。XFAIL 2 列照舊。
- 差異測試：22 萬對旋轉矩形的面積與比例一位不差；NMS 索引、`OverlapIndex` 完全相同；panic 被擋在 Rust。
- Qt Creator 條件的模擬建置（Profile 參數、PATH 沒有 cargo）成功；**Qt Creator GUI 沒有實際操作**。
- `-NoOpenCv`、`-NoHalcon` 的組合沒跑（R05.5 的完整矩陣）。
- `check-deps` 0 違規、`ste-lint` 0 error。效能數字在 ADR-031「後續」（資訊性，沒有預算）。

## 下一步

- R05.2 晶格（從 `main` 開 `refactor/r05-2-lattice`），prompt 依計畫檔「Start prompt（R05.2 起的共通格式）」。照 R05.1 的模式：C++ 搬到 `lattice/legacy/`（`aoi::legacy`）、`*_rs.cpp` 包裝、差異測試擴充；自訂 `IVectorClustering` 用 C 函式指標＋context。

## 注意

- 不要在 `AiInspTool/` 原地執行 qmake：原地建置的舊 `.moc` 會被相依掃描抓到、不重新 moc（R05.1 把它們搬到 `build/r05-insource-generated-backup/`）。
- `aoi::legacy` 裡呼叫同名函式要寫 `legacy::`（ADL 會找到 `aoi::` 的同名函式）。
- 換 Rust 版本後 `rust_manifest` 失敗時，照它印的清單更新 `aoi_rust.pri` 的 `AOI_RUST_SYSLIBS`。
- 效能量測雜訊大（剛建置完會慢 2 倍）：建置後等一下再量，前後交替。

## 不要動與界線

- 不改 Qt Widgets、OpenCV、HALCON 的 adapter 與 application 層（不在 R05 範圍）。
- 預算與基準線等使用者指示（R05.5 會問）；C++ 重複實作的去留由使用者決定（R05.5）。
- 先前的提醒：Debug 的 Presenter 2 秒忙碌等待是既有的不穩定測試；HALCON 讀不到授權時等 5 分鐘、看 CodeMeter 的 log。
