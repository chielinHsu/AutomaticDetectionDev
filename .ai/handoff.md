# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Fable 5.1 → Claude Opus 5.5)
- 日期：2026-10-16

## 正在做

無。**操作步驟的提示（[ADR-027](../docs/adr/027-step-guidance.md)）完成**：分支 `feat/step-guidance`（從 `main` eb95b79 開；三個 commit：application、presentation、文件；之後併入 `main` 48f7ef4 ＝ 晶格位置吸附〔ADR-026〕的合併，解掉兩個測試檔與文件的衝突），`--no-ff` 合併到 `main`（主 checkout `D:\AOI_Project\Qt\AutoCadCreated`，`git -C`），沒有 tag、沒有 push。**兩個 2026-10-15 的需求（ADR-026、ADR-027）都已在 `main`。接下來仍是 S12**（Start prompt 在 `docs/plan/S12-release.md` 底部），或使用者指定的 P2。**使用者指示：效能基準線（`perf.ps1 -Runs 3 -WriteBaseline`）等他說了再做**（他會先關掉 VS 與 Chrome）；重建時要看 T5、U3（ADR-026 多了 use case 的第 6 階段）。

## 這次做了什麼（細節在 `docs/changelog-ai.md` 最上面兩筆、ADR-027、`docs/current-status.md` 的「操作步驟的提示」）

- 使用者要求步驟提示與影片提示；確認現況、比較三個方案與四種影片做法，使用者選了訊息列補強＋操作步驟 dock＋程式產生的 PNG 逐格動畫＋F1。
- application：`interaction/StepGuide`（`guideStatus(const InspectionSession&)`：目前這一步、做完的、進行中、要重做）。presentation：左側「操作步驟」dock（`guidance/GuidancePanel`）、`GuideDemo.h`（示範的張數與檔名）、`AutoRoiPresenter::publishGuidance`（狀態變了才呼叫 View）、`Searching` 的提示、空白卡片的三步、說明選單 `F1`；app：記住 dock 的開關（`guidance/visible`）。
- 示範畫面：`tests/ui/record_guidance`（不是測試，`tests.pro` 的 `AOI_TOOLS`）產生 `docs/ux/guidance/*.png`（13 張、400 px），`aoi_ux.qrc` 內嵌。**UI 改了要重跑再重新建置**（張數改了三邊一起改，`tst_messages` 會擋）。
- `copy.json` 新增 36 個 key；presentation 多用 Qt Concurrent（示範畫面在背景解碼）。
- ADR-026（另一個 session）：`Candidate::latticePose`、`PositionMode`（吸附到陣列／匹配位置）、參數面板「位置」、選取資訊的「位置」列；細節見它的 changelog 那一筆。

## 建置與測試

- 合併 `main` 48f7ef4 之後（最終的程式）：Release **58／58**、Debug **58／58**（2026-10-16；HALCON 0 跳過；跳過的 5 個測試與 2 個 XFAIL 和改之前相同；`tst_presenter` 85＋1 skipped、`tst_messages` 18、`tst_cli` 17）；示範畫面在合併之後重新產生（參數面板多了「位置」一列），重建後 UI 的測試再跑一次通過。
- 合併之前（以 eb95b79 為底，2026-10-16）：Release **58／58**、Debug **58／58**（HALCON 0 跳過；跳過與 XFAIL 的清單和改之前相同）；`ste-lint` 0 errors、`check-deps` 0 violations。
- `perf_ui_s04`（Release，和 `main` eb95b79 前後交替各兩次）：U3 p95 前 29.4、13.9／後 10.1、15.2，I6 前 3.6、4.9／後 2.8、3.1，全部在預算內、差異在雜訊內。

## 不要動

- `publishGuidance` 只在 `GuideStatus` 變了才碰 dock（U3 的保護；`tst_presenter` 數 `updateCount`）。`StepGuide` 的 Idle 規則用「樣本的類型有沒有自動產生的元件」，不能用 `lastSearch()`（回 Idle 時被清掉）。
- 示範畫面的解碼與平滑縮放都在背景（UI 執行緒不等）；第一版同步做時 I6 變慢、Debug 的 `tst_presenter` 逾時。`PresenterRig` 預設把 dock 關著（`showGuidance()` 才開）。
- ADR-026：`Candidate::pose` 留匹配位置；吸附只在 `classify`／`ProposalSession`；吸附門檻 ＝ 晶格的內點容差；位置模式不進堆疊；樣本不自動移動。
- 之前的清單照舊：`SampleListCache`／`DocumentSceneCache` 的鍵、表單「Enter 或離開欄位才生效」、`ResizeTypes`、`mergeSampleProposals`、`accept` 整批換掉、`RoiLayerItem` 字形路徑、兩個 back end 的搜尋選項預設、`ContractTolerances.h`、opencv B1 的 `QEXPECT_FAIL`、`tokens.json`、`copy.json` 只新增 key、`budgets.json` 的 limit、`ai-project-template/`、`third_party/halcon/`、fixture JSON、`PresenterRig.h` 的視窗閘門、ADR-013、port 契約、`perf/baseline/*.json` 只用 `-WriteBaseline` 重建。

## 已知問題與風險

- **對時間敏感的測試**：Debug 的 `tst_presenter::aSlowSearchShowsBusyThenProgressAndEscCancelsToIdleAtOnce`（機器滿載時忙碌與進度兩個計時器同一輪觸發，只看到 Progress；2026-10-15 晚上在 `main` eb95b79 上也失敗）、`tst_cli` 的 HALCON 兩項（等 stderr）。看到它們失敗先在 `main` 上對照或在安靜時重跑，不要當成新修改造成的（這次繞了一大圈，經過在 changelog）。
- 預覽卡片沒有「下一步」（訊息列 96 px）；真實板子的示範影片沒做；示範畫面「過期」沒有測試抓得到。
- ADR-026：樣本自己的偏差沒有顯示、位置模式不存檔、跨樣本不對齊；`IParamsSink` 6 個方法。
- 改 `.pro` 時 JS `String.replace` 的 `$$` 會變成 `$`；主 checkout 第一次建 Debug 時 jom 偶爾在 `.moc` 之前編 `.obj`（先跑各測試 Makefile 的 `mocables`）。
- 等使用者決定的事沒有變（ADR-004、ADR-009、S09、S10）。
