# ADR-000 AI agent 共用結構（Codex 與 Claude Code）

- 日期：{{DATE}}
- 決定者：範本（使用者要求整套照搬 AutoCadCreated 的 AI 結構）
- 狀態：採用

## 背景

專案會在 Claude Code 與 Codex 之間切換，兩者看不到彼此的對話。兩邊各維護完整規則會漂移；沒有共同的進度與決策紀錄，換 agent 就得從頭問起。

## 決定

套用 `ai-project-template` 的結構（版本見 `.agents/harness/TEMPLATE_VERSION`）：

- `AGENTS.md` 是平台中立的入口，只放「不可省略的四條」、必讀清單與平台備註；`CLAUDE.md` 用 `@AGENTS.md` 匯入，並只放 Claude 專屬規則。
- 專案事實放 `AI_CONTEXT.md`；通用流程放 `.agents/harness/engineering-workflow.md`（範本管理）；專案慣例放 `.agents/harness/project-rules.md`（SOLID、四種輸出、Step Protocol、Git、Commit 訊息、文件規則）。
- 交接用 `.ai/handoff.md`（覆寫）與 `docs/changelog-ai.md`（只新增）；進度用 `docs/current-status.md` 開頭的「需求進度表」，跨 session 的需求另有 `docs/plan/<ID>-<slug>.md`。
- 設計決策一檔一檔放 `docs/adr/NNN-slug.md`，`docs/decisions.md` 只是索引。
- 模型路由（Claude 專用）：`MODEL_ROUTING.md`、`.claude/agents/` 五個 subagent、`.claude/hooks/model_router.mjs`、`scripts/ai/check-model-routing.mjs`。
- 檢查：`scripts/ai/check-ai-structure.ps1`（必要檔案、入口規則、進度表、ADR 索引、skill 鏡像、模型路由）；`-Strict` 把未填的 `TODO(project)` 也算失敗。

## 否決的替代方案

- 兩個入口各抄一份完整規則：會漂移。
- 把決策內嵌在 `decisions.md`：檔案越來越長，無法單獨引用某一筆；改為 ADR 一檔一決策加索引。
- 進度只寫在對話或 commit 訊息：換 session 就看不到。改為需求進度表，並要求非單純需求在實作前展開成細項。

## 後果

- 新增共用必讀文件時，要同步更新 `AGENTS.md` 的清單與 `CLAUDE.md` 的匯入，檢查腳本會驗證。
- 範本管理的檔案（`engineering-workflow.md`、`TEMPLATE_VERSION`、`ai-handoff` skill、`scripts/ai/check-ai-structure.ps1`、`sync-skills.ps1`、`tools/ste-lint.mjs`、各 `_template`）由 `New-AiProject.ps1 -Refresh` 更新，不要在專案裡改；其餘檔案由專案擁有。
- Codex 端的行為依官方文件設計，沒有端到端實測，除非專案另外記錄。
