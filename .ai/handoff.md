# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-09

## 正在做

無。**S09（契約測試與 A/B 比較）完成**：分支 `step/09-contract-ab`，`--no-ff` 合併到 `main`、tag `step-09-done`（沒有 push）。**下一個是 S10**（`docs/plan/S10-perf-harness.md`，開頭有「S09 完成後的提醒」）。
**HALCON 的部分未測**：2026-10-09 12:39 起讀不到 dongle（錯誤 2300，第四次）。使用者決定 dongle 相關的部分記下來、等之後再測。恢復後照 `docs/steps/S09-contract-ab/perf.md` §6 補：`tst_halcon_backend` 0 skipped → `test.ps1` 預設與 `-Config Debug` → `test.ps1 -Perf -Filter 'perf_ab'` 3 次 → 補 `perf.md` §2、ADR-004 §3、`explainer.html` 的 `ab-data`（現在顯示標明「示意」的資料）。

## S09 做了什麼

- 契約測試 `tests/contract/`：一個 back end 一列（fake、opencv、halcon），沒編進來或沒有授權的 SKIPPED 並說原因；容差只在 `ContractTolerances.h`（附依據；改之前看 ADR-004）。S03／S07／S08 的契約草稿與端到端併進來。
- 找到並修掉：假 back end 不檢查角度範圍（Liskov）。變異測試 4 個都被抓到。
- A/B：`application/compare/BackendComparison`、`presentation/compare/`（`ComparePresenter`、`CompareWindow` ＋ `.ui`、`CompareOverlayItem`）、主視窗〔A/B 比較…〕。AB-U1a p95 19.6–27.0 ms（U1a 33）、AB-U1b 30.5–38.8 ms（U1b 50）。
- ADR-004：契約一樣、準確度各有上限；接受的差異（分數不是同一把尺、雜訊時 HALCON 不穩、反光時 OpenCV 偏 0.5 px…）。

## 建置與測試

- `test.ps1`：預設 **56 項**（`-NoHalcon` 54、`-NoOpenCv` 53、兩個都關 51）。契約測試在每一種建置都有。
- `test.ps1` 以 `<建置>/test-work` 為工作資料夾：測試的輸出直接寫在目前的資料夾。
- 契約測試比錯誤碼用 `QCOMPARE_ERROR`（在成功的 `Result` 上呼叫 `error()` 會中止整個執行檔）。

## 不要動

- `ai-project-template/`；`docs/ux/tokens.json`；`copy.json` 只新增 key；`perf/budgets.json` 的 limit；`knownOver("U3")` 等 S11。
- `third_party/halcon/`；`tests/infra/fixtures/*.json`；`AOI_SYNTHETIC_REFERENCE_HASH`；`PresenterRig.h` 的視窗閘門；晶格的門檻與 ADR-013；port 契約的語意、`ImageView`、`SearchParams`；`OpenCvSearchOptions`、`HalconSearchOptions` 的預設值（`nativeGray16` 等使用者決定）。
- `AiInspTool/main.cpp`、`mainwindow.*`；`aoi_target.pri` 的 `/IGNORE:4199`；表單的文字不要改成可翻譯（ADR-022）。

## 已知問題與風險

- HALCON 授權：判斷用 `test.ps1 -Filter 'tst_halcon_backend'`（0 skipped），不要用 `hhostid`；`SESSIONS=1`，跑 HALCON 的量測時別同時開其他用 HALCON 的程式。
- 等使用者決定（ADR-004）：分數門檻要不要依 back end 分開、`nativeGray16` 要不要當預設。
- 真實影像不在 `D:\Benano\20260922_*` 了；`D:\Benano\20261007_4677_AdvancePin_Proj\` 有新的，能不能用要問使用者。
- AB-U3（改門檻之後重新比較）p95 48.8–63.4 ms，沒有預算（S10、S11）。
- UI 效能量測受機器負載影響很大（同一份程式兩輪可以差 2–3 倍），比較時要前後交替量。

## 驗證（實際執行）

- 見 `docs/changelog-ai.md` 最上面那一筆（各組合的通過數、HALCON 12:3x 的結果、變異測試、效能）。
