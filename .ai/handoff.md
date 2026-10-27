# Agent Handoff

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-27
- 分支：`refactor/r05-2-lattice` 已以 `--no-ff` 合併到 `main`，tag `r05.2-done`（只在本機，沒有 push）。

## 本次交付：R05.2 晶格搬進 Rust（[ADR-031](../docs/adr/031-rust-domain-hybrid.md) 的「後續（R05.2 晶格）」）

- `rust/aoi_domain_rs/src/lattice/`（逐檔對應 C++ 的晶格）、`ffi_lattice.rs`、`cpp.rs`（C++／MSVC 的數值行為）、`fallible.rs`（`try_reserve`）；C ABI 版本 2（`src/domain/ffi/aoi_domain_rs.h`）。
- C++ 的 8 個 .cpp 用 `git mv` 搬到 `src/domain/lattice/legacy/`（`aoi::legacy`）；cpp 建置的公開 .cpp 轉呼叫 legacy，rust 建置的 `*_rs.cpp` 經 `lattice/lattice_rs_bridge.{h,cpp}` 呼叫 Rust。估計一次 FFI 呼叫；自訂的 `IVectorClustering` 用回呼，C++ 例外記下後重新丟出。
- 公開標頭只改私有部分：`SpatialGrid` 改 pimpl；`LatticeAccumulator` 的欄位合成 `detail::LatticeSums`（零複製，兩種實作直接操作）；兩個內建分群策略多 friend。
- 差異測試 `tst_domain_rs_diff` 擴充到 164 項：晶格的每一部分、1 029 種情境＋其他情境、回呼、例外、預測器。

## 實際驗證

- `test.ps1`：Release＋Rust 66／66、Release＋Cpp 65／65、Debug＋Rust 66／66、Debug＋Cpp 65／65；0 個 LNK4。對時間敏感的測試（`tst_cli` 多行程 HALCON、`tst_presenter`、`tst_contract_searcher` 取消延遲）在機器滿載時失敗過，單獨重跑通過；Debug 的 `tst_presenter` 2 秒忙碌等待仍不穩定（重跑 4 次 2 次通過）。
- 差異測試：約 177 萬個浮點值全部一位不差（Release 與 Debug）；整數、索引、原因完全相同。
- `check-deps` 0 違規、`ste-lint` 0 error、cargo 的 4 項通過。
- 效能（資訊性）：rust 的 T3 0.46–0.66 ms（before 0.57–1.47）；逐點呼叫累加器每次多約 11 ns（FULL-10000 約 480 → 1 050 ms）；cpp 建置與搬之前相同。數字在 ADR-031。
- 沒跑：`-NoOpenCv`、`-NoHalcon`、Profile（R05.5 的完整矩陣）；Qt Creator GUI 沒有操作。

## 下一步

- R05.3 比對區與特徵擷取（`match_region.cpp`、`feature_extract.cpp`），從 `main` 開 `refactor/r05-3-match-region`。
- **先確認 AI 結構**：另一個 session 正把本專案遷移到 AI 結構 3.0.0（分支 `docs/ai-structure-v3`，計畫會搬到 `docs/plans/active/R05-rust-domain/`）。R05.3 的 Start prompt 依遷移之後的位置。

## 注意

- `AiInspTool/` 底下有 2026-10-27 13:00 的一次完整原地建置（`Makefile`、`.obj`、`.moc`、`bin/`），不是 R05.2 產生的，可能是使用者的 Qt Creator；沒有動它。改到有原地 `.moc` 的測試檔（`tst_match_region`、`tst_lattice*` 等）時，新的測試函式可能不執行，先看 log 的項數。
- `aoi::legacy` 裡呼叫同名函式要寫 `legacy::`（ADL）。Rust 的整數運算照 MSVC 用 `wrapping_*`。
- Debug＋Cpp 的建置遇過一次 `LNK1236`（`.obj` 寫壞），刪掉那個 `.obj` 重建即可。
- 效能量測雜訊大：另一個 session 在建置時不要量；before／after 交替。

## 不要動與界線

- 不改 application、presentation、OpenCV、HALCON（R05 範圍外）。預算與基準線、C++ 重複實作的去留留給 R05.5 問使用者。
