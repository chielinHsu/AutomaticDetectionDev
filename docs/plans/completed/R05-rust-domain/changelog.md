# R05 變更紀錄

> 只新增，新紀錄放最上方；格式見 `docs/plans/_template/changelog.md`。
> 3.0.0 之前（R05.1、R05.2）的紀錄在 [`docs/legacy/changelog-ai.md`](../../../legacy/changelog-ai.md) 的 2026-10-27 各筆，沒有搬進來；結果與數字在 ADR-031 的「後續」。

## 2026-10-28 — Claude Code (Claude Opus 5.5)：R05.5（計畫完成）

### 細項
- R05.5 ⬜ → ✅：`AOI_DOMAIN_IMPL` 的預設改成 `rust`；`test.ps1`、`perf.ps1` 的 `-DomainImpl` 預設 `Rust`，建置資料夾 Rust 沒有字尾、C++ 加 `-cpp`（之前相反）。
- 問使用者（2026-10-28）：保留 `cpp` 選項與 C++ 的實作；效能先比再決定，有影響就重建基準線；第二階段先不做；兩項真實資料的測試跳過、標未驗證；`AiInspTool/` 的原地建置搬到 `build/r05-5-insource-backup/`。
- 完整矩陣 10 組、效能閘門（`-Runs 3`）、cpp 與 rust 交替量 3 次、重建基準線。
- 計畫完成：資料夾從 `docs/plans/active/` 搬到 `docs/plans/completed/`，`INDEX.md` 改 ✅。

### 修改的檔案
- `AiInspTool/qmake/aoi_options.pri`（預設 `rust`）、`AiInspTool/qmake/aoi_rust.pri`（註解）
- `scripts/dev/test.ps1`（預設 `Rust`、`-cpp` 字尾、一律把 `AOI_DOMAIN_IMPL` 傳給 qmake）、`scripts/dev/perf.ps1`（新增 `-DomainImpl`）、`scripts/dev/env.ps1`（找不到 cargo 時的說明）
- `perf/baseline/casparhsu1-harness.json`（重建）
- `AI_CONTEXT.md`、`docs/architecture.md`、`docs/current-status.md`、`docs/decisions.md`、`docs/adr/031-rust-domain-hybrid.md`（「後續（R05.5）」）、本計畫的 `plan.md`／`status.md`、`docs/plans/INDEX.md`、`.ai/handoff.md`

### 驗證
- `test.ps1`：Release＋Rust 66／66、Release＋Cpp 65／65、Debug＋Rust 66／66、Debug＋Cpp 64／65 → `tst_presenter` 單獨重跑通過、Profile＋Rust 65／66 → `tst_presenter` 單獨重跑通過、Profile＋Cpp 65／65、Release＋Rust `-NoOpenCv` 63／63、`-NoHalcon` 64／64、兩個都關 61／61、Release＋Cpp 兩個都關 60／60；10 次建置 0 個 LNK4。HALCON 有授權；XFAIL 只有既有的兩列；跳過的都是環境因素。
- 第一次跑矩陣失敗的兩件事（都不是程式的問題）：改名的建置資料夾裡 `tst_cli` 指向舊路徑（刪掉物件檔重編）、`AiInspTool/` 的原地 `.moc` 讓新的建置資料夾編不起來（使用者同意搬走）；細節在 ADR-031「後續（R05.5）」。
- 效能：`perf.ps1 -Runs 3` 對舊基準線三次都判失敗（雜訊大，第 1 次 CPU 負載 87%）；cpp 與 rust 交替量 3 次：use case 與晶格估計相同或較快，逐次呼叫的 `INC-*`／`FULL-*`／`NMS-PAIR` 較慢（FFI 成本），`ui-s04` 的 I4／I6 兩種建置都比基準線慢（不是 Rust）。用同一份程式 6 次完整量測裡最安靜的三次（123143、124030、130058）重建 `perf/baseline/casparhsu1-harness.json`（`-WriteBaseline` 那三次的後兩次有 Visual Studio 的建置在跑，沒有採用），新的基準線對這三次通過；預算不動。
- `check-deps` 0 違規、`ste-lint` 0 error、`check-ai-structure.ps1 -Strict` OK（7 個既有的警告）。
- **未驗證**：`perf_match_region_real`、`tst_repeatability::realFiveRuns`（真實資料）；Qt Creator 的 GUI 沒有操作。

### Commit
- `refactor/r05-5-finish`，合併到 `main`（`--no-ff`），tag `r05.5-done`。

## 2026-10-27 — Claude Code (Claude Opus 5.5)：R05.4

### 細項
- R05.4 ⬜ → ✅：`repeatability.cpp` 的 `summarize` 在 `AOI_DOMAIN_IMPL=rust` 時改由 Rust 計算，公開標頭不變。C++ 原樣 `git mv` 到 `src/domain/legacy/repeatability_legacy.cpp`；`valueOf`、`defaultMetrics`（型別的存取與預設的指標表）分到兩種建置共用的 `repeatability_types.cpp`。C ABI 版本 4（`AoiRsStatistics` 逐欄 `static_assert`、值零複製）。
- 先量 before：新增效能量測 `perf_repeatability_stats`（`RS-SUM-*`；`perf_repeatability` 有原地 `.moc`，而且量的是 application 的 `analyze`），兩個執行檔備份後交替量 3 次。
- 差異測試擴充 2 項（169 → 171）：20 萬組隨機輸入，與 `AOI_REPEATABILITY_DATA` 的真實 BOX LEVEL 報表。

### 修改的檔案
- `src/domain/legacy/repeatability_legacy.cpp`（從 `repeatability.cpp` 搬過來）、`repeatability_legacy.h`（新）、`src/domain/repeatability.cpp`（改成轉呼叫）、`repeatability_rs.cpp`、`repeatability_types.cpp`（新）、`src/domain/ffi/aoi_domain_rs.h`
- `rust/aoi_domain_rs/src/`：`repeatability.rs`、`ffi_repeatability.rs`（新），`lib.rs`、`ffi.rs`
- `AiInspTool/src/domain/domain.pro`、`AiInspTool/tests/domain/domain_legacy.pro`、`perf_repeatability_stats.pro`（新）、`AiInspTool/tests/tests.pro`
- `tests/domain/tst_domain_rs_diff.cpp`、`tests/domain/perf_repeatability_stats.cpp`（新）、`tools/perf-compare.mjs`（來源 `repeatability_stats`）
- `docs/adr/031-rust-domain-hybrid.md`、`docs/architecture.md`、`AI_CONTEXT.md`、本計畫的 `plan.md`／`status.md`、`docs/plans/INDEX.md`、`.ai/handoff.md`

### 驗證
- `test.ps1` Release＋Rust 66／66、Release＋Cpp 65／65、Debug＋Rust 66／66、Debug＋Cpp 65／65，建置記錄 0 個 LNK4。跳過的都是環境因素，HALCON 有授權；XFAIL 只有既有的兩列。
- `tst_domain_rs_diff` 171 項（170 通過；真實資料那一項在 `test.ps1` 裡沒設環境變數而跳過）：隨機輸入 988 059 個浮點值一位不差（Release、Debug）。另外以 `AOI_REPEATABILITY_DATA=D:\Benano\20261020_20261020_4677_AdvancePin_LeftTopRegion_Proj` 單獨跑真實資料那一項（Release＋Rust）：14 個資料夾、309 份 CSV、61 992 組、371 952 個浮點值一位不差。`tst_repeatability`（7 通過、`realFiveRuns` 跳過）、`tst_repeatability_ui`（5 通過）四種組合都相同。
- cargo test（41 個）、clippy、fmt、`check-rust` 通過；`check-deps` 0 違規、`ste-lint` 0 error、`perf-compare --self-test` 20 項、`check-ai-structure.ps1` OK（7 個既有的警告）。
- 效能（Release，交替 3 次）：cpp 建置不變；rust 建置的 `summarize` 較快（RS-SUM-30x2812 1.69–2.48 → 0.67–1.48 ms），RS-ANALYZE 在雜訊內相同；數字在 ADR-031「後續（R05.4）」。
- **未驗證**：`tst_repeatability::realFiveRuns`（原始五份 CSV 在這台機器的 D: 找不到）；`-NoOpenCv`、`-NoHalcon`、Profile；Qt Creator 的 GUI。

### Commit
- `refactor/r05-4-repeatability`，合併到 `main`（`--no-ff`），tag `r05.4-done`。

## 2026-10-27 — Claude Code (Claude Opus 5.5)：R05.3

### 細項
- R05.3 ⬜ → ✅：`match_region.cpp`（`PixelMask::count`、`runsFromMask`、`maskFromRuns`、`matchSpecFor`、`matchSettingsFromPixels`）與 `feature_extract.cpp`（`pickBrightBlob`、`growMask`）在 `AOI_DOMAIN_IMPL=rust` 時改由 Rust 計算，公開標頭不變。C++ 原樣 `git mv` 到 `src/domain/legacy/`；`operator==` 分到兩種建置共用的 `match_region_equality.cpp`。C ABI 版本 3。
- 先量 before：新增效能量測 `perf_match_region`（`MR-*`；`perf_lattice` 有原地 `.moc`，加新函式不會執行，所以開新的執行檔），搬之前的執行檔備份後交替量 3 次。
- `cpp.rs` 加 `msvc_lround`（實測 MSVC 的 `std::lround` 超出範圍或 NaN 時是 0）。
- 差異測試擴充 5 項（164 → 169）；比對函式 `agree()` 補上兩邊都是 ±∞ 的情況。

### 修改的檔案
- `src/domain/legacy/`（新）、`src/domain/match_region.cpp`、`feature_extract.cpp`（改成轉呼叫）、`match_region_rs.cpp`、`feature_extract_rs.cpp`、`match_region_rs_bridge.h`、`match_region_equality.cpp`、`src/domain/ffi/aoi_domain_rs.h`
- `rust/aoi_domain_rs/src/`：`match_region.rs`、`feature_extract.rs`、`ffi_match.rs`（新），`lib.rs`、`ffi.rs`、`cpp.rs`
- `AiInspTool/src/domain/domain.pro`、`AiInspTool/tests/domain/domain_legacy.pro`、`perf_match_region.pro`（新）、`AiInspTool/tests/tests.pro`
- `tests/domain/tst_domain_rs_diff.cpp`、`tests/domain/perf_match_region.cpp`（新）、`tools/perf-compare.mjs`（來源 `match_region`）
- `docs/adr/031-rust-domain-hybrid.md`、`docs/architecture.md`、`AI_CONTEXT.md`、本計畫的 `plan.md`／`status.md`、`docs/plans/INDEX.md`、`.ai/handoff.md`

### 驗證
- `test.ps1` Release＋Rust 66／66、Release＋Cpp 65／65、Debug＋Cpp 65／65、Debug＋Rust 65／66：唯一的失敗是已知不穩定的 `tst_presenter::aSlowSearchShowsBusyThenProgressAndEscCancelsToIdleAtOnce`（Debug 的 2 秒忙碌等待），單獨重跑通過。建置記錄 0 個 LNK4。跳過的 6 個是環境因素（和 R05.2 相同），HALCON 有授權。
- `tst_domain_rs_diff` 169 項通過：遮罩、runs、Otsu 的門檻完全相同，浮點數 32 720 個一位不差。cargo test（36 個）、clippy、fmt、`check-rust` 通過；`check-deps` 0 違規、`ste-lint` 0 error、`perf_compare_self_test` 20 項通過、`check-ai-structure.ps1 -Strict` OK。
- 效能（Release，交替 3 次）：數字在 ADR-031「後續（R05.3）」；rust 的 `matchSpecFor` 慢約 0.03–0.13 ms、`count` 每次多約 6 ns，其他相同或較快。
- **未驗證**：`perf_match_region_real` 的召回與位置（影像 2026-10-20 換過，預設參數對不上，搬之前的執行檔也失敗；只比了真實影像上的擷取，before／after 相同）；`-NoOpenCv`、`-NoHalcon`、Profile；Qt Creator 的 GUI。

### Commit
- `refactor/r05-3-match-region`，合併到 `main`（`--no-ff`），tag `r05.3-done`。

## 2026-10-27 — Claude Code (Claude Opus 5.5)

### 細項
- 從 2.x 的需求進度表遷移成計畫（AI 結構 3.0.0，ADR-032）：舊的 `docs/plan/R05-rust-domain.md` 以 `git mv` 搬成本資料夾的 `plan.md`，加上 frontmatter 與細項表（R05.1–R05.5 照抄舊表的狀態、分支與驗證），Start prompt 改讀本資料夾。

### 修改的檔案
- `docs/plans/active/R05-rust-domain/`

### 驗證
- 未建置（文件遷移）；結構以 `scripts/ai/check-ai-structure.ps1 -Strict` 檢查。

### Commit
- 見計畫 R06。
