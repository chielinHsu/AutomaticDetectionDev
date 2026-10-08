# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Sonnet 5.5，中途換成 Claude Opus 5.5)（只做了模型路由；下面 session A 的內容是之前的 Claude Opus 5.5 寫的）
- 日期：2026-10-08

## 新增（2026-10-08）：模型路由（ADR-019；分支 `chore/model-routing`，**沒有合併到 `main`**、沒有 push）

`MODEL_ROUTING.md`（`CLAUDE.md` 匯入）、`.claude/agents/`（`fast-worker`／`developer`／`architect`）、`.claude/hooks/model_router.mjs`（`settings.json` 的 `UserPromptSubmit`；只注入提示、不切換主模型、失敗不擋；`AOI_MODEL_ROUTER=off` 關掉）。改這些檔案後跑 `node scripts/ai/check-model-routing.mjs`。`haiku` 別名目前對應 Haiku 4.5。

## 正在做

無。**S09 之前的 session A（建置改成只用 qmake）已完成**：分支 `build/qmake-only`，以 `--no-ff` 合併到 `main`（沒有 tag、沒有 push）。設計決定：[ADR-018](../docs/adr/018-qmake-only-build.md)。
**下一個是 session B**（授權提醒改卡片＋狀態列、執行中切換 back end、Designer 的 `.ui`），S09 在 B 之後。
**仍等使用者決定**（S08 之前留下的，見 `docs/current-status.md`）：ADR-009（I1）、U3 超標、T3、存檔／開啟配方與樣本庫（含流程 B）的 UI、S06 的指標、S07 的門檻與晶格、T7、HALCON 的影像來源、授權提醒的規則、兩個 back end 的分數門檻。

## 建置（2026-10-07 起）

- 只有 qmake：Qt Creator 開 `AiInspTool/AiInspTool.pro`；命令列 `powershell -ExecutionPolicy Bypass -File scripts/dev/test.ps1`（預設 Release＋OpenCV＋HALCON，53 項；`-Config Debug|Profile`、`-NoOpenCv`、`-NoHalcon`、`-Perf`、`-Filter`、`-NoBuild`、`-BuildDir`）。CMake、preset、CTest 都已移除；`docs/steps/S02`–`S08` 的那些指令是歷史，對照表在 ADR-018。
- 執行檔 `AiInspTool.exe`（GUI 子系統）在 `<建置>/bin`；命令列的建置在 `build/qmake-<組態>[-noopencv][-nohalcon]`，Qt Creator 預設在 `AiInspTool/build/<kit>_<組態>`。
- 新增原始碼：加進對應的 `.pro`（明確列出，不自動納入）。新增模組：`AiInspTool/qmake/aoi_deps.pri`＋`docs/architecture.md`＋`check-deps` 的 `RULES` 三邊一起改。新增測試：`AiInspTool/tests/<資料夾>/<名稱>.pro`＋`AiInspTool/tests/tests.pro` 的清單（`AOI_TESTS`／`AOI_PERF_TESTS`）。

## 已完成（session A）

- qmake 專案（`.qmake.conf`、`qmake/*.pri`、15 個 `src` 的 `.pro`、`tests.pro`＋60 個測試與 support 的 `.pro`）、`tests/support/test_environment.cpp`、`test.ps1`／`env.ps1` 改寫、`CompositionRoot.cpp` 的訊息、移除 CMake、`.gitignore`。
- 驗證：8 種組合 45／50／48／53 全過（Release、Debug；HALCON 拿不到授權那段跳過的部分已在 2026-10-08 補跑通過）；Qt Creator 的三種組態模擬建置成功（0 個警告），Profile 跑全部測試 53／53；Release 開使用者的 TIFF 成功、`--backend halcon` 有授權提醒；效能 10 項通過、和 CMake 時代同一個量級（ADR-018「驗證」）。
- 文件：ADR-018、ADR-003／017 的後續、`decisions.md`、`AI_CONTEXT.md`、`architecture.md`、`README.md`、`docs/plan/S09`–`S12`、`perf-test-plan.md`、`current-status.md`。

## 下一步

session B：使用者 2026-10-07 給的 prompt（agent A 用 `spawn_task` 開的那個 session）。開工時先讀 ADR-018 與本交接單的「建置」。

## 真實影像（不入版控）

- `D:\Benano\20260922_Pin4677_SingleFovTest\FovImages\0_0_HA.tif`、`0_0_LA.tif`（S07、S08 的真實影像量測）；`D:\Benano\20260922_Pin4677_Milti4FovTest\FovImages\0_0_B.tif` 等（4 個 FOV，session A 用 B 驗證開圖）。只讀，**不要複製進 repo**。`perf_*_real`：設 `AOI_REAL_IMAGE_DIR` 才跑。

## 不要動

- `ai-project-template/`；`docs/ux/tokens.json`；`copy.json` 只新增 key；`perf/budgets.json` 的 limit；`knownOver("U3")` 等 S11。
- `third_party/halcon/`（照原樣）；`tests/infra/fixtures/*.json`；`AOI_SYNTHETIC_REFERENCE_HASH`；`PresenterRig.h` 的視窗閘門；晶格的門檻與 ADR-013；port 契約、`ImageView`、`SearchParams`；`OpenCvSearchOptions`、`HalconSearchOptions` 的預設值（改之前先跑對應的 `tst_*`、`perf_*`）。
- `AiInspTool/main.cpp`、`mainwindow.*`（沒有使用，使用者決定保留）；`aoi_target.pri` 的 `/IGNORE:4199`；測試清單不要改成看 `bin/` 的檔案。

## 已知問題與風險

- **HALCON 的授權（錯誤 2300）**：2026-10-07 15:06 到 2026-10-08 13:20 拿不到授權，期間 HALCON 的測試與 `perf_halcon` 是「跳過」（`test.ps1` 列在「有跳過項目的測試」，**不是通過**）。重插、換埠、重新啟動都沒恢復，使用者在另一個專案 `Algo_SkewInspect` 拔插之後恢復，原因不明；恢復後已補跑並通過。**`hhostid` 在這台不列出這個 dongle，不能拿來判斷**；用 `test.ps1 -Filter 'tst_halcon_backend'`。授權 `SESSIONS=1`：別的 HALCON 程式同時在跑的影響沒有驗證。
- 新的專案只用命令列模擬過 Qt Creator 的步驟，還沒有在 Qt Creator 的畫面裡打開過。Qt Creator 的「執行」設定會列出所有測試，要選 `AiInspTool`。
- 建置進行中不要改 `.pro`／`.pri`（Makefile 會觸發 qmake 重跑）。`scripts/**/*.ps1` 要 UTF-8 with BOM；PowerShell 的 `Get-Content -Raw`／`Set-Content` 會把 UTF-8 當 CP950，改檔用編輯工具；qmake 的 `#` 在引號裡也是註解。
- HALCON：強雜訊時比 OpenCV 不穩（T6b p95 0.48）；T2b 偶爾超過預算；HA 真實影像只找到少數針腳；授權第一次約 1.6 秒；影像來源沒有做。流程 B 的 UI 沒有接。

## 驗證（都已實際執行）

- 見 `docs/changelog-ai.md` 最上面那一筆與 ADR-018 的「驗證」。
