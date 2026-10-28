# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史，保持在 60 行內。流程見 `ai-handoff` skill。
> 本檔只寫「上一個 session 留下了什麼」。計畫的進度與接續資訊在各計畫的 `status.md`（入口 `docs/plans/INDEX.md`），這裡不重抄；2.x 的紀錄在 `docs/legacy/`。

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-28
- 分支：`refactor/r05-5-finish` 以 `--no-ff` 合併到 `main`，tag `r05.5-done`，只在本機，沒有 push。

## 上一個 session 動到的計畫

- R05.5（收尾：`AOI_DOMAIN_IMPL` 預設改 `rust`）：R05 計畫完成，搬到 [`docs/plans/completed/R05-rust-domain/`](../docs/plans/completed/R05-rust-domain/status.md)。結果與數字在 ADR-031 的「後續（2026-10-28，R05.5 收尾）」。
- 效能基準線 `perf/baseline/casparhsu1-harness.json` 用 Rust 的預設建置重建（三次最安靜的量測），預算不動。

## 接下來

- 沒有進行中的計畫步驟。待使用者決定的事（第二階段、兩項真實資料）在 `docs/current-status.md` 與 R05 的 `status.md`。
- `docs/plans/INDEX.md` 裡還在規劃中的是 S12（匯出、打包、最終文件）。

## 不要動

- `docs/legacy/` 只讀；新的紀錄只寫進各計畫的資料夾。
- 範本管理的檔案不要在專案裡改（`engineering-workflow.md`、`ai-handoff` skill、`check-ai-structure.ps1`、`sync-skills.ps1`、`migrate-ai-structure-3.mjs`、`tools/ste-lint.mjs`、`docs/plans/_template/`、`docs/adr/_template.md`）。
- 不要在 `AiInspTool/` 原地執行 qmake；2026-10-27 的原地建置（使用者同意）搬到了 `build/r05-5-insource-backup/`（清單 `moved-list.txt`），確定不要可以刪。
- 建置資料夾不要改名（`tst_cli` 把執行檔的路徑編進物件檔），要換命名就刪掉重建。

## 未提交的變更

- 無（本次已提交）。`.archify/` 不是本 session 建的，沒有動。
- `build/r05-5/`（矩陣與效能的記錄、交替量測 `ab/`）不入版控；建置資料夾已照新命名：`build/qmake-<組態>` 是 Rust、`-cpp` 是 C++。

## 假設與未驗證項目

- `perf_match_region_real`（影像 2026-10-20 換過，預設的點擊與主體框對不上）與 `tst_repeatability::realFiveRuns`（原始五份 CSV 找不到）跳過，未驗證。
- Qt Creator 的 GUI 沒有操作：Qt Creator 沒給選項時是 Rust，要讀得到 cargo（重新開 Qt Creator 讀新的 PATH），否則 qmake 報錯。
- 這台機器量效能時常有其他建置（Visual Studio）與程式在跑，雜訊很大；比較要交替量，不要看單次。
