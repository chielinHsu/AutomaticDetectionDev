# ADR-032 AI 結構 3.0.0：每個計畫一個資料夾

- 日期：2026-10-27
- 決定者：使用者（要求依 ChatGPT「架構分類建議」<https://chatgpt.com/share/6ae038fd-38b8-83ec-a2f0-10803a8a52a3> 調整，各計畫的紀錄分開；檔名與編號沿用現有慣例、未完成的需求拆成計畫、已完成的封存）；Claude Code (Claude Opus 5.5)（範本 3.0.0 的做法）
- 狀態：採用；取代 [ADR-030](030-ai-structure-2-0-0.md) 裡「需求進度表與 `docs/changelog-ai.md`」的部分，其餘照舊

## 背景

2.0.0 把所有需求的進度放在 `docs/current-status.md` 開頭的一張表、所有 session 的紀錄放在 `docs/changelog-ai.md`。本專案的需求一多（S00–S12、R01–R05 與多個修正），`current-status.md` 有數百行、各步驟的「產出與發現」也寫在裡面，changelog 更長；不同需求的紀錄混在一起，難維護，也難找某個需求的歷史。使用者 2026-10-27 要求：一個計畫拆成細項時記錄在自己的地方，另一個計畫的紀錄要分開。

本專案的 `ai-project-template/` 是所有專案共用的範本來源，所以範本本身也在這次升到 3.0.0（同一分支 `docs/ai-structure-v3`）。

## 決定

- 範本 3.0.0（`ai-project-template/README.md`「計畫分開管理」）：`docs/plans/INDEX.md` 是計畫索引；每個計畫一個資料夾 `docs/plans/<active|completed|cancelled>/<ID>-<slug>/`，`plan.md` 的細項表是進度的單一事實來源，`status.md` 是現況快照，`changelog.md` 只記這個計畫，選用的 `tasks/`。`docs/current-status.md` 只放專案層級的快照。
- 本專案的遷移（計畫 R06）：舊的 `current-status.md`、`changelog-ai.md`、`docs/plan/`（S01–S11 的計畫檔）原樣封存在 `docs/legacy/`（只讀，連結已改寫）；未完成的 S12 與 R05 的舊計畫檔以 `git mv` 搬成 `docs/plans/active/` 的 `plan.md`，加上 frontmatter 與細項表；已完成的步驟與修正不建資料夾。
- 本專案加強過的範本管理檔（`docs/steps/_template/` 的 AOI 專屬量測欄位與 `perf-test-plan.md` 連結）登記在 `.agents/harness/template-overrides.txt`，`-Refresh` 不覆蓋。

## 否決的替代方案

- 把已完成的 S00–S11、R01–R04 也拆成資料夾：沒有新資訊，只增加工作量；舊的進度表與「產出與發現」整份封存，連結保持可用。
- 檔名與編號照搬 ChatGPT 的寫法（`PLAN-001`、`CURRENT_STATUS.md`、`.ai/WORKFLOW.md`）：會讓既有的 `Sxx`／`Rnn`、tag（`step-NN-done`、`r05.N-done`）與 commit 訊息對不上。
- 每個計畫另放 `DECISIONS.md`：跨計畫的決策本來就在 `docs/adr/`，計畫的 `plan.md` 有「決策」一節連過去就夠。

## 後果

- 開工時先在 `INDEX.md` 找到計畫，只讀那個計畫的 `plan.md` 與 `status.md`；收工時只更新那個計畫（`ai-handoff` skill）。R05.3 起的 Start prompt 讀 `docs/plans/active/R05-rust-domain/`。
- `check-ai-structure.ps1` 擋下索引與資料夾不一致、搬動後斷掉的連結。
- 程式碼註解裡還有兩處舊路徑（`tests/infra/perf_storage.cpp`、`tests/perf/HarnessSupport.h` 提到 `docs/plan/S06…`、`docs/plan/S10`）：只是註解，這次不改程式碼，檔案現在在 `docs/legacy/plan/`。
