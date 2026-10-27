# R01 變更紀錄

> 只新增，新紀錄放最上方；格式見 `docs/plans/_template/changelog.md`。

## {{DATE}} — 範本

### 細項
- R01.1：建立 AI agent 共用結構 3.0.0（Codex 與 Claude Code）：入口、工作流程、專案規則、計畫分開管理（`docs/plans/`）、ADR、總體計畫、模型路由、檢查腳本。

### 修改的檔案
- `AGENTS.md`、`CLAUDE.md`、`AI_CONTEXT.md`、`MODEL_ROUTING.md`、`.agents/`、`.claude/`、`.ai/`、`docs/`、`scripts/ai/`、`tools/ste-lint.mjs`

### 驗證
- 未驗證（範本建立時尚無程式碼）；結構以 `scripts/ai/check-ai-structure.ps1` 檢查。

### Commit
- 未提交（由使用者決定何時提交）。
