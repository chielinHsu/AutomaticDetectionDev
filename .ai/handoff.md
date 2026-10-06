# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-06

## 正在做

無。**S07（OpenCV back end）已完成**：分支 `step/07-opencv`，以 `--no-ff` 合併到 `main`，tag `step-07-done`。全部只在本機，**沒有 push**。S08 由新的 session 做。
**S07 中使用者的回答與決定**：（1）12 bit 是 16 bit 容器、值 0..4095；（2）有效位元數的覆寫**存在板子裡（配方 v2）、換影像沿用、UI 是參數面板的下拉選單**；（3）流程 B 只做 adapter＋測試＋T7，**UI 先不接**；（4）提供兩張去識別化的真實影像（見下）。
**仍等使用者決定**：ADR-009（I1）、U3 超標、T3 要不要涵蓋散佈的點、存檔／開啟配方與樣本庫（含流程 B）的 UI 放進哪一步、S06 的指標要不要有預算；**S07 新增**：真實影像上 use case 的種子門檻與預設的分數門檻（0.70）偏低、晶格在空槽補假缺格、T7 的預算（`docs/current-status.md`）。

## 已完成（S07）

- `src/infrastructure/backend_opencv/`：範本建立器（金字塔 × 旋轉範本）、由粗到細的搜尋器（頂層全部角度 → 逐層精修、爬坡 → 拋物線內插 → domain 的 NMS；視窗清單）、小分數圖自己算（`OpenCvCorrelation`）、樣本庫比對器（`ILibraryMatcher`）、工廠與 `setOpenCvThreads`。ADR-016。
- 組裝根：`AOI_WITH_OPENCV=ON` 時 opencv 可用（`build\release-opencv\aoi.exe --backend opencv`）；`app::workerThreads()` 給 JobRunner 與 OpenCV 共用。**use case、port、晶格沒有為了 OpenCV 改。**
- 有效位元數：`Board::significantBits`、`decideSignificantBits`（比推測小就不收）、`InspectionDocument::setSignificantBits`、配方 schema v2、參數面板「影像位元數」。
- 測試 `tests/backend/`；效能 `perf_opencv`（T1、T2a、T4、T6、T7、曲線、記憶體）、`perf_opencv_real`（真實影像）。
- 文件：`docs/steps/S07-opencv/`（四種輸出＋`perf.md`）、ADR-016、ADR-014／015 的「後續（S07）」、`docs/plan/S08`–`S12` 的「S07 完成後的提醒」、`perf/baseline/casparhsu1-opencv.json`、`perf/budgets.json`（T1、T2a、T4、T6a、T6b 標 calibrated）。

## 下一步

**S08：HALCON back end**（`docs/plan/S08-halcon.md`，開頭有 S06、S07 的提醒）。**這台機器的 HALCON 只有 runtime（沒有 `HalconCpp.h` 與 `.lib`）：開工時先問使用者能不能補裝開發元件。** 用 `tests/backend/` 的 OpenCV 測試與 `perf_opencv` 的情境當對照。

## 真實影像（不入版控）

- `D:\Benano\20260922_Pin4677_SingleFovTest\FovImages\0_0_HA.tif`、`0_0_LA.tif`（5314 × 4594、16 bit TIFF、值 0..4095）；其他 `0_0_*.tif` 全是 0。只讀，**不要複製進 repo**。
- `perf_opencv_real`：設環境變數 `AOI_REAL_IMAGE_DIR` 為上面的資料夾才跑；樣本預設 (442, 444, 80, 30)，`AOI_REAL_SAMPLE` 可換；疊圖在建置目錄 `real-*-overlay.png`。

## 不要動

- `ai-project-template/`；`docs/ux/tokens.json`；`copy.json` 只新增 key（S07 加了 `ui.params.bits*` 5 個）；`perf/budgets.json` 的 limit（S07 一個都沒改）；`knownOver("U3")` 等 S11。
- `tests/infra/fixtures/recipe-v1.json`、`recipe-v2.json`、`library-v1.json`：範例檔，**永遠要讀得進來**；格式改了就升版＋遷移。
- `AOI_SYNTHETIC_REFERENCE_HASH`；`tests/ui/PresenterRig.h` 的視窗閘門；晶格的門檻與 ADR-013；port 契約、`ImageView`、`SearchParams`；`check-deps` 的 `RULES` 與 `docs/architecture.md` 的依賴表要一起改。
- `OpenCvSearchOptions` 的預設值：改之前先跑 `tst_opencv_*` 與 `perf_opencv`（T2a、T4、T6）。

## 已知問題與風險

- 角度誤差約是第 0 層步進的 0.2 倍（48×28 的範本最大 0.42°）；範本有一部分在影像外的元件找不到；強度相關法對光照、反光、縮放敏感。
- 變異 M11（拿掉角度的爬坡）存活：測過的情境都等價，爬坡留著當防禦。
- 流程 B：每個類型每次重建範本（約 8–9 ms／類型）、只有尺寸的預篩選；小的類型可能在大元件的一部分上得到高分。
- 開圖在 UI 執行緒解碼（24.4 MP 的 LZW TIFF 約 0.4–0.5 秒）。HALCON 的轉換與來源沒有做。
- 建置：`env.ps1` 之後才 configure／build；含反斜線的編輯不要用 bash 的 `node -e`／heredoc（會吃掉），用 Write 寫腳本檔；Qt Test 要 `-o 檔案`。

## 驗證（都已實際執行）

- 見 `docs/changelog-ai.md` 本步驟那一筆（四種組態的 CTest、check-deps、ste-lint、Mermaid、說明頁、效能 3 次、變異 14 個、qmake＋jom）。
