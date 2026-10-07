# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-07

## 正在做

無。**S08（HALCON back end）已完成**：分支 `step/08-halcon`，以 `--no-ff` 合併到 `main`，tag `step-08-done`。全部只在本機，**沒有 push**。S09 由新的 session 做。
**S08 中使用者的決定**：（1）HALCON 開發檔從 CV_Algorithm 專案複製到本專案（`third_party/halcon/`）；（2）RTL 與 SDK 兩種 dongle 要能分辨，授權錯了要提醒。
**仍等使用者決定**：ADR-009（I1）、U3 超標、T3 要不要涵蓋散佈的點、存檔／開啟配方與樣本庫（含流程 B）的 UI 放進哪一步、S06 的指標要不要有預算、S07 的真實影像門檻與晶格、T7 的預算；**S08 新增**：HALCON 的影像來源要不要做、授權提醒的規則是否符合期待、兩個 back end 的分數門檻要不要分開（`docs/current-status.md`）。

## 已完成（S08）

- `src/infrastructure/backend_halcon/`（ADR-017）：shape model 的範本建立器、搜尋器、樣本庫比對器（一次 `find_shape_models`）、`ImageView` ⇄ `HImage`、位姿換算（`HalconPose.h`）、`interrupt_operator` 取消、延遲載入（`HalconRuntime`）、授權探測與判斷（`HalconLicense`）。
- 組裝根：`AOI_WITH_HALCON=ON` 時 PATH 上有 `halconcpp.dll` 就登記 halcon，授權在工廠裡檢查；`backendNotices`。main：提醒寫 stderr、吐司、選單提示。back end 選單停用不可用的項目。
- CMake：`cmake/FindHalconLocal.cmake`（`Halcon::HalconCpp`，`/DELAYLOAD`、`NOMINMAX`）、preset `release-halcon`／`debug-halcon`（OpenCV＋HALCON）、`test.ps1` 的 `-Preset release-halcon`。
- 測試 `tests/backend/tst_halcon_*`、`perf_halcon`、`perf_halcon_real`；`tst_cli`（授權／沒有 DLL 的降級）、`tst_backend_registry`、`tst_shell`（選單）。
- 文件：`docs/steps/S08-halcon/`（四種輸出＋`perf.md`）、ADR-017、ADR-010／014 的 S08 後續、`docs/plan/S09`–`S12` 的提醒、`perf/budgets.json`（T2b calibrated，limit 沒改）、`perf/baseline/casparhsu1-halcon.json`。

## 下一步

**S09：契約測試與 A/B**（`docs/plan/S09-contract-ab.md`，開頭有 S08 的提醒）。契約草稿：`tst_opencv_*` 與 `tst_halcon_*`（同一組情境）；A/B 數字：`perf_halcon`（同一次量 `CV-*`）、`perf_halcon_real`。

## 真實影像（不入版控）

- `D:\Benano\20260922_Pin4677_SingleFovTest\FovImages\0_0_HA.tif`、`0_0_LA.tif`（5314 × 4594、16 bit TIFF、值 0..4095）。只讀，**不要複製進 repo**。`perf_opencv_real`、`perf_halcon_real`：設 `AOI_REAL_IMAGE_DIR` 才跑；樣本預設 (442, 444, 80, 30)，`AOI_REAL_SAMPLE` 可換；疊圖在建置目錄 `real-*-overlay.png`。

## 不要動

- `ai-project-template/`；`docs/ux/tokens.json`；`copy.json` 只新增 key（S08 加 2 個）；`perf/budgets.json` 的 limit；`knownOver("U3")` 等 S11。
- `third_party/halcon/`（照原樣；換版本整份換掉）；`tests/infra/fixtures/*.json`（範例檔永遠要讀得進來）。
- `AOI_SYNTHETIC_REFERENCE_HASH`；`PresenterRig.h` 的視窗閘門；晶格的門檻與 ADR-013；port 契約、`ImageView`、`SearchParams`；`check-deps` 的 `RULES` 與 `docs/architecture.md` 的依賴表要一起改。
- `OpenCvSearchOptions`、`HalconSearchOptions` 的預設值：改之前先跑對應的 `tst_*` 與 `perf_*`（HALCON 看 AS-*、SUB-*、T6）。

## 已知問題與風險

- HALCON：強雜訊時位置與角度比 OpenCV 不穩（T6b p95 0.48，預算 0.5）；T2b 3 次中 1 次超過預算（機器變慢時）；真實影像 HA 上只找到少數針腳；0° 附近 ±0.5° 的元件可能回報接近 0°；建模型 7–19 ms、不能中斷；授權第一次約 1.6 秒；影像來源沒有做。
- OpenCV：角度誤差約是第 0 層步進的 0.2 倍；強度相關法對光照、反光、縮放敏感。
- 流程 B：兩個 adapter 各抄一份合併規則（S09 用契約鎖住）；UI 沒有接。
- HALCON 的 dongle：曾經插著卻讀不到（錯誤 2300），重插後恢復；這時 HALCON 的測試會「跳過」而不是失敗，**看 log 的 SKIP**，別當成通過。這台電腦另有 HALCON 22.11（AppData，本專案沒用）。
- 建置：`env.ps1` 之後才 configure／build；VS 安裝程式在更新時 `vswhere` 找不到 VS；**PowerShell 的 `Get-Content -Raw`／`Set-Content` 會把 UTF-8 檔案當 CP950 讀壞**，改檔一律用編輯工具；Qt Test 要 `-o 檔案`；`QVERIFY2` 的訊息在通過時也會求值（別在訊息裡讀一次性的資料）。

## 驗證（都已實際執行）

- 見 `docs/changelog-ai.md` 本步驟那一筆。
