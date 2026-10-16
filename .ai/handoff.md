# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Fable 5.1 → Claude Opus 5.5)
- 日期：2026-10-16

## 正在做

無（這個 session 結束時 `main` 在「合併 feat/step-guidance」那個 commit；它之前一個是晶格位置吸附的合併 48f7ef4）。**操作步驟的提示（[ADR-027](../docs/adr/027-step-guidance.md)）完成**：分支 `feat/step-guidance`（從 `main` eb95b79 開；session 在 worktree，`main` 在主 checkout `D:\AOI_Project\Qt\AutoCadCreated` 簽出，用 `git -C <主 checkout> merge --no-ff` 合併；沒有 tag、沒有 push）。**另一個 session 同時在做晶格位置吸附**（`feat/lattice-snapped-positions`，ADR-026，會改 `ProposalSession`、`InspectionSession`、`ParamsPanel`、`ObjectsPanel`、`MessageBar` 的卡片文字、`copy.json`）：它合併時如果和本分支衝突，`copy.json` 兩邊都是追加 key（保留兩邊）、`InspectionSession` 本分支沒改。**接下來仍是 S12**（Start prompt 在 `docs/plan/S12-release.md` 底部），或使用者指定的 P2。

## 這次做了什麼（細節在 `docs/changelog-ai.md` 最上面那一筆、ADR-027、`docs/current-status.md` 的「操作步驟的提示」）

- 使用者要求步驟提示與影片提示；先確認現況、比較三個方案與四種影片做法，使用者選了訊息列補強＋操作步驟 dock＋程式產生的 PNG 逐格動畫＋F1。
- application：`interaction/StepGuide`（`guideStatus(const InspectionSession&)`：目前這一步、做完的、進行中、要重做）。presentation：左側「操作步驟」dock（`guidance/GuidancePanel`）、`GuideDemo.h`（示範的張數與檔名）、`AutoRoiPresenter::publishGuidance`（狀態變了才呼叫 View）、`Searching` 的提示、空白卡片的三步、說明選單 `F1`；app：記住 dock 的開關。
- 示範畫面：`tests/ui/record_guidance`（不是測試，`tests.pro` 的 `AOI_TOOLS`）產生 `docs/ux/guidance/*.png`（13 張），`aoi_ux.qrc` 內嵌。**UI 改了要重跑再重新建置**（張數改了三邊一起改，`tst_messages` 會擋）。
- `copy.json` 新增 36 個 key；測試 `tst_step_guide`（新）、`tst_shell`、`tst_presenter`、`tst_messages` 各加。

## 建置與測試

- `test.ps1` 預設 Release：**58／58 通過**（2026-10-16 最後一輪；新增 `tst_step_guide` 後是 58 項）；HALCON 0 跳過；有跳過項目的 5 個測試（`tst_backend_registry`、`tst_shell`、`tst_presenter` 的截圖、`tst_opencv_image`、`tst_image_sources_agree`）與 2 個 XFAIL（`tst_contract_end_to_end` 的 opencv B1、`tst_image_sources_agree` 的 palette BMP）和改之前相同。`tst_step_guide` 18 passed、`tst_shell` 37 passed＋1 skipped、`tst_presenter` 84 passed＋1 skipped、`tst_messages` 17 passed。
- `test.ps1 -Config Debug`：**58／58 通過**（2026-10-16；HALCON 0 跳過；跳過與 XFAIL 的清單和 Release 相同；`tst_presenter` 84 passed＋1 skipped、整組 85 秒，`tst_shell` 37＋1、`tst_step_guide` 18、`tst_messages` 17）。前一晚的幾次 Debug 都是 57／58（見下一條）。
- `node tools/ste-lint.mjs`：979 sentences、0 errors；`node tools/check-deps.mjs`：225 files、0 violations。
- `test.ps1 -Perf -Filter 'perf_ui_s04'`：2026-10-16 和 `main` eb95b79 前後交替各兩次，U3 p95 前 29.4、13.9／後 10.1、15.2，I6 前 3.6、4.9／後 2.8、3.1，全部在預算內、差異在雜訊內；細節在 changelog。沒有跑 `perf.ps1 -WriteBaseline`（使用者另外指示）。

## 注意

- `publishGuidance` 只在 `GuideStatus` 變了才碰 dock（U3 的保護）；`publishAll` 路徑上新增 View 更新都要比照。
- `GuideStatus` 的 Idle 規則用「樣本的類型有沒有自動產生的元件」，不能用 `lastSearch()`（回 Idle 時被清掉）；已知的限制：一個樣本一個都沒找到，接受後仍指著〔搜尋〕。
- 預覽卡片沒有加「下一步」（訊息列 96 px）；真實板子的影片沒做（之後用外部播放器開）。
- 示範畫面在背景解碼與縮放（`QtConcurrent`，presentation 多用 Qt Concurrent）：第一版在 UI 執行緒同步做，I6 變慢、Debug 的 `tst_presenter` 逾時；UI 執行緒不要等任何解碼。**測試台 `PresenterRig` 預設把 dock 關著**（`showGuidance()` 才開）：Debug 的 `tst_presenter` 從 231 秒變 297 秒。
- **Debug 的 `tst_presenter::aSlowSearchShowsBusyThenProgressAndEscCancelsToIdleAtOnce` 是不穩定的測試**：2026-10-15 晚上機器慢時，本分支與 `main` eb95b79 都失敗（`main` 整組 231 秒）；隔天正常時通過（85 秒）。看到它失敗先在 `main` 上對照，不要當成新的修改造成的（這次繞了一大圈，經過在 changelog）。
- 改 `.pro` 時 JS `String.replace` 的 `$$` 會變成 `$`（這次踩到）；用 `split/join`。
