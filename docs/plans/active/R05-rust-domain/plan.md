---
id: R05
title: Rust 混合架構第一階段：domain 的演算法搬進 Rust
type: 重構
status: 進行中
created: 2026-10-27
updated: 2026-10-27
depends_on: []
related: [R01, R03]
branch: refactor/r05-<N>-<slug>（每一步一個）
---

# R05 — Rust 混合架構第一階段：domain 的演算法搬進 Rust

| 項目 | 內容 |
|---|---|
| 層級 | 底層（domain）＋建置 |
| 分支 | R05.1：`refactor/rust-domain`（已含本計畫與 ADR-031）；R05.2 起每一步從 `main` 開 `refactor/r05-N-slug` |
| 依賴 | Rust 工具鏈（使用者 2026-10-27 安裝：`rustc 1.99.1`、`stable-x86_64-pc-windows-msvc`）；設計決定 [ADR-031](../../../adr/031-rust-domain-hybrid.md) |
| 計畫編號 | R05，細項 R05.1–R05.5（細項表見下方「細項」） |
| 產出位置 | `rust/aoi_domain_rs/`、`src/domain/ffi/`、`src/domain/`（.cpp 改成包裝）、`AiInspTool/src/domain_rs/`、`AiInspTool/qmake/`、`tests/domain/tst_domain_rs_diff.cpp`、`scripts/dev/` |

> **修訂紀錄**：
> 2026-10-27：AI 結構 3.0.0（ADR-032）：本檔從 `docs/plan/R05-rust-domain.md` 搬成計畫資料夾的 `plan.md`，加上 frontmatter 與細項表（從舊的需求進度表照抄）；之前的紀錄在 `docs/legacy/`。
> 2026-10-27：R05.3 完成（比對區與自動擷取搬進 Rust；差異測試全部一位不差；新增效能量測 `perf_match_region`；結果見 ADR-031 的「後續（R05.3）」）。
> 2026-10-27：R05.2 完成（晶格整個搬進 Rust；差異測試全部一位不差；累加器改成零複製；結果見 ADR-031 的「後續（R05.2 晶格）」）。
> 2026-10-27：R05.1 完成（關卡通過，做法沒有推翻；實測結果見 ADR-031 的「後續」）。
> 2026-10-27：建立。範圍由使用者決定（混合架構、domain 先搬、C++ 介面不變、用既有測試做差異驗證）；技術做法見 ADR-031，R05.1 實測後確認。

<!--STE-->
## Purpose
This plan moves the domain algorithms from C++ to Rust. The algorithms are the lattice, NMS, match region, feature extraction and repeatability statistics. The C++ headers stay the same. Other layers do not change.

## Reason
The user wants to use Rust in this project. Rust finds memory errors and data races during compilation. The domain layer uses only the standard library and has many tests. Thus it is the safest first part to move.

## Effect
After this plan, a Rust crate does the domain calculations. The same tests pass with the C++ code and with the Rust code. A difference test compares the two results for each input.
<!--/STE-->

**繁中**：把 domain 的演算法（晶格、NMS、比對區、特徵擷取、重複性統計）從 C++ 搬到 Rust，C++ 標頭不變、其他層不動。domain 只用標準庫又有大量測試，是最安全的第一塊。完成後由 Rust crate 計算，同一組測試在兩種實作都通過，另有差異測試逐筆比對。

## 細項

細項表是本計畫進度的單一事實來源（2.x 時在 `docs/current-status.md` 的需求進度表，3.0.0 搬到這裡）。

| 編號 | 細項 | 狀態 | 分支／commit | 驗證 |
|---|---|---|---|---|
| R05.1 | 建置整合與 NMS 試點（關卡）：env.ps1 的 cargo、`rust/aoi_domain_rs`、qmake 呼叫 cargo、`AOI_DOMAIN_IMPL`、C ABI 標頭、NMS 改 Rust、差異測試骨架、cargo 檢查進 `test.ps1`、Debug CRT 實測 | ✅（關卡通過；Qt Creator GUI 未實際操作） | `refactor/rust-domain`，tag `r05.1-done` | PARTIALLY VERIFIED：`test.ps1` Release＋Rust 66／66（含 `tst_domain_rs_diff`：22 萬對面積一位不差、NMS 索引完全相同；cargo 4 項）、Release＋Cpp 65／65、Debug＋Rust 66／66、Debug＋Cpp 65／65、Profile＋Rust 66／66，0 個 LNK 警告；Qt Creator 條件的模擬建置（Profile、PATH 沒有 cargo）成功，GUI 未操作；`-NoOpenCv`／`-NoHalcon` 未跑；效能 before／after 見 ADR-031「後續」 |
| R05.2 | 晶格（累加器、空間格子、分群含自訂策略回呼、基底、格子分量、接受規則、估計、預測）；T3 before／after | ✅ | `refactor/r05-2-lattice`，tag `r05.2-done` | VERIFIED：`test.ps1` Release＋Rust 66／66、Release＋Cpp 65／65、Debug＋Rust 66／66、Debug＋Cpp 65／65（完整執行時兩種的 Debug 都只有已知不穩定的 `tst_presenter::aSlowSearchShowsBusyThenProgressAndEscCancelsToIdleAtOnce`（Debug 的 2 秒忙碌等待，進度表「待使用者決定」第 5 項）失敗，單獨重跑通過；Cpp 單獨重跑 4 次 2 次通過，耗時 164–270 s 隨另一個 session 的建置負載變動。Debug＋Cpp 第一次建置遇到一次 `LNK1236`（`lattice_accumulator_legacy.obj` 寫壞），刪掉那個 .obj 重建後正常），0 個 LNK 警告；`tst_domain_rs_diff` 164 項（1 147 組點的估計、7 080 次預測等，約 177 萬個浮點值全部一位不差）；對時間敏感的 HALCON／取消延遲測試在機器滿載時失敗、單獨重跑通過；效能：rust 的 T3 0.46–0.66 ms（before 0.57–1.47），逐點呼叫累加器每次多約 11 ns（ADR-031「後續（R05.2）」）；`-NoOpenCv`／`-NoHalcon` 未跑 |
| R05.3 | 比對區與特徵擷取（`match_region`、`feature_extract`） | ✅ | `refactor/r05-3-match-region`，tag `r05.3-done` | VERIFIED（真實影像的召回與位置除外）：`test.ps1` Release＋Rust 66／66、Release＋Cpp 65／65、Debug＋Cpp 65／65、Debug＋Rust 65／66（只有已知不穩定的 `tst_presenter::aSlowSearchShowsBusyThenProgressAndEscCancelsToIdleAtOnce` 失敗，單獨重跑通過），0 個 LNK4 警告；`tst_domain_rs_diff` 169 項（約 3.3 萬個浮點值一位不差，遮罩、runs、門檻完全相同）；cargo 4 項（36 個單元測試）、`check-deps`、`ste-lint` 通過；效能：新的 `perf_match_region` 交替 3 次（ADR-031「後續（R05.3）」）；`perf_match_region_real`：影像 2026-10-20 換過、預設參數對不上，搬之前就失敗，只比了真實影像上的擷取（before／after 相同）；`-NoOpenCv`／`-NoHalcon`／Profile 未跑 |
| R05.4 | 重複性統計（`repeatability`） | ⬜ |  | 尚未開始 |
| R05.5 | 收尾：預設改 `rust`、完整測試矩陣、效能閘門與基準線、C++ 重複實作的去留（問使用者）、文件 | ⬜ |  | 尚未開始 |

圖例：✅ 完成　⏳ 進行中　⬜ 尚未開始　⚠️ 有未解決問題　⛔ 取消

## 任務

每一步一個 session（Step Protocol），對應上表的一個細項。

1. **R05.1 建置整合與 NMS 試點（關卡）**
   - `scripts/dev/env.ps1` 把 `%USERPROFILE%\.cargo\bin` 加進 PATH（已在就不重複）。
   - 建立 `rust/aoi_domain_rs/`（`staticlib`＋`rlib`、`[dependencies]` 空、`Cargo.lock` 入版控、`edition` 用目前 stable 支援的最新版）。
   - qmake：子專案 `AiInspTool/src/domain_rs/` 在 domain 之前呼叫 cargo（Debug 用 dev profile、Release／Profile 用 `--release`，`--target-dir` 放在建置資料夾裡）；選項 `AOI_DOMAIN_IMPL=cpp|rust`（預設 `cpp`）；`aoi_deps.pri` 的 `AOI_EXT_domain` 帶 Rust 的程式庫與 `--print native-static-libs` 的系統程式庫；找不到 cargo 時 qmake 報錯。
   - C 標頭 `src/domain/ffi/aoi_domain_rs.h`；`overlapArea`、`overlapRatio`、`OverlapIndex`（handle）、`NonMaxSuppression::keep` 改由 Rust 計算（`AOI_DOMAIN_IMPL=rust` 時）。
   - 差異測試的骨架：只給測試用的 `aoi::legacy` 程式庫（舊的 C++ 實作）＋ `tests/domain/tst_domain_rs_diff`（固定 seed 的隨機旋轉矩形，`keep` 的索引完全相同、重疊面積的容差寫明理由）。
   - `test.ps1`：`-DomainImpl Cpp|Rust`；加 `cargo test`、`cargo clippy --all-targets -- -D warnings`、`cargo fmt --check` 與「`[dependencies]` 必須空」四項檢查（測試項數跟著改，`AI_CONTEXT.md` 的數字一起改）。
   - **要實測的風險**：Debug（`/MDd`）配 Rust 的 release CRT 會不會衝突（LNK4098 等）；三種組態都建置並跑測試；Qt Creator 開 `.pro` 能不能建置（要重新開 Qt Creator 讀新的 PATH）；連結器警告不可以關掉了事。
   - 效能：搬之前量 NMS 的 before（`perf_lattice` 或 `perf_core` 裡相關的指標；沒有 NMS 的指標就先補一個微基準），搬之後 after，前後交替量。
   - 文件：依賴表三邊一起改（`docs/architecture.md`、`aoi_deps.pri`、`check-deps` 的 `RULES`，如有需要）；`AI_CONTEXT.md` 的環境表加 Rust 一列、建置與測試的指令。
   - 關卡：CRT 或建置整合解不掉時停下來回報，不做 R05.2；ADR-031 的做法被推翻時在 ADR-031 加「後續」。
2. **R05.2 晶格**：`lattice_accumulator`、`spatial_grid`、`vector_clustering`、`basis_finder`、`cell_components`、`lattice_acceptance`、`lattice_estimator`、`lattice_predictor`。內建分群策略用列舉，自訂的 `IVectorClustering` 用 C 函式指標＋context 回呼（`tst_lattice` 的 `CountingClustering` 要照樣能用）。驗證：`tst_lattice*`（含 1 029 種情境）、`tst_lattice_crosscheck`、`tst_lattice_explainer`、`tst_nms`、契約測試、差異測試擴充到 `LatticeEstimator::estimate` 與 `LatticePredictor`；`perf_lattice`（T3）before／after。
3. **R05.3 比對區與特徵擷取**：`match_region.cpp`、`feature_extract.cpp`（Otsu、最近亮塊、外擴）。驗證：`tst_match_region`、`tst_session`、契約測試；有 `AOI_REAL_MATCH_IMAGE` 時跑 `perf_match_region_real` 確認召回與位置不變。
4. **R05.4 重複性統計**：`repeatability.cpp`。驗證：`tst_repeatability`（有 `AOI_REPEATABILITY_DATA` 時含真實五份 CSV）、`tst_repeatability_ui`、`perf_repeatability`。
5. **R05.5 收尾**：`AOI_DOMAIN_IMPL` 預設改 `rust`；完整矩陣（Release、Debug、Profile；`-NoOpenCv`、`-NoHalcon`；兩種 `-DomainImpl`）；`perf.ps1 -Runs 3` 與基準線比，影響效能時重建基準線（`-Runs 3 -WriteBaseline`）；C++ 的重複實作與 `cpp` 選項要不要刪，**問使用者**；更新 `docs/architecture.md`、`AI_CONTEXT.md`、ADR-031 的「後續」；第二階段（application）要不要做，列進「待使用者決定」。

## SOLID 重點

- **D**：application 與其他層仍只看 domain 的 C++ 標頭，不知道背後是 C++ 還是 Rust；Rust 只出現在 domain 的 .cpp 包裝與建置設定。
- **L**：兩種實作要通過同一組測試，差異測試逐筆比對；這就是兩種實作可以互換的證據。

## 效能檢查（寫入各步驟的紀錄）

先量再改（S11 規則）。每一步：搬之前用 `scripts/dev/perf.ps1 -Filter <相關的執行檔> -Runs 3` 量 before，搬之後量 after，前後交替；比基準線範圍上緣慢超過 15%（ADR-023 的閘門）要回報原因，不可以改預算。R05.1 另外記錄建置時間（乾淨建置與只改一個 .rs 的增量建置）。

## 四種輸出

R05 的類型是「重構」，依 `project-rules.md` 免四種輸出；設計記在 ADR-031，結果記在本計畫的細項表、`status.md`、`changelog.md` 與交接單。

## 完成定義（每一步）

- [ ] `test.ps1`（Release＋OpenCV＋HALCON）在 `-DomainImpl Cpp` 與 `-DomainImpl Rust` 都全部通過；`-Config Debug` 也跑（R05.1 另跑 Profile）
- [ ] `tst_domain_rs_diff` 通過；`cargo test`、`clippy`、`fmt --check`、`[dependencies]` 檢查通過
- [ ] 效能 before／after 已量並記錄
- [ ] `node tools/check-deps.mjs`、`node tools/ste-lint.mjs` 無 error
- [ ] 本計畫的細項表、`status.md`、`changelog.md` 與交接單已更新（`ai-handoff` skill）；R05.5 完成時資料夾搬到 `docs/plans/completed/`、`docs/plans/INDEX.md` 改 ✅
- [ ] HALCON 讀不到授權而跳過的測試列成「未驗證」，不算通過

## Commit 範本

格式依 `project-rules.md`「Commit 訊息」（提交身分依上層 `D:\AOI_Project\Qt\CLAUDE.md`）；這裡只列內容參考。

```
[Caspar]refactor: NMS 改由 Rust crate 計算，qmake 接上 cargo

正文：為什麼（ADR-031）、怎麼接進 qmake、CRT 的實測結果、差異測試的
容差與理由、before／after 的數字。

測試 :
兩種 -DomainImpl 的 test.ps1 結果、tst_domain_rs_diff、cargo 的檢查。

Co-Authored-By: Claude <模型名稱>

[Test]
Build OK, Test OK
```

## Start prompt（R05.1，已完成，保留參考；當時的路徑是 2.x 的）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續專案。回覆一律使用繁體中文（zh-TW），完成時總結修改內容。
請先讀 AGENTS.md、docs/current-status.md（需求進度表的 R05）、docs/plan/R05-rust-domain.md、
docs/adr/031-rust-domain-hybrid.md，再依 .agents/harness/project-rules.md 的 Step Protocol 只做 R05.1
（建置整合與 NMS 試點），在分支 refactor/rust-domain 上做（已含計畫與 ADR）。
Rust 已安裝：%USERPROFILE%\.cargo\bin（rustc 1.99.1，stable-x86_64-pc-windows-msvc，含 clippy、rustfmt）；
目前的 shell 可能還沒有這個 PATH，先用完整路徑或由 env.ps1 加入。
R05.1 是關卡：Debug 的 CRT、qmake 整合、Qt Creator 建置有解不掉的問題就停下來回報，不要做 R05.2。
完成後：commit（格式依 project-rules.md，身分依上層 CLAUDE.md）、在本機 merge --no-ff 到 main、打 tag r05.1-done（不 push）、
更新進度表與交接單（ai-handoff skill）、對我提示 commit 內容，並用 start_session 為 R05.2 開新 session
（prompt 依本計畫 R05.2 的任務改寫，提醒繁體中文）。
```

## Start prompt（R05.2 起的共通格式）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續專案。回覆一律使用繁體中文（zh-TW），完成時總結修改內容。
請先讀 AGENTS.md、docs/plans/INDEX.md、計畫 docs/plans/active/R05-rust-domain/ 的 plan.md 與 status.md、
docs/adr/031-rust-domain-hybrid.md（含「後續」）與 .ai/handoff.md，再依 Step Protocol 只做 R05.<N>（從 main 開分支 refactor/r05-<N>-<slug>）。
先量 before 再搬；兩種 -DomainImpl 都要全部通過，差異測試擴充到這一步搬的函式。
完成後 commit、merge --no-ff 到 main、打 tag r05.<N>-done（不 push）、更新本計畫的細項表、status.md、changelog.md 與交接單、提示 commit 內容，
並為下一步開新 session。
```
