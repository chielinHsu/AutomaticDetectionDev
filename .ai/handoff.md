# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Fable 5.1)
- 日期：2026-10-13

## 正在做

無。**S10 完成**（分支 `step/10-perf-harness`，`--no-ff` 合併到 `main`，tag `step-10-done`，沒有 push）。

**接下來的順序（使用者 2026-10-12 決定）**：（1）~~B1–B3 的回歸測試~~ →（2）~~S10~~ →（3）**HALCON 準確度的修正**（這個 session 收工時用 `spawn_task` 開；分支 `fix/halcon-accuracy`，`--no-ff` 合併、不打 tag；Start prompt 在 `docs/plan/S11-optimize.md` 開頭；B1–B3 加上 O1）→（4）S11（修正的 session 收工時開；開工前確認 `tst_contract_end_to_end` 的 log 沒有 XFAIL）。

## 這次做了什麼（細節在 `docs/changelog-ai.md` 最上面那一筆、`docs/steps/S10-perf-harness/`、ADR-023）

- 一鍵效能測試 `scripts/dev/perf.ps1`（環境快照 → `test.ps1 -Perf` → 彙整 → 和基準線與預算比 → 報表）；閘門 `tools/perf-compare.mjs`（`collect`、`compare`、`baseline`、`noise`、`html`、`--self-test`；結束碼 0／1／2／3）。
- 新的量測 `tests/perf/perf_matrix`（搜尋矩陣）、`perf_e2e`（T5）；基準線 `perf/baseline/casparhsu1-harness.json`（整套跑 3 次）。
- `perf/budgets.json`：T5 `calibrated`、頂層 `status: calibrated`、`knownIssues`（B1–B3）；limit 沒有改。
- `test.ps1` 多 1 個 node 檢查（`perf_compare_self_test`）：預設 **57 項**（`-NoHalcon` 55、`-NoOpenCv` 54、兩個都關 52）。
- 沒有改 `src/`、S02–S09 的量測程式、容差、back end 的預設。

## 建置與測試

- `test.ps1` 預設 57／57（HALCON 0 跳過；XFAIL 照舊 5 列）；`-Config Debug -Filter perf_e2e` 建置通過。
- 效能：`perf.ps1`（約 15 分鐘一次）。before／after 用 `-Filter` 縮小、`-Runs 3` 看範圍、前後交替量；改了會影響效能的程式之後 `-Runs 3 -WriteBaseline` 重建基準線。
- **雜訊**：這台機器跨行程差到中位數 44%（處理器只跑在額定速度的 54–67%：Visual Studio、Chrome、遠端桌面在跑）。報表會標；INCONCLUSIVE（結束碼 3）＝ 整台變慢，再量一次。

## 不要動

- `ai-project-template/`；`docs/ux/tokens.json`；`copy.json` 只新增 key；`perf/budgets.json` 的 limit；`knownOver("U3")`、`knownOver("I1")` 等 S11。
- `third_party/halcon/`；`tests/infra/fixtures/*.json`；`AOI_SYNTHETIC_REFERENCE_HASH`；`PresenterRig.h` 的視窗閘門；晶格的門檻與 ADR-013；port 契約的語意、`ImageView`、`SearchParams`；`OpenCvSearchOptions`、`HalconSearchOptions` 的預設值（修正的 session 要改時先把證據給使用者）。
- `tst_contract_end_to_end` 的四個 `QEXPECT_FAIL`：只有修好那一列才拿掉；不要放寬容差。修好之後一併拿掉 `perf/budgets.json` 的 `knownIssues` 與 `tests/perf/perf_matrix.cpp` 的 `knownIssue()`。
- `AiInspTool/main.cpp`、`mainwindow.*`；`aoi_target.pri` 的 `/IGNORE:4199`；表單的文字不要改成可翻譯（ADR-022）。
- `perf/baseline/casparhsu1-harness.json`：只用 `perf-compare baseline`（或 `perf.ps1 -WriteBaseline`）重建，不要手改。

## 已知問題與風險

- 基準線含一次慢的執行（範圍偏寬）：機器狀態正常時重建。`-Window`、`-Profile full`、真實影像沒有跑過。
- HALCON 修正會改變它的時間：修正合併之後 S11 開工前要重建基準線（S11 計畫的提醒）。
- HALCON dongle 的 5 分鐘限制（AI_CONTEXT 的 HALCON 列）：讀不到時先等 5 分鐘；判斷用 `test.ps1 -Filter 'tst_halcon_backend'`（0 skipped）。這個 session 全程都讀得到。
- 等使用者決定（ADR-004、S09、S10）：分數門檻要不要依 back end 分開、`nativeGray16` 要不要當預設、AB-U3 與 T7 的預算、U3 與 I1。

## 驗證（實際執行）

- `test.ps1` 預設 Release 57／57；`-Config Debug` 整個建置通過，`-Config Debug -Perf -NoBuild -Filter perf_e2e` 執行通過；`perf-compare --self-test` 20 項；`ste-lint --strict` 0 errors；`check-ai-structure.ps1` OK（1 個既有的警告：`current-status.md` 超過 80 行）。
- `test.ps1 -Perf` 整套 3 次 → 基準線；第 3 次對基準線 PASS；`perf.ps1 -NoBuild -Filter perf_matrix -InjectDelayMs 60` 閘門 FAIL（35 個回歸）。
