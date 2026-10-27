# R05 變更紀錄

> 只新增，新紀錄放最上方；格式見 `docs/plans/_template/changelog.md`。
> 3.0.0 之前（R05.1、R05.2）的紀錄在 [`docs/legacy/changelog-ai.md`](../../../legacy/changelog-ai.md) 的 2026-10-27 各筆，沒有搬進來；結果與數字在 ADR-031 的「後續」。

## 2026-10-27 — Claude Code (Claude Opus 5.5)

### 細項
- 從 2.x 的需求進度表遷移成計畫（AI 結構 3.0.0，ADR-032）：舊的 `docs/plan/R05-rust-domain.md` 以 `git mv` 搬成本資料夾的 `plan.md`，加上 frontmatter 與細項表（R05.1–R05.5 照抄舊表的狀態、分支與驗證），Start prompt 改讀本資料夾。

### 修改的檔案
- `docs/plans/active/R05-rust-domain/`

### 驗證
- 未建置（文件遷移）；結構以 `scripts/ai/check-ai-structure.ps1 -Strict` 檢查。

### Commit
- 見計畫 R06。
