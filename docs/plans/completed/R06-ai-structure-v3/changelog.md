# R06 變更紀錄

> 只新增，新紀錄放最上方；格式見 `docs/plans/_template/changelog.md`。

## 2026-10-27 — Claude Code (Claude Opus 5.5)

### 細項
- R06.1–R06.5（見 `plan.md`）。遷移其他專案時發現兩個範本缺陷並修正：專案自訂過的範本管理檔會被 `-Refresh` 蓋掉（R06.2）；遷移腳本會把連到 `docs/current-status.md` 的連結轉到封存檔（改成不轉址）。

### 修改的檔案
- `ai-project-template/`（範本與兩支腳本）；本專案：`AGENTS.md`、`CLAUDE.md`、`AI_CONTEXT.md`、`MODEL_ROUTING.md`、`README.md`、`.agents/harness/project-rules.md`、`.agents/harness/template-overrides.txt`、範本管理檔、`.claude/agents/architect*.md`、`docs/00-master-plan.md`、`docs/decisions.md`、`docs/adr/032-ai-structure-v3.md`、`docs/current-status.md`、`docs/legacy/`、`docs/plans/`、`.ai/handoff.md`，以及舊 ADR 與步驟文件裡指向舊計畫檔的連結。

### 驗證
- 見 commit 訊息。

### Commit
- 分支 `docs/ai-structure-v3`（hash 見 `git log`）。

### 注意事項
- 不要從 `docs/legacy/` 再往回搬紀錄；新的紀錄只寫進各計畫。
