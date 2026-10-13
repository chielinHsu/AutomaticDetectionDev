# S12 — 匯出、打包、最終文件

| 項目 | 內容 |
|---|---|
| 層級 | 發行 |
| 分支 | `step/12-release` |
| 依賴 | S11 |
| 產出位置 | `src/infrastructure/export/`、`packaging/`、`docs/manual/`、`docs/steps/S12-release/` |

> **S01 修訂（2026-10-05）**：匯出要包含 **CAD 清單**：元件名稱、X、Y、角度、元件類型（料號）、正反面（`docs/ux/ux-spec.md` §12.1 #9）；S01 原型有預覽。單位與原點見 ADR-006 未決 #1。

> **S06 完成後的提醒（2026-10-06）**：（1）配方的 schema 是 v1（`docs/adr/015-recipe-and-library-storage.md`；範例檔 `tests/infra/fixtures/recipe-v1.json` 要永遠讀得進來）。v1 **沒有影像的路徑**（`Board::name` 只是檔名）：要能〔開啟配方〕，升成 v2、加影像路徑、在 `JsonRecipeStore::builtInMigrations()` 加 v1 → v2 的遷移。（2）**存檔與開啟配方的 UI 還沒有任何一步涵蓋**（使用者在 S06 決定先不接，記在 `docs/current-status.md`）；背景存檔用 `application/document/RecipeSaver`，存完才 `markSaved`，而且要標在取快照時的位置。（3）參考影像不壓縮（Qt 6.8.0 的 `qUncompress` 對截斷的資料是未定義行為，ADR-015）。（4）S06 起用 Qt 6.12.0，打包時要帶 6.12.0 的 DLL；OpenCV 版要帶 `opencv_world490.dll`。

> **S07 完成後的提醒（2026-10-06）**：配方的 schema 升到 **v2**（`board.significantBits`：影像的有效位元數，0 ＝ 自動；ADR-015 的 S07 後續）。範例檔 `tests/infra/fixtures/recipe-v1.json`、`recipe-v2.json` 都要永遠讀得進來（不要改）。打包要帶 OpenCV 的 DLL（`opencv_world490.dll`，建置時已複製到 `<建置>/bin`、`AiInspTool.exe` 的旁邊；OpenCV 預設編進去）。

> **S08 完成後的提醒（2026-10-07）**：（1）**HALCON 不要打包進安裝程式**：`halconcpp.dll`、`halcon.dll` 與授權都屬於使用者電腦上的 HALCON 安裝（`%HALCONROOT%`），程式用延遲載入、找 PATH；沒有 HALCON 的電腦，halcon 在選單上停用並說原因。HALCON 的 runtime 授權（RTL）是部署用的，開發用 SDK 授權（程式在「用 runtime 授權、SDK 的 dongle 沒插」時提醒；只有 runtime 授權的電腦不提醒）。（2）開發檔在 `third_party/halcon/`（24.11.2）；打包的電腦上 HALCON 的主、次版本要和它相同（`HalconRuntime` 會檢查，不同就不可用）。（3）`copy.json` 新增 `ui.backend.unavailable`、`ui.backend.halcon.sdkDongleMissing`。

> **建置改 qmake 之後的提醒（2026-10-07，[ADR-018](../adr/018-qmake-only-build.md)）**：建置只用 qmake（`AiInspTool/AiInspTool.pro`；CMake、preset、CTest 都已移除，S02–S08 文件裡的那些指令是歷史紀錄，對照表在 ADR-018），執行檔叫 `AiInspTool.exe`（GUI 子系統），在 `<建置>/bin`。OpenCV 與 HALCON **預設編進去**（以前的 preset `release-halcon`）；關掉用 qmake 變數 `AOI_WITH_OPENCV=OFF`／`AOI_WITH_HALCON=OFF`。命令列：`scripts/dev/test.ps1`（建置＋測試；`-Config Debug|Profile`、`-NoOpenCv`、`-NoHalcon`、`-Perf`、`-Filter`、`-NoBuild`）；Qt Creator 開 `AiInspTool/AiInspTool.pro` 也能建置、執行與跑測試。**新增測試**：在 `AiInspTool/tests/<資料夾>/` 加一個 `.pro`（照同資料夾的寫法：`TARGET`、需要時 `AOI_LINK_EXTRA`、`include(<資料夾>_tests.pri)`、`SOURCES`），再列進 `AiInspTool/tests/tests.pro` 的 `AOI_TESTS`（效能量測列進 `AOI_PERF_TESTS`；只有某個 back end 才有的放進對應的 `equals(AOI_WITH_…, ON)` 區塊）。**新增原始碼**要加進對應的 `.pro`（不會自動納入）。S09 之前還有一個 session B（授權提醒的卡片、執行中切換 back end、Designer 的 `.ui`），開工時先看 `.ai/handoff.md`。打包取 `<建置>/bin/AiInspTool.exe` 與它旁邊的 `opencv_world490.dll`（`bin/` 裡也有測試執行檔，不要一起打包），`windeployqt` 對 `AiInspTool.exe` 執行。`AiInspTool.exe` 是 GUI 子系統：啟動失敗的原因寫在 stderr，使用者雙擊執行時看不到（要不要改成對話框，是 S12 的決定）。

> **S11 完成後的提醒（2026-10-13，[ADR-024](../adr/024-box-glyph-cache.md)）**：（1）`perf/budgets.json` 已經沒有 `measured-over-limit` 的項目（U3 在 S11 做進預算：字形快取、延後格式化、文件的場景快取），只剩 I1 的 `calibrated-at-limit`（真實視窗的數字沒有重量；ADR-009 等使用者決定）；`knownIssues` 是空的。發行說明裡不必再列 U3。（2）字形快取只在 devicePixelRatio ＝ 1 時生效（這台機器 96 DPI）；在高 DPI 的目標機器上 U3 會回到填色矩形的速度（S04 的數字），發行前在目標機器上跑 `perf.ps1 -Window -Filter perf_ui_s04` 看一次。（3）基準線 `perf/baseline/casparhsu1-harness.json` 在 S11 收工時重建（`-Runs 3 -WriteBaseline`），發行版照舊在目標機器上再建一次。

> **S10 完成後的提醒（2026-10-13，[ADR-023](../adr/023-perf-regression-gate.md)）**：發行版重跑全套效能測試並更新基準線 ＝ `scripts/dev/perf.ps1 -Runs 3 -WriteBaseline`（約 35 分鐘；寫 `perf/baseline/<機器>-harness.json`，入版控），報表在 `perf/results/<日期>-<commit>-<機器>.html`。在目標機器上跑時，HALCON 的列不可以是「未驗證」（跳過）；`perf/budgets.json` 的 `knownIssues` 到那時應該已經清空（U3 在 S11、B1–B3 在 `fix/halcon-accuracy`），還在的要在發行說明裡列出。`tests/perf/` 的兩個執行檔和其他測試一樣在 `bin/`，不要打包。

> **S09 完成後的提醒（2026-10-09）**：主視窗多了〔A/B 比較…〕（檢視選單與工具列）與比較視窗（`presentation/compare/`）：它是給工程與驗收用的工具，要不要留在發行版（或藏起來）由使用者決定。契約測試（`tests/contract/`）在每一種建置都有；發行前在目標機器上跑一次，HALCON 的列不可以是 SKIPPED。`copy.json` 新增 46 個 `ui.compare.*` 的 key（兩種語言）。

<!--STE-->
## Purpose
This step finishes the first release. It adds export, builds an installer and completes the user manual.

## Reason
The user needs the boxes in other tools. A program that the user cannot install or learn has no value.

## Effect
The user installs the program, makes the boxes, exports them and finds the answer in the manual.
<!--/STE-->

**繁中**：讓第一版可以真正交給人用：匯出、安裝、說明文件。

## 任務

1. **匯出**：Recipe JSON（定案 schema）、CSV（座標/角度/狀態/分數）、DXF（矩形，作法在 ADR-005 決定：自寫簡易 writer 或使用函式庫）。匯出走 `IExporter` port，每種格式一個 adapter。
2. **打包**：`windeployqt`、OpenCV DLL、**HALCON 執行環境與授權的說明**（HALCON 授權不隨安裝檔散布，文件要清楚寫出）；沒有 HALCON 的安裝選項。
3. **Release 重跑**：在 Release 組態重跑 S10 全套，更新 baseline；確認 `/W4 /WX` 乾淨。
4. **使用手冊**（`docs/manual/`，STE 英文＋繁中）：快速開始、每個工具、疑難排解（無匹配、授權、效能慢）。
5. **文件總覽**：`docs/index.html` 互動入口，串起每一步的 explainer、diagram、storyboard、perf 結果。
6. **解說影片**：詢問使用者是否要實際製作（語音合成與剪輯工具選擇需要使用者決定）；不要擅自呼叫付費服務。
7. **v0.1.0 tag**，撰寫 CHANGELOG。

## SOLID 重點

匯出格式以 adapter 增減（O）；使用者介面只依賴 `IExporter`（D）。

## 效能檢查（寫入 `perf.md`）

Release 版 S10 全套；匯出 5 000 個物件的時間；安裝後冷啟動時間（目標 ≤ 2 s，量測後確認）。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | 匯出與打包的 Purpose/Reason/Effect；手冊目錄 |
| 2 | diagram.md | 匯出 adapter 結構圖；安裝內容圖（哪些 DLL 從哪來） |
| 3 | explainer.html | 文件總覽入口＋「30 秒上手」互動導覽 |
| 4 | storyboard.md | 90 秒產品介紹（若使用者同意才製作成片） |

## 完成定義

- [ ] 在乾淨的 Windows 環境安裝並能啟動（有／無 HALCON 兩種）
- [ ] 三種匯出格式的往返或外部工具開啟測試通過
- [ ] Release 版效能全套通過
- [ ] 手冊與文件入口完成
- [ ] `node tools/ste-lint.mjs --strict` 無 error
- [ ] tag `v0.1.0`

## Commit 範本

```
feat(release): add exporters, packaging, manual and docs index

- JSON/CSV/DXF exporters behind IExporter
- windeployqt packaging, optional HALCON runtime notes
- Release-build performance baseline refreshed
- User manual (STE + zh-TW) and interactive documentation index
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 AGENTS.md、docs/current-status.md、docs/plan/S12-release.md，
再依 .agents/harness/project-rules.md 的 Step Protocol 完成步驟 S12（分支 step/12-release）。只做 S12。
製作實際解說影片前先詢問使用者是否同意與工具選擇。完成後：commit、合併到 main、打 tag v0.1.0、更新 docs/current-status.md 與交接單（ai-handoff skill）、對我提示 commit 內容。
```
