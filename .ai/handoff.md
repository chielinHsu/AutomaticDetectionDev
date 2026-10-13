# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Fable 5.1)
- 日期：2026-10-13

## 正在做

無。**HALCON 準確度的修正完成**（分支 `fix/halcon-accuracy`，`--no-ff` 合併到 `main`，沒有 tag、沒有 push）。**接下來是 S11**（這個 session 收工時用 `spawn_task` 開；分支 `step/11-optimize`；開工前確認 `tst_contract_end_to_end` 的 XFAIL 只剩 `opencv B1`）。

## 這次做了什麼（細節在 `docs/changelog-ai.md` 最上面那一筆、ADR-004 的「後續（2026-10-13）」、`docs/steps/S09-contract-ab/halcon-accuracy-probe.md`）

- 根因（探測，不是推測）：B1 ＝ 'auto' 對比落在雜訊量級；B2 ＝ 模糊後「電極–底–電極」只差主體上下邊三成的點（半格距分數 0.83），重複偵測是 use case 驗證階段對半格距種子的後果；B3 ＝ 'auto' 把門檻拉到 38、主體長邊掉出模型；O1 ＝ 角度 0 的範本帶著樣本自己的雜訊。
- 修正（只動 `infrastructure/backend_halcon`，`HalconSearchOptions` 的預設）：MaxOverlap 0.5 → 0.2；建範本時量樣本的邊寬與相對雜訊，邊寬 ≥ 5 → `mean_image` 5×5 三次＋least_squares＋只追蹤到金字塔第 2 層，相對雜訊 ≥ 4% → `mean_image` 3×3 三次＋least_squares_high，其餘不平滑（和 S08 相同）；決定在 `HalconTemplateTuning`，搜尋影像跟著做；流程 B 不平滑。
- 使用者的決定：「強雜訊」改相對定義（`isStrongNoise`，數字沒放寬）；O1 列為 ADR-004 的 D6（OpenCV B1 列的 `QEXPECT_FAIL` 留著當哨兵）；OpenCV 的半格距現象只記錄。
- `perf/budgets.json` 的 `knownIssues` 清空、`perf_matrix` 的 `knownIssue()` 拿掉、基準線重建。

## 建置與測試

- `test.ps1` 預設 57／57（HALCON 0 跳過；XFAIL 2 列：opencv B1、`tst_image_sources_agree` 的 1 列）；`-Config Debug` 57／57。
- `tst_contract_end_to_end`：HALCON B1 0.353 px／0.745°（上限 0.80）、B2 0.215 px／0.575°（上限 0.6）、B3 0.142 px／0.650°（上限 1.03）。**餘裕小**（B2 4%、B1 7%）：合成板子是確定的，同一個建置每次一樣；換 HALCON 版本要重跑。
- 效能：`perf_halcon` 0 項超出預算（T2b 20.4／24.7 ms、T4 24.7／42.9 ms、T6b p95 0.114 px）；`perf_ab` 低對比 100%、重模糊 0 誤判。

## 不要動

- `HalconSearchOptions` 的新預設（使用者確認過）；要改先看 `halcon-accuracy-probe.md` §5、§8（gauss_filter、binomial_filter、'interpolation'、全部平滑都試過、都比較差）。
- `ai-project-template/`；`docs/ux/tokens.json`；`copy.json` 只新增 key；`perf/budgets.json` 的 limit；`knownOver("U3")`、`knownOver("I1")` 等 S11。
- `third_party/halcon/`；`tests/infra/fixtures/*.json`；`AOI_SYNTHETIC_REFERENCE_HASH`；`PresenterRig.h` 的視窗閘門；晶格的門檻與 ADR-013；port 契約的語意、`ImageView`、`SearchParams`；`OpenCvSearchOptions` 的預設值。
- `tst_contract_end_to_end` 的 opencv B1 `QEXPECT_FAIL`：是刻意接受的差異（D6），修好 O1 才拿掉；不要放寬容差。
- `AiInspTool/main.cpp`、`mainwindow.*`；`aoi_target.pri` 的 `/IGNORE:4199`；表單的文字不要改成可翻譯（ADR-022）。
- `perf/baseline/casparhsu1-harness.json`：只用 `perf.ps1 -WriteBaseline` 重建，不要手改。

## 已知問題與風險

- use case 驗證階段的漏洞（視窗裡找到的匹配離別的已知候選更近時仍當成那個缺格的元件）：`current-status.md` 待決定（10），沒有改 application。
- 探測程式 `tests/backend/probe_*.cpp` 不在 `tests.pro`，要跑要單獨 qmake（changelog 的注意事項）。
- HALCON dongle 的 5 分鐘限制（AI_CONTEXT 的 HALCON 列）：這個 session 全程都讀得到。
- 等使用者決定（ADR-004、S09、S10）：分數門檻要不要依 back end 分開、`nativeGray16`、AB-U3 與 T7 的預算、U3 與 I1、真實影像 HA 的位置。

## 驗證（實際執行）

- `test.ps1` 預設 Release 57／57、`-Config Debug` 57／57；`tst_halcon_backend` 19 項 0 skipped；`ste-lint` 0 errors；`check-ai-structure.ps1` OK（既有警告：`current-status.md` 超過 80 行）。
- `perf.ps1 -NoBuild -Filter perf_halcon`（1 次）、`-Filter perf_ab -Runs 3`、`-Runs 3 -WriteBaseline`（結果見 changelog）。
