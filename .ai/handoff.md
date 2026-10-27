# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史，保持在 60 行內。流程見 `ai-handoff` skill。
> 本檔只寫「上一個 session 留下了什麼」。計畫的進度與接續資訊在各計畫的 `status.md`（入口 `docs/plans/INDEX.md`），這裡不重抄；2.x 的紀錄在 `docs/legacy/`。

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-27
- 分支：`refactor/r05-3-match-region` 以 `--no-ff` 合併到 `main`，tag `r05.3-done`，只在本機，沒有 push。

## 上一個 session 動到的計畫

- R05.3（比對區與自動擷取搬進 Rust）：[`docs/plans/active/R05-rust-domain/`](../docs/plans/active/R05-rust-domain/status.md)。設計與數字在 ADR-031 的「後續（R05.3）」。
- `tools/perf-compare.mjs` 多一個來源 `match_region`（新的效能量測 `tests/domain/perf_match_region`）。

## 接下來

- R05.4 重複性統計：同一個計畫資料夾，Start prompt 在它的 `plan.md` 底部（共通格式）。

## 不要動

- `docs/legacy/` 只讀；新的紀錄只寫進各計畫的資料夾。
- 範本管理的檔案不要在專案裡改（`engineering-workflow.md`、`ai-handoff` skill、`check-ai-structure.ps1`、`sync-skills.ps1`、`migrate-ai-structure-3.mjs`、`tools/ste-lint.mjs`、`docs/plans/_template/`、`docs/adr/_template.md`）。
- 不要在 `AiInspTool/` 原地執行 qmake；那裡 2026-10-27 13:00 的原地建置（`.moc`、`.obj`）沒有動，可能是使用者的。
- R05 的界線：不改 application、presentation、OpenCV、HALCON。

## 未提交的變更

- 無（本次已提交）。`build/r05-3/` 底下是本次的建置記錄與效能量測（不入版控）；`build/qmake-release*/bin/perf_match_region*_before.exe` 是搬之前的執行檔備份，可以刪。

## 假設與未驗證項目

- `perf_match_region_real` 的影像 2026-10-20 換過，預設參數對不上，召回與位置沒有量到（`status.md`「待使用者決定」）。
- 沒有跑 `-NoOpenCv`、`-NoHalcon`、Profile；Qt Creator 的 GUI 沒有操作。
