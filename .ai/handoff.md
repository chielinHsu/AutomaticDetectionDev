# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史，保持在 60 行內。流程見 `ai-handoff` skill。
> 本檔只寫「上一個 session 留下了什麼」。計畫的進度與接續資訊在各計畫的 `status.md`（入口 `docs/plans/INDEX.md`），這裡不重抄；2.x 的紀錄在 `docs/legacy/`。

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-27
- 分支：`docs/ai-structure-v3`（含 R05.2 的 `main`）以 `--no-ff` 合併到 `main`，只在本機，沒有 push。

## 上一個 session 動到的計畫

- R06（AI 結構 3.0.0，已完成）：[`docs/plans/completed/R06-ai-structure-v3/`](../docs/plans/completed/R06-ai-structure-v3/plan.md)。決策 ADR-032。範本 `ai-project-template/` 一起升到 3.0.0。
- R05、S12 從 2.x 的需求進度表遷移成計畫，舊計畫檔以 `git mv` 搬成各自的 `plan.md`；內容沒有變。
- R05.2（晶格搬進 Rust）是前一個 session 做的，已合併（tag `r05.2-done`）。

## 接下來

- R05.3 比對區與特徵擷取：[`docs/plans/active/R05-rust-domain/`](../docs/plans/active/R05-rust-domain/status.md)，Start prompt 在它的 `plan.md` 底部。

## 不要動

- `docs/legacy/` 只讀；新的紀錄只寫進各計畫的資料夾。
- 範本管理的檔案不要在專案裡改（`engineering-workflow.md`、`ai-handoff` skill、`check-ai-structure.ps1`、`sync-skills.ps1`、`migrate-ai-structure-3.mjs`、`tools/ste-lint.mjs`、`docs/plans/_template/`、`docs/adr/_template.md`）；`docs/steps/_template/` 是本專案自己維護的（`.agents/harness/template-overrides.txt`）。
- `ai-project-template/` 是所有專案共用的範本：改它要遞增 `TEMPLATE_VERSION` 並照它的 README 更新各專案。
- 不要在 `AiInspTool/` 原地執行 qmake（見 `docs/current-status.md`）。
- R05 的界線：不改 application、presentation、OpenCV、HALCON。

## 未提交的變更

- 無（本次已提交）。

## 假設與未驗證項目

- Codex 與 Claude Code 實際載入新入口沒有實測；本次沒有建置（只改文件與 AI 結構的腳本）。
