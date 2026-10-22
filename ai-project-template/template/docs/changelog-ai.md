# AI 工作紀錄

> 只新增，新紀錄放最上方。每個 agent session 一筆，讓下一位 agent 知道上一位做過什麼、為什麼。
> agent 欄位寫實際的工具與模型；不要從舊紀錄抄。

## 格式

```markdown
## YYYY-MM-DD — Claude Code (模型) ／ Codex (模型)

### 完成
- 做了什麼（一行一件；對應需求進度表的 Rnn）

### 修改的檔案
- path/to/file

### 驗證
- 實際執行的指令與結果；沒執行就寫「未驗證」與原因

### 未完成
- 還沒做完的事

### 注意事項
- 下一位 agent 需要知道的限制、否決的做法、不要還原的修改
```

## {{DATE}} — 範本

### 完成
- 建立 AI agent 共用結構 2.0.0（Codex 與 Claude Code）：入口、工作流程、專案規則、需求進度表、ADR、總體計畫與計畫檔範本、模型路由、檢查腳本（R01）。

### 修改的檔案
- `AGENTS.md`、`CLAUDE.md`、`AI_CONTEXT.md`、`MODEL_ROUTING.md`、`.agents/`、`.claude/`、`.ai/`、`docs/`、`scripts/ai/`、`tools/ste-lint.mjs`

### 驗證
- 未驗證（範本建立時尚無程式碼）。

### 未完成
- 填寫 `TODO(project)`（進度表 R02）。

### 注意事項
- 無。
