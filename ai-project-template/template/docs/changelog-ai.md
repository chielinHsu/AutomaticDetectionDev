# AI 工作紀錄

> 只新增，新紀錄放最上方。每個 agent session 一筆，讓下一位 agent 知道上一位做過什麼、為什麼。
> agent 欄位寫實際的工具與模型；不要從舊紀錄抄。

## 格式

```markdown
## YYYY-MM-DD — Claude Code (模型) ／ Codex (模型)

### 完成
- 做了什麼（一行一件）

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
- 建立 AI agent 共用結構（Codex 與 Claude Code）。

### 修改的檔案
- `AGENTS.md`、`CLAUDE.md`、`AI_CONTEXT.md`、`.agents/`、`.claude/settings.json`、`.ai/`、`docs/`、`scripts/ai/`

### 驗證
- 未驗證（範本建立時尚無程式碼）。

### 未完成
- 填寫 `AI_CONTEXT.md` 與 `project-rules.md` 的 `TODO(project)`。

### 注意事項
- 無。
