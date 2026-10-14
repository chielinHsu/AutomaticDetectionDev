# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Fable 5.1)
- 日期：2026-10-14

## 正在做

無。**CAD 樣本建立的四個問題（[ADR-025](../docs/adr/025-sample-creation-separate-from-search.md)）完成**：分支 `feat/cad-sample-editing`（兩個 commit：程式、文件），`--no-ff` 合併到 `main` 由使用者在主 checkout 執行（這個 session 在 worktree，`main` 在主 checkout 上簽出，不能在這裡合併），沒有 tag、沒有 push。**接下來仍是 S12**（Start prompt 在 `docs/plan/S12-release.md` 底部），或使用者指定的 P2（完整流程 B 與樣本庫 UI，另開 session）。

## 這次做了什麼（細節在 `docs/changelog-ai.md` 最上面那一筆、ADR-025、`docs/current-status.md` 的「樣本的建立與搜尋分開」）

- 使用者的決定：樣本建立是建立、搜尋是搜尋。`S` 放開只建立樣本（每個樣本自己一個類型、一個 Command），〔搜尋〕（`F5`）用板子上全部樣本各搜一次、結果依位置合併（同位置留分數最高的），接受時上一次自動產生的元件整批換掉（樣本與手動補的留著）；改大小＝改類型的主體框（同類型一起，只在 Idle）；框選、`Ctrl+A`、`Ctrl+Shift+A`；`B` 在所選元件上畫子框（存成 Pin 1×1，同類型一起貼上、預覽就看得到）。P2 另開 session，R 工具維持現狀。
- 新檔：`application/usecases/BoardProposal.*`（合併規則）、`SearchBoardWithSamples.*`；狀態機多 `ReleaseSample`、`SearchRequested`；`InspectionDocument` 多 `addSample`、`setBodySize`、`addBoxGroup`、`samples()`；工具多 `BoxTool`、`SelectTool` 的把手與框選；主視窗多〔搜尋〕〔全選〕〔選取同類型〕與 `B`。

## 建置與測試

- `test.ps1` 預設 Release：**57／57**（HALCON 0 跳過；跳過與 XFAIL 的清單和改之前相同）。
- `test.ps1 -Config Debug`：**57／57**。
- `-Perf` 與基準線**沒有跑**：T5 的定義改成「按〔搜尋〕→ 提案就緒」（`perf_e2e`、`perf_ui_s04` 已改成樣本建一次、每回合按 F5），下一個動效能的 session 先 `perf.ps1 -Runs 3 -WriteBaseline`。

## 不要動

- `mergeSampleProposals` 的四條規則與順序（ADR-025 §2）；`InspectionDocument::accept` 的「整批換掉」語意；樣本元件的 `state = Sample`、`origin = Manual`。
- 預覽時不顯示把手、不能改大小（ADR-002 規則 5）。
- `tst_presenter` 的 `twoSamplesOnTheSameGridAreMergedAndAcceptReplacesTheLastResult`：第二個樣本放在第一個右邊一格（假世界的格距 1.5 × 樣本大小），兩個樣本看到同一片網格。
- 上一次交接的清單照舊：`RoiLayerItem` 的字形路徑、`DocumentSceneCache` 的鍵（改板子內容一律做成 Command）、`HalconSearchOptions`／`OpenCvSearchOptions` 的預設、`ContractTolerances.h`、opencv B1 的 `QEXPECT_FAIL`、`tokens.json`、`copy.json` 只新增 key、`budgets.json` 的 limit、`ai-project-template/`、`third_party/halcon/`、fixture JSON、`PresenterRig.h` 的視窗閘門、晶格的門檻與 ADR-013、port 契約、`perf/baseline/*.json` 只用 `-WriteBaseline` 重建。

## 已知問題與風險

- 子框目前只能用 `Ctrl+Z` 拿掉；Pin 矩陣的列數欄數沒有 UI（同一個群組改 `rows`／`cols` 就有）；只改這一個元件（實例覆寫）延後。
- 兩個 back end 的分數不是同一把尺（ADR-004）：合併時「分數最高者」在同一個 back end 內比較才有意義；一次搜尋只用一個 back end，所以目前沒有問題。
- 樣本框到不只一個元件時 back end 仍會回 `SampleCoversMany`（區域樣本是刻意的用法時，使用者可以忽略那個提示）；真實 back end 上區域樣本的效果沒有量。
- 等使用者決定的事沒有變（ADR-004、S09、S10、待決定（10））。

## 驗證（實際執行）

- `test.ps1` 預設 Release：57／57 通過；`node tools/ste-lint.mjs`：979 sentences、0 errors；`node tools/check-deps.mjs`：216 files、0 violations。
- `test.ps1 -Filter tst_tools`（含三個新測試）：1／1，Totals 38 passed。
- `test.ps1 -Config Debug`：**57／57**（log 2 362 項 passed、0 failed；跳過與 XFAIL 的清單和 Release 相同）。
