# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-08

## 正在做

無。**S09 之前的 session B 已完成**：分支 `feat/backend-switch-ui-forms`（三個 feat commit ＋ 交接文件），以 `--no-ff` 合併到 `main`（沒有 tag、沒有 push）。**下一個是 S09**（`docs/plan/S09-contract-ab.md`，開頭有「session B 完成後的提醒」）。
**仍等使用者決定**（見 `docs/current-status.md`）：ADR-009（I1）、U3 超標、T3、存檔／開啟配方與樣本庫（含流程 B）的 UI、S06 的指標、S07 的門檻與晶格、T7、HALCON 的影像來源、授權提醒的規則、兩個 back end 的分數門檻。

## session B 做了什麼（設計見 ADR-017 的後續、ADR-021、ADR-022）

1. 授權提醒：訊息列的**提醒卡片**（按〔關閉〕才收起，被搜尋的卡片蓋過後再出現）＋狀態列最左邊的 back end 標示（「HALCON · runtime 授權」）。`Backend::notices`、`BackendPresenter`、`IBackendView`。
2. **執行中切換 back end**：`SearchEngine`（`shared_ptr` 共用，舊的 adapter 等工作結束才釋放）、`BackendSwitch`（規則，沒有 Qt）、背景建立、失敗回到原本的並顯示卡片、搜尋中先作廢、有提案時用新的重搜同一個區域、**啟動時用上次用的**（`%APPDATA%\AOI\AutoCadCreated.ini` 的 `backend/last`；`--backend` 照舊嚴格）。
3. **Designer 的 `.ui`**：`src/presentation/shell/MainWindow.ui`、`MessageBar.ui`、`EmptyCard.ui`、`objects/ObjectsPanel.ui`、`params/ParamsPanel.ui`（`presentation.pro` 的 `FORMS`）；畫布、訊息列、兩個面板是提升元件；表單的文字一律 `notr` 佔位。

## 建置與測試（ADR-018）

- `scripts/dev/test.ps1`：預設 **54 項**（新增 `tst_backend_switch`），`-NoHalcon` 51、`-NoOpenCv` 49、兩個都關 46。
- `CoreServices` 的 `backend`、`generateObjects` 合成 `engine`（`engine->backend`、`engine->generate`）。`tst_cli` 啟動程式時一律設 `AOI_SETTINGS_DIR` 指到暫存資料夾。
- 新增原始碼、表單、測試都要加進對應的 `.pro`（明確列出）。建置進行中不要改 `.pro`／`.pri`。

## 不要動

- `ai-project-template/`；`docs/ux/tokens.json`；`copy.json` 只新增 key（`ui.status.summary`、`ui.status.none` 已不使用，沒有刪）；`perf/budgets.json` 的 limit；`knownOver("U3")` 等 S11。
- `third_party/halcon/`；`tests/infra/fixtures/*.json`；`AOI_SYNTHETIC_REFERENCE_HASH`；`PresenterRig.h` 的視窗閘門；晶格的門檻與 ADR-013；port 契約、`ImageView`、`SearchParams`；`OpenCvSearchOptions`、`HalconSearchOptions` 的預設值。
- `AiInspTool/main.cpp`、`mainwindow.*`（精靈產生，沒有使用，使用者決定保留）；`aoi_target.pri` 的 `/IGNORE:4199`。
- 表單裡的文字不要改成可翻譯（會和 `copy.json` 衝突，ADR-022）。

## 已知問題與風險

- **HALCON 的授權**：判斷用 `test.ps1 -Filter 'tst_halcon_backend'`（log 的 Totals 0 skipped），不要用 `hhostid`。授權探測整個程式只做一次：第一次切到 HALCON 失敗之後插上 dongle 要**重新啟動程式**。`SESSIONS=1`：從 HALCON 換走之後仍初始化著，會不會佔住授權沒有驗證；同時跑兩邊的 HALCON 測試要避開。
- 搜尋時 back end 無法使用的錯誤卡片（S08 的 `errorCard`）標題把錯誤訊息當成 back end 名稱，沒有改。
- UI 效能量測受機器負載影響很大（同一份程式兩輪可以差 2–3 倍），比較時要前後交替量（ADR-022 的表）。
- Qt Creator 的 Designer：使用者 2026-10-09 打開五個表單確認過（沒有存檔）。之後在 Designer 存檔，整個 `.ui` 會被重寫，diff 會比實際的修改大。

## 真實影像（不入版控）

- `D:\Benano\20260922_Pin4677_SingleFovTest\FovImages\0_0_HA.tif`、`0_0_LA.tif`；`D:\Benano\20260922_Pin4677_Milti4FovTest\FovImages\0_0_B.tif` 等。只讀，不要複製進 repo。`perf_*_real`：設 `AOI_REAL_IMAGE_DIR` 才跑。

## 驗證（都已實際執行）

- 見 `docs/changelog-ai.md` 最上面那一筆、ADR-021 與 ADR-022 的「證據／驗證」。
