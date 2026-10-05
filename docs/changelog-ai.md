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

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)

### 完成
- 把前三個 commit 的訊息改寫成使用者的 `[作者]屬性:` 格式（檔案內容、作者、時間不變；新編號 `fd4fd2a`、`13230f8`、`74272a2`）。改寫前的完整歷史留在分支 `backup/before-commit-reformat`。
- `project-rules.md` 的 Commit 規則由 Conventional Commits 改為該格式，並訂為唯一權威；Step Protocol 第 5 步同步調整。訊息一律寫成檔案再 `git commit -F`，`AI_CONTEXT.md` 的 shell 備註同步修改。
- ADR-000 補上這次格式變更的說明。
- 依使用者要求以 `--no-ff` 合併到 `main`，沒有 push。

### 修改的檔案
- `.agents/harness/project-rules.md`、`AI_CONTEXT.md`、`docs/adr/000-ai-agent-structure.md`、`docs/changelog-ai.md`、`.ai/handoff.md`、`docs/current-status.md`
- 範本 `ai-project-template/`（未追蹤，不在 commit 內）：`project-rules.md` 改成同一份預設，README 更正「commit 格式屬 AOI 專屬」的說法。

### 驗證
- `scripts/ai/check-ai-structure.ps1 -Strict`：通過，0 警告。
- `node tools/ste-lint.mjs`：206 句，0 錯誤。
- 用修改後的範本重新建立範例專案：建置腳本結束碼 0，結構檢查通過，新的 Commit 訊息一節已帶入。
- 新 commit 訊息以腳本逐項檢查格式（標題、`測試 :`、Co-Authored-By、結尾兩行、每行寬度）。

### 未完成
- 12 份 `docs/plan/S*.md` 的「Commit 範本」仍是英文 Conventional 風格，沒有改寫；`project-rules.md` 規定它們只當內容參考，格式以規則為準。
- 確認改寫後的歷史沒問題後，刪除備份分支 `backup/before-commit-reformat`。

### 注意事項
- 結尾的 `[Test]` 與 `Build OK, Test OK` 是使用者的固定規則；專案目前沒有建置，所以它不代表本次有建置。實際驗證只看 `測試 :` 區塊，且必須如實。
- 兩份規範衝突時（repo 內文件與使用者慣例），先問使用者，不要自己挑。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)

### 完成
- 把 AI 共用結構（Codex 與 Claude Code）套用到本專案，並與既有的 `CLAUDE.md` 合併：原內容搬到 `AI_CONTEXT.md`、`docs/architecture.md`、`.agents/harness/project-rules.md`，`CLAUDE.md` 改為薄入口。
- `docs/STATUS.md` 更名為 `docs/current-status.md`；14 個檔案（12 份 `docs/plan/S*.md`、`00-master-plan.md`、`S00-overview/spec.md`）的舊引用改成新路徑。
- 新增 `docs/adr/000-ai-agent-structure.md`；`docs/decisions.md` 作為 ADR 索引。
- `.gitignore` 補上 `.claude/worktrees/`、`CLAUDE.local.md`。

### 修改的檔案
- 新增：`AGENTS.md`、`AI_CONTEXT.md`、`.agents/`、`.claude/`、`.ai/handoff.md`、`scripts/ai/`、`docs/architecture.md`、`docs/decisions.md`、`docs/changelog-ai.md`、`docs/adr/000-ai-agent-structure.md`
- 改寫：`CLAUDE.md`、`.gitignore`
- 更名：`docs/STATUS.md` → `docs/current-status.md`
- 引用更新：`docs/plan/S01`–`S12`、`docs/00-master-plan.md`、`docs/steps/S00-overview/spec.md`

### 驗證
- `scripts/ai/check-ai-structure.ps1 -Strict`：通過，0 警告（Windows PowerShell 5.1）。
- `node tools/ste-lint.mjs`：206 句，0 錯誤，0 警告。
- 舊名稱（`STATUS.md`、`CLAUDE.md 的…`）區分大小寫掃描：除了刻意保留的歷史說明，沒有殘留。
- Claude Code 實測：`AI_CONTEXT.md`、`project-rules.md`（含 Step Protocol）、`AGENTS.md`、`architecture.md` 都經 `CLAUDE.md` 的 `@` 匯入載入。
- 未驗證：Codex 端。這台的 Codex CLI 0.136.0 無法執行 `codex exec`（解析模型清單失敗），所以 `AGENTS.md` 的必讀清單流程沒有實測。

### 未完成
- 變更已 commit（`fd4fd2a`），並依使用者要求以 `--no-ff` 合併到 `main`。沒有 push，是否推到遠端由使用者決定。
- `ai-project-template/` 仍在 repo 內且未追蹤，不在這次 commit，要放哪裡由使用者決定。

### 注意事項
- 分層規則原本每個 Claude session 都載入，所以 `CLAUDE.md` 額外匯入 `docs/architecture.md`（`AGENTS.md` 也列入必讀）。不要移除，否則護欄會變弱。
- 舊版 `CLAUDE.md` 在 git 歷史（commit `28414b1`）。
- `.claude/settings.json` 的 deny 規則在 Claude Code 信任此資料夾後才生效。
