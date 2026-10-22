# AI Agent 專案入口（Codex 與 Claude Code 共用）

本檔是入口（bootloader），不是知識庫。專案事實、工作流程與規則都在下列文件；**不要在本檔複製內容**，要改規則就改那份文件。

## 不可省略的四條

1. 回覆一律使用繁體中文（zh-TW）：目前的 session、由其他 session 開出的子 session、委派的 subagent 都一樣；程式識別字、命令、錯誤訊息與既有專有名詞保留原文。任務完成時要總結修改內容：改了哪些檔案與行為、實際驗證的結果、未驗證或未完成的事項。
2. 不虛構驗證、不隱藏失敗；沒有實際執行成功的檢查，不得回報 `VERIFIED`。
3. 不擅自 commit／push（`project-rules.md` 明確授權的流程除外）；stage 時明確列出路徑，禁止 `git add .` 與 `git add -A`。
4. 非單純需求在實作前，先在 `docs/current-status.md` 的「需求進度表」把需求展開成細項（狀態 ⬜）；完成或變動時逐項更新狀態、分支／commit 與驗證。規則見 `engineering-workflow.md`「需求展開與進度表」。

## 開始非單純任務前，依序讀取

1. [`AI_CONTEXT.md`](AI_CONTEXT.md) — 專案事實：用途、技術棧、目錄、建置與測試指令、限制
2. [`.agents/harness/engineering-workflow.md`](.agents/harness/engineering-workflow.md) — 共用工作流程：證據、驗證、回報、交接、需求進度表
3. [`.agents/harness/project-rules.md`](.agents/harness/project-rules.md) — 程式碼風格、SOLID、四種輸出、Step Protocol、Git、文件規則
4. [`.ai/handoff.md`](.ai/handoff.md) — 上一位 agent 留下的交接單
5. [`docs/current-status.md`](docs/current-status.md) — 需求進度表（開頭）、目前進度與已知問題
6. [`docs/architecture.md`](docs/architecture.md) — 分層與依賴規則（本專案加入：寫任何程式前都要遵守）

做設計決策前，另讀 `docs/decisions.md`（索引，內容在 `docs/adr/`）；接續跨 session 的需求時，讀進度表「計畫」欄連到的 `docs/plan/` 檔。純問答、解釋、狀態詢問不必走完整流程，但仍遵守上面四條。

## 收工前（有意義的變更之後）

依 `ai-handoff` skill 執行：更新 `docs/current-status.md` 的需求進度表、覆寫 `.ai/handoff.md`、在 `docs/changelog-ai.md` 最上方新增一筆，有設計決策就新增 `docs/adr/NNN-slug.md` 並在 `docs/decisions.md` 加索引。改過入口檔或 skill 後，執行 `scripts/ai/check-ai-structure.ps1`。最後的回覆要總結修改內容（第 1 條）。

## 平台備註

- **Codex**：不支援在 AGENTS.md 內匯入檔案，所以上面是「必讀清單」。Skill 的來源在 `.agents/skills/<name>/SKILL.md`。
- **Claude Code**：入口是 `CLAUDE.md`（它匯入本檔）。Skill 在 `.claude/skills/`，是 `.agents/skills/` 的鏡像複本，由 `scripts/ai/sync-skills.ps1` 產生，不要手改。模型路由（`MODEL_ROUTING.md`、`.claude/agents/`）是 Claude 專用，Codex 不讀。
- 平台專屬規則只寫在對應入口：Codex 專屬放下一節，Claude 專屬放 `CLAUDE.md`。共通內容不得兩邊各寫一份。

## Codex 專屬（Claude 可忽略）

- 目前沒有。需要時加在這裡，例如 sandbox 或 approval 的使用習慣。
